#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace phantasm {

struct Entity {
    std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t generation = 0;
    std::uint32_t store = 0;
    friend bool operator==(Entity, Entity) = default;
};

// Packed columns per component combination. Structural changes move an entity
// between archetypes and may invalidate component pointers and iteration spans.
class EntityStore {
    struct ColumnBase {
        virtual ~ColumnBase() = default;
        virtual void reserve(std::size_t count) = 0;
        virtual std::size_t capacity() const noexcept = 0;
        virtual std::size_t storage_bytes() const noexcept = 0;
        virtual void pop_back() noexcept = 0;
        virtual void erase_swap(std::size_t row) noexcept = 0;
        virtual void move_to(ColumnBase& target, std::size_t row) noexcept = 0;
    };

    template <typename T>
    struct Column final : ColumnBase {
        std::vector<T> values;

        void reserve(std::size_t count) override { values.reserve(count); }
        std::size_t capacity() const noexcept override { return values.capacity(); }
        std::size_t storage_bytes() const noexcept override {
            return values.capacity() * sizeof(T);
        }
        void pop_back() noexcept override { values.pop_back(); }
        void erase_swap(std::size_t row) noexcept override {
            if (row + 1 != values.size()) {
                values[row] = std::move(values.back());
            }
            values.pop_back();
        }
        void move_to(ColumnBase& target, std::size_t row) noexcept override {
            static_cast<Column<T>&>(target).values.push_back(std::move(values[row]));
        }
    };

    using Signature = std::vector<std::type_index>;
    struct Archetype {
        Signature signature;
        std::vector<Entity> entities;
        std::map<std::type_index, std::unique_ptr<ColumnBase>> columns;
    };
    struct Slot {
        Archetype* archetype = nullptr;
        std::uint32_t row = 0;
        std::uint32_t generation = 1;
    };
    using Factory = std::unique_ptr<ColumnBase> (*)();

public:
    EntityStore() : id_{next_store_id_.fetch_add(1, std::memory_order_relaxed)} {}
    EntityStore(const EntityStore&) = delete;
    EntityStore& operator=(const EntityStore&) = delete;
    EntityStore(EntityStore&&) = delete;
    EntityStore& operator=(EntityStore&&) = delete;

    template <typename... Components>
    Entity create(Components&&... components) {
        check_structure();
        (register_type<std::remove_cvref_t<Components>>(), ...);
        Signature signature{std::type_index(typeid(std::remove_cvref_t<Components>))...};
        normalize(signature);
        if (signature.size() != sizeof...(Components)) {
            throw std::invalid_argument("Duplicate component type");
        }
        Archetype& target = archetype_for(signature);
        if (free_.empty() && slots_.size() == std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("Entity index limit reached");
        }
        if (free_.empty()) {
            if (slots_.size() == slots_.capacity()) {
                slots_.reserve(std::max<std::size_t>(1, slots_.capacity() * 2));
            }
        }
        reserve_row(target);
        const auto index = free_.empty() ? static_cast<std::uint32_t>(slots_.size()) : free_.back();
        const auto generation = free_.empty() ? 1U : slots_[index].generation;
        const Entity entity{index, generation, id_};
        (append_component(target, std::forward<Components>(components)), ...);
        target.entities.push_back(entity);
        if (free_.empty()) {
            slots_.push_back({&target, static_cast<std::uint32_t>(target.entities.size() - 1),
                              generation});
        } else {
            free_.pop_back();
            slots_[index] = {&target, static_cast<std::uint32_t>(target.entities.size() - 1),
                             generation};
        }
        ++size_;
        return entity;
    }

    void destroy(Entity entity) {
        check_structure();
        Slot* slot = slot_for(entity);
        if (slot == nullptr) {
            return;
        }
        if (free_.size() == free_.capacity()) {
            free_.reserve(std::max<std::size_t>(1, free_.capacity() * 2));
        }
        Archetype& archetype = *slot->archetype;
        erase_row(archetype, slot->row);
        slot->archetype = nullptr;
        if (slot->generation == std::numeric_limits<std::uint32_t>::max()) {
            slot->generation = 0; // retire the slot instead of resurrecting an old handle
        } else {
            ++slot->generation;
            free_.push_back(entity.index);
        }
        --size_;
    }

