#include "asset_cache.hpp"

#include <exception>

namespace phantasm::editor::detail {

namespace {
constexpr std::size_t kLoadsPerFrame = 2;
constexpr float kPreloadMargin = 1024.0F; // world units around the view
} // namespace

void AssetCache::set_scene_file(const std::filesystem::path& file) {
    const auto directory = std::filesystem::absolute(file).parent_path();
    if (directory == directory_) {
        return;
    }
    clear();
    directory_ = directory;
}

void AssetCache::clear() {
    renderer_.wait_idle(); // textures may still be in use by submitted frames
    loaded_.clear();
    failed_.clear();
    error_.clear();
}

const Texture* AssetCache::get(const std::string& asset) const noexcept {
    const auto it = loaded_.find(asset);
    return it == loaded_.end() ? nullptr : &it->second;
}

void AssetCache::prepare(const iso::Scene& scene, const iso::IsoGrid& grid, int level, Rect view) {
    std::size_t loaded = 0;
    scene.entities().each<iso::SceneObject>([&](const iso::SceneObject& object) {
        if (loaded >= kLoadsPerFrame || object.position.level != level || object.asset.empty() ||
            loaded_.contains(object.asset) || failed_.contains(object.asset)) {
            return;
        }
        const auto point = grid.grid_to_world(object.position);
        if (point.x < view.x - kPreloadMargin || point.x > view.x + view.w + kPreloadMargin ||
            point.y < view.y - kPreloadMargin || point.y > view.y + view.h + kPreloadMargin) {
            return;
        }
        load(object.asset);
        ++loaded;
    });
}

void AssetCache::load(const std::string& asset) {
    const std::filesystem::path path{asset};
    const std::string key = (path.is_absolute() ? path : directory_ / path).lexically_normal().string();
    try {
        loaded_.emplace(asset, renderer_.load_texture(key));
        error_.clear();
    } catch (const std::exception& error) {
        failed_.insert(asset);
        error_ = "Could not load " + key + ": " + error.what();
    }
}

} // namespace phantasm::editor::detail
