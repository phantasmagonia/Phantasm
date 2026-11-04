#include <phantasm/entity_store.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using phantasm::Entity;
using phantasm::EntityStore;
using phantasm::EntityFlags;

namespace {

struct Position {
    float x = 0.0F;
    float y = 0.0F;
};

struct Velocity {
    float x = 0.0F;
    float y = 0.0F;
};

struct Name {
    std::string value;
};

// Counts live instances so tests can prove every component is destroyed exactly once.
struct Tracked {
    static inline int live = 0;
    std::unique_ptr<int> payload;
    explicit Tracked(int value) : payload{std::make_unique<int>(value)} { ++live; }
    Tracked(Tracked&& other) noexcept : payload{std::move(other.payload)} { ++live; }
    Tracked& operator=(Tracked&& other) noexcept {
        payload = std::move(other.payload);
        return *this;
    }
    Tracked(const Tracked&) = delete;
    Tracked& operator=(const Tracked&) = delete;
    ~Tracked() { --live; }
};

} // namespace

TEST_CASE("Created entities are alive and hold their components") {
    EntityStore store;
    const Entity e = store.create(Position{1, 2}, Velocity{3, 4});

    CHECK(store.alive(e));
    CHECK(store.size() == 1);
    REQUIRE(store.get<Position>(e) != nullptr);
    CHECK(store.get<Position>(e)->x == 1.0F);
    CHECK(store.get<Velocity>(e)->y == 4.0F);
    CHECK(store.has<Position>(e));
    CHECK_FALSE(store.has<Name>(e));
    CHECK(store.get<Name>(e) == nullptr);
}

TEST_CASE("Destroyed entities are dead and ignored safely") {
    EntityStore store;
    const Entity e = store.create(Position{});

    store.destroy(e);

    CHECK_FALSE(store.alive(e));
    CHECK(store.size() == 0);
    CHECK(store.get<Position>(e) == nullptr);
    CHECK_FALSE(store.add(e, Velocity{}));
    CHECK_FALSE(store.remove<Position>(e));
    store.destroy(e); // double destroy is a no-op
}

TEST_CASE("A stale handle is detected after its slot is reused") {
    EntityStore store;
    const Entity old_entity = store.create(Position{1, 1});
    store.destroy(old_entity);

    const Entity new_entity = store.create(Position{2, 2});

    CHECK(new_entity.index == old_entity.index);
    CHECK(new_entity != old_entity);
    CHECK_FALSE(store.alive(old_entity));
    CHECK(store.get<Position>(old_entity) == nullptr);
    CHECK(store.get<Position>(new_entity)->x == 2.0F);
}

TEST_CASE("Handles from another store never identify local entities") {
    EntityStore a;
    EntityStore b;
    const Entity from_a = a.create(Position{1, 0});
    const Entity from_b = b.create(Position{2, 0});

    CHECK(from_a.index == from_b.index);
    CHECK(from_a.generation == from_b.generation);
    CHECK_FALSE(a.alive(from_b));
    CHECK_FALSE(b.alive(from_a));
    CHECK(a.get<Position>(from_b) == nullptr);
}

TEST_CASE("Sidecar flags toggle without moving components and reject reused handles") {
    EntityStore store;
    EntityFlags flags{store};
    const Entity first = store.create(Position{1, 0});
    Position* before = store.get<Position>(first);
    REQUIRE(flags.set(first, 1, true));
    CHECK(flags.test(first, 1));
    CHECK(flags.set(first, 1, false));
    CHECK_FALSE(flags.test(first, 1));
    CHECK(store.get<Position>(first) == before);
    REQUIRE(flags.set(first, 2, true));
    store.destroy(first);
    const Entity replacement = store.create(Position{2, 0});
    CHECK(replacement.index == first.index);
    CHECK_FALSE(flags.test(first, 2));
    CHECK_FALSE(flags.test(replacement, 2));
    CHECK_FALSE(flags.set(first, 2, true));
    CHECK(flags.set(replacement, 2, true));
    CHECK(flags.test(replacement, 2));
}

TEST_CASE("Adding a component keeps existing values; adding again overwrites") {
    EntityStore store;
    const Entity e = store.create(Position{5, 6});

    CHECK(store.add(e, Velocity{1, 0}));
    CHECK(store.get<Position>(e)->y == 6.0F);
    CHECK(store.get<Velocity>(e)->x == 1.0F);

    CHECK(store.add(e, Velocity{9, 0}));
    CHECK(store.get<Velocity>(e)->x == 9.0F);
}

TEST_CASE("Removing a component keeps the others") {
    EntityStore store;
    const Entity e = store.create(Position{5, 6}, Velocity{1, 2});

    CHECK(store.remove<Velocity>(e));

    CHECK_FALSE(store.has<Velocity>(e));
    CHECK(store.get<Position>(e)->x == 5.0F);
    CHECK_FALSE(store.remove<Velocity>(e));
}

TEST_CASE("Destroying an entity does not disturb the others") {
    EntityStore store;
    const Entity a = store.create(Position{1, 0});
    const Entity b = store.create(Position{2, 0});
    const Entity c = store.create(Position{3, 0});

    store.destroy(a);

    CHECK(store.get<Position>(b)->x == 2.0F);
    CHECK(store.get<Position>(c)->x == 3.0F);
}