    [[nodiscard]] bool alive(Entity entity) const noexcept { return slot_for(entity) != nullptr; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    // Array capacities only; excludes allocator and map-node overhead.
    [[nodiscard]] std::size_t storage_bytes() const noexcept {
        std::size_t bytes = slots_.capacity() * sizeof(Slot) +
                            free_.capacity() * sizeof(std::uint32_t);
        for (const auto& [key, archetype] : archetypes_) {
            (void)key;
            bytes += archetype->entities.capacity() * sizeof(Entity);
            for (const auto& [type, column] : archetype->columns) {
                (void)type;
                bytes += column->storage_bytes();
            }
        }
        return bytes;
    }

    template <typename T>
    [[nodiscard]] T* get(Entity entity) noexcept {
        Slot* slot = slot_for(entity);
        if (slot == nullptr) {
            return nullptr;
        }
        auto it = slot->archetype->columns.find(std::type_index(typeid(T)));
        return it == slot->archetype->columns.end() ? nullptr
            : &static_cast<Column<T>&>(*it->second).values[slot->row];
    }

    template <typename T>
    [[nodiscard]] const T* get(Entity entity) const noexcept {
        return const_cast<EntityStore*>(this)->get<T>(entity);
    }

    template <typename T>
    [[nodiscard]] bool has(Entity entity) const noexcept { return get<T>(entity) != nullptr; }

    template <typename T>
    bool add(Entity entity, T value) {
        check_structure();
        Slot* slot = slot_for(entity);
        if (slot == nullptr) {
            return false;
        }
        if (T* existing = get<T>(entity)) {
            *existing = std::move(value);
            return true;
        }
        register_type<T>();
        Signature signature = slot->archetype->signature;
        signature.emplace_back(typeid(T));
        normalize(signature);
        migrate(entity, archetype_for(signature), [&value](Archetype& target) noexcept {
            static_cast<Column<T>&>(*target.columns.at(std::type_index(typeid(T))))
                .values.push_back(std::move(value));
        });
        return true;
    }

    template <typename T>
    bool remove(Entity entity) {
        check_structure();
        Slot* slot = slot_for(entity);
        if (slot == nullptr || !has<T>(entity)) {
            return false;
        }
        Signature signature = slot->archetype->signature;
        signature.erase(std::find(signature.begin(), signature.end(),
                                  std::type_index(typeid(T))));
        migrate(entity, archetype_for(signature), [](Archetype&) {});
        return true;
    }

    template <typename... Components, typename Callback>
    void each_chunk(Callback&& callback) {
        Iteration guard{*this};
        for (auto& [key, archetype] : archetypes_) {
            if (((archetype->columns.contains(std::type_index(typeid(Components)))) && ...)) {
                std::invoke(callback, std::span<const Entity>{archetype->entities},
                            std::span<Components>{static_cast<Column<Components>&>(
                                *archetype->columns.at(std::type_index(typeid(Components)))).values}...);
            }
        }
    }

    template <typename... Components, typename Callback>
    void each_chunk(Callback&& callback) const {
        Iteration guard{*this};
        for (const auto& [key, archetype] : archetypes_) {
            (void)key;
            if (((archetype->columns.contains(std::type_index(typeid(Components)))) && ...)) {
                std::invoke(callback, std::span<const Entity>{archetype->entities},
                            std::span<const Components>{static_cast<const Column<Components>&>(
                                *archetype->columns.at(std::type_index(typeid(Components)))).values}...);
            }
        }
    }

    template <typename... Components, typename Callback>
    void each(Callback&& callback) {
        each_chunk<Components...>([&](std::span<const Entity> entities,
                                      std::span<Components>... columns) {
            for (std::size_t row = 0; row < entities.size(); ++row) {
                if constexpr (std::is_invocable_v<Callback&, Entity, Components&...>) {
                    std::invoke(callback, entities[row], columns[row]...);
                } else {
                    std::invoke(callback, columns[row]...);
                }
            }
        });
    }

    template <typename... Components, typename Callback>
    void each(Callback&& callback) const {
        each_chunk<Components...>([&](std::span<const Entity> entities,
                                      std::span<const Components>... columns) {
            for (std::size_t row = 0; row < entities.size(); ++row) {
                if constexpr (std::is_invocable_v<Callback&, Entity, const Components&...>) {
                    std::invoke(callback, entities[row], columns[row]...);
                } else {
                    std::invoke(callback, columns[row]...);
                }
            }
        });
    }

private:
    struct Iteration {
        const EntityStore& store;
        explicit Iteration(const EntityStore& owner) : store{owner} { ++store.iterations_; }
        ~Iteration() { --store.iterations_; }
    };

    template <typename T>
    void register_type() {
        static_assert(std::is_nothrow_move_constructible_v<T> &&
                      std::is_nothrow_move_assignable_v<T>,
                      "Components must be movable without throwing during archetype changes");
        registry_.try_emplace(std::type_index(typeid(T)),
                              +[]() -> std::unique_ptr<ColumnBase> {
                                  return std::make_unique<Column<T>>();
                              });
    }

    static void normalize(Signature& signature) {
        std::sort(signature.begin(), signature.end());
        signature.erase(std::unique(signature.begin(), signature.end()), signature.end());
    }

    Archetype& archetype_for(const Signature& signature) {
        auto it = archetypes_.find(signature);
        if (it != archetypes_.end()) {
            return *it->second;
        }
        auto archetype = std::make_unique<Archetype>();
        archetype->signature = signature;
        for (const auto type : signature) {
            archetype->columns.emplace(type, registry_.at(type)());
        }
        auto [inserted, success] = archetypes_.emplace(signature, std::move(archetype));
        (void)success;
        return *inserted->second;
    }

    static void reserve_row(Archetype& archetype) {
        const std::size_t size = archetype.entities.size() + 1;
        if (archetype.entities.capacity() < size) {
            archetype.entities.reserve(std::max(size, archetype.entities.capacity() * 2));
        }
        for (auto& [type, column] : archetype.columns) {
            (void)type;
            if (column->capacity() < size) {
                column->reserve(std::max(size, column->capacity() * 2));
            }
        }
    }

    template <typename T>
    static void append_component(Archetype& archetype, T&& value) noexcept {
        static_cast<Column<std::remove_cvref_t<T>>&>(
            *archetype.columns.at(std::type_index(typeid(std::remove_cvref_t<T>))))
            .values.push_back(std::forward<T>(value));
    }

    void erase_row(Archetype& archetype, std::size_t row) noexcept {
        const std::size_t last = archetype.entities.size() - 1;
        if (row != last) {
            slots_[archetype.entities[last].index].row = row;
            archetype.entities[row] = archetype.entities[last];
        }
        archetype.entities.pop_back();
        for (auto& [type, column] : archetype.columns) {
            (void)type;
            column->erase_swap(row);
        }
    }

    template <typename Extra>
    void migrate(Entity entity, Archetype& target, Extra&& extra) {
        Slot& slot = slots_[entity.index];
        Archetype& source = *slot.archetype;
        reserve_row(target);
        for (auto& [type, column] : source.columns) {
            auto it = target.columns.find(type);
            if (it != target.columns.end()) {
                column->move_to(*it->second, slot.row);
            }
        }
        extra(target);
        const std::size_t row = target.entities.size();
        target.entities.push_back(entity);
        erase_row(source, slot.row);
        slot.archetype = &target;
        slot.row = static_cast<std::uint32_t>(row);
    }

    [[nodiscard]] Slot* slot_for(Entity entity) noexcept {
        if (entity.store != id_ || entity.index >= slots_.size()) {
            return nullptr;
        }
        Slot& slot = slots_[entity.index];
        return slot.archetype != nullptr && slot.generation == entity.generation ? &slot : nullptr;
    }

    [[nodiscard]] const Slot* slot_for(Entity entity) const noexcept {
        return const_cast<EntityStore*>(this)->slot_for(entity);
    }

    void check_structure() const {
        if (iterations_ != 0) {
            throw std::logic_error("Structural changes during entity iteration are not allowed");
        }
    }

    inline static std::atomic<std::uint32_t> next_store_id_{1};
    const std::uint32_t id_;
    std::map<std::type_index, Factory> registry_;
    std::map<Signature, std::unique_ptr<Archetype>> archetypes_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_;
    std::size_t size_ = 0;
    mutable std::size_t iterations_ = 0;
};

// Frequently toggled booleans stay outside archetype signatures, so changing
// them does not move packed components. The store must outlive this sidecar.
class EntityFlags {
public:
    explicit EntityFlags(const EntityStore& store) : store_{store} {}

    bool set(Entity entity, std::uint64_t mask, bool enabled) {
        if (!store_.alive(entity)) {
            return false;
        }
        if (entries_.size() <= entity.index) {
            entries_.resize(static_cast<std::size_t>(entity.index) + 1);
        }
        Entry& entry = entries_[entity.index];
        if (entry.generation != entity.generation) {
            entry = {entity.generation, 0};
        }
        entry.bits = enabled ? entry.bits | mask : entry.bits & ~mask;
        return true;
    }

    [[nodiscard]] bool test(Entity entity, std::uint64_t mask) const noexcept {
        return store_.alive(entity) && entity.index < entries_.size() &&
               entries_[entity.index].generation == entity.generation &&
               (entries_[entity.index].bits & mask) == mask;
    }

private:
    struct Entry {
        std::uint32_t generation = 0;
        std::uint64_t bits = 0;
    };
    const EntityStore& store_;
    std::vector<Entry> entries_;
};

} // namespace phantasm
