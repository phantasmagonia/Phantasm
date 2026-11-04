#pragma once

#include <phantasm/entity_store.hpp>
#include <phantasm/iso/tile_map.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace phantasm::iso {

struct SceneObject {
    std::uint64_t id = 0; // persistent across saves; runtime Entity handles are not
    GridPos position;
    std::string name;
};

// Editor-facing scene data. The store owns objects; IDs map persistent names to runtime handles.
class Scene {
public:
    explicit Scene(TileMap map);
    [[nodiscard]] static Scene parse(std::string_view text);
    [[nodiscard]] static Scene load_file(const std::filesystem::path& path);
    [[nodiscard]] std::string serialize() const;
    void save_file(const std::filesystem::path& path) const;

    [[nodiscard]] TileMap& map() noexcept { return map_; }
    [[nodiscard]] const TileMap& map() const noexcept { return map_; }
    [[nodiscard]] std::uint64_t create(GridPos position, std::string name);
    [[nodiscard]] bool erase(std::uint64_t id);
    [[nodiscard]] SceneObject* object(std::uint64_t id) noexcept;
    [[nodiscard]] const SceneObject* object(std::uint64_t id) const noexcept;
    [[nodiscard]] Entity runtime_entity(std::uint64_t id) const noexcept;
    [[nodiscard]] const EntityStore& entities() const noexcept { return *store_; }

private:
    void insert(std::uint64_t id, GridPos position, std::string name);

    TileMap map_;
    std::unique_ptr<EntityStore> store_ = std::make_unique<EntityStore>();
    std::map<std::uint64_t, Entity> ids_;
    std::uint64_t next_id_ = 1;
};

} // namespace phantasm::iso
