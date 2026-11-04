#pragma once

#include <phantasm/geometry.hpp>
#include <phantasm/iso/iso_grid.hpp>
#include <phantasm/iso/scene.hpp>
#include <phantasm/renderer.hpp>
#include <phantasm/texture.hpp>

#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace phantasm::editor::detail {

// Loads scene object images lazily, near the view, a couple per frame. Relative paths
// resolve against the scene file's directory; failures are remembered until clear().
class AssetCache {
public:
    explicit AssetCache(Renderer& renderer) : renderer_{renderer} {}

    void set_scene_file(const std::filesystem::path& file);
    void clear();
    [[nodiscard]] const Texture* get(const std::string& asset) const noexcept;
    void prepare(const iso::Scene& scene, const iso::IsoGrid& grid, int level, Rect view);
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
    void load(const std::string& asset);

    Renderer& renderer_;
    std::filesystem::path directory_ = std::filesystem::current_path();
    std::map<std::string, Texture> loaded_;
    std::set<std::string> failed_;
    std::string error_;
};

} // namespace phantasm::editor::detail