TEST_CASE("each visits exactly the entities with all requested components") {
    EntityStore store;
    store.create(Position{1, 0}, Velocity{1, 0});
    store.create(Position{2, 0}, Velocity{1, 0}, Name{"walker"});
    store.create(Position{3, 0});

    int visited = 0;
    store.each<Position, Velocity>([&](Position& p, Velocity& v) {
        p.x += v.x;
        ++visited;
    });

    CHECK(visited == 2);
    float total = 0.0F;
    store.each<Position>([&](const Position& p) { total += p.x; });
    CHECK(total == 2.0F + 3.0F + 3.0F);
}

TEST_CASE("each can receive the entity handle") {
    EntityStore store;
    const Entity a = store.create(Position{1, 0});
    const Entity b = store.create(Position{2, 0}, Velocity{});

    std::map<float, Entity> seen;
    store.each<Position>([&](Entity e, const Position& p) { seen[p.x] = e; });

    CHECK(seen.at(1.0F) == a);
    CHECK(seen.at(2.0F) == b);
}

TEST_CASE("A const store can enumerate entities for saving") {
    EntityStore store;
    const Entity entity = store.create(Position{3, 4});
    const EntityStore& read_only = store;
    std::vector<Entity> seen;
    read_only.each<Position>([&](Entity e, const Position& position) {
        CHECK(position.x == 3);
        seen.push_back(e);
    });
    CHECK(seen == std::vector<Entity>{entity});
    CHECK_THROWS_AS(read_only.each<Position>([&](const Position&) {
                        store.destroy(entity);
                    }), std::logic_error);
    CHECK(store.alive(entity));
}

TEST_CASE("each_chunk exposes contiguous component spans") {
    EntityStore store;
    for (int i = 0; i < 10; ++i) {
        store.create(Position{static_cast<float>(i), 0}, Velocity{1, 0});
    }

    std::size_t total = 0;
    store.each_chunk<Position, Velocity>(
        [&](std::span<const Entity> entities, std::span<Position> positions,
            std::span<Velocity> velocities) {
            REQUIRE(entities.size() == positions.size());
            REQUIRE(positions.size() == velocities.size());
            for (std::size_t i = 0; i < positions.size(); ++i) {
                positions[i].x += velocities[i].x;
            }
            total += positions.size();
        });

    CHECK(total == 10);
}

TEST_CASE("Structural changes during iteration are rejected") {
    EntityStore store;
    const Entity e = store.create(Position{});

    CHECK_THROWS_AS(store.each<Position>([&](Position&) { store.create(Position{}); }),
                    std::logic_error);
    CHECK_THROWS_AS(store.each<Position>([&](Position&) { store.destroy(e); }), std::logic_error);
    CHECK_THROWS_AS(store.each<Position>([&](Position&) { store.add(e, Velocity{}); }),
                    std::logic_error);
    // After a rejected change the store is usable again.
    CHECK(store.alive(e));
    CHECK(store.add(e, Velocity{}));
}

TEST_CASE("Non-trivial components survive moves and are destroyed exactly once") {
    Tracked::live = 0;
    {
        EntityStore store;
        std::vector<Entity> entities;
        for (int i = 0; i < 100; ++i) {
            entities.push_back(store.create(Tracked{i}, Name{"entity " + std::to_string(i)}));
        }
        for (int i = 0; i < 100; i += 3) {
            store.add(entities[static_cast<std::size_t>(i)], Position{});
        }
        for (int i = 0; i < 100; i += 5) {
            store.destroy(entities[static_cast<std::size_t>(i)]);
        }
        for (int i = 1; i < 100; i += 5) {
            store.remove<Name>(entities[static_cast<std::size_t>(i)]);
        }

        for (int i = 0; i < 100; ++i) {
            const Entity e = entities[static_cast<std::size_t>(i)];
            if (i % 5 == 0) {
                CHECK_FALSE(store.alive(e));
                continue;
            }
            REQUIRE(store.get<Tracked>(e) != nullptr);
            CHECK(*store.get<Tracked>(e)->payload == i);
            if (i % 5 == 1) {
                CHECK_FALSE(store.has<Name>(e));
            } else {
                CHECK(store.get<Name>(e)->value == "entity " + std::to_string(i));
            }
        }
        CHECK(Tracked::live == 80);
    }
    CHECK(Tracked::live == 0);
}

TEST_CASE("Random create, destroy, add, and remove stay consistent with a reference model") {
    EntityStore store;
    std::map<std::uint32_t, std::pair<Entity, float>> model; // index -> (handle, position x)
    std::map<std::uint32_t, bool> has_velocity;
    std::mt19937 rng{1};

    for (int step = 0; step < 20'000; ++step) {
        const int action = static_cast<int>(rng() % 4);
        if (action == 0 || model.empty()) {
            const auto x = static_cast<float>(step);
            const Entity e = store.create(Position{x, 0});
            model[e.index] = {e, x};
            has_velocity[e.index] = false;
            continue;
        }
        auto it = model.begin();
        std::advance(it, static_cast<long>(rng() % model.size()));
        const Entity e = it->second.first;
        if (action == 1) {
            store.destroy(e);
            has_velocity.erase(e.index);
            model.erase(it);
        } else if (action == 2) {
            store.add(e, Velocity{1, 1});
            has_velocity[e.index] = true;
        } else {
            store.remove<Velocity>(e);
            has_velocity[e.index] = false;
        }
    }

    CHECK(store.size() == model.size());
    for (const auto& [index, entry] : model) {
        REQUIRE(store.alive(entry.first));
        CHECK(store.get<Position>(entry.first)->x == entry.second);
        CHECK(store.has<Velocity>(entry.first) == has_velocity.at(index));
    }
}
