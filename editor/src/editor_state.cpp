#include <phantasm/editor/editor_state.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace phantasm::editor {

namespace {

char tile_for(Tool tool) noexcept {
    switch (tool) {
    case Tool::floor: return '.';
    case Tool::wall: return '#';
    default: return iso::TileMap::kEmpty;
    }
}

} // namespace

EditorState::EditorState(iso::Scene scene) : scene_{std::move(scene)} {}

void EditorState::set_tool(Tool tool) {
    history_.end_stroke();
    tool_ = tool;
}

void EditorState::set_level(int level) noexcept {
    level_ = std::clamp(level, 0, std::max(0, scene_.map().level_count() - 1));
}

bool EditorState::on_map(iso::TileCoord tile) const noexcept {
    return tile.x >= 0 && tile.y >= 0 && tile.x < scene_.map().width() &&
           tile.y < scene_.map().height();
}

std::uint64_t EditorState::object_at(iso::TileCoord tile) const {
    std::uint64_t picked = 0;
    scene_.entities().each<iso::SceneObject>([&](const iso::SceneObject& object) {
        if (object.position.level == level_ &&
            std::floor(object.position.x) == static_cast<float>(tile.x) &&
            std::floor(object.position.y) == static_cast<float>(tile.y)) {
            picked = object.id;
        }
    });
    return picked;
}

void EditorState::pointer_down(iso::TileCoord tile, bool pressed) {
    tile.level = level_;
    if (!on_map(tile)) {
        return;
    }
    switch (tool_) {
    case Tool::floor:
    case Tool::wall:
    case Tool::erase: history_.paint(tile, tile_for(tool_)); break;
    case Tool::place:
        if (pressed) {
            finish_object_edit();
            selected_ = history_.create(iso::GridPos{static_cast<float>(tile.x) + 0.5F,
                                                     static_cast<float>(tile.y) + 0.5F, level_},
                                        "Entity", placement_asset_);
        }
        break;
    case Tool::select:
        if (pressed) {
            select(object_at(tile));
        }
        break;
    }
}

void EditorState::pointer_up() {
    history_.end_stroke();
}

void EditorState::select(std::uint64_t id) {
    if (selected_ == id) {
        return;
    }
    finish_object_edit();
    selected_ = id;
}

iso::SceneObject* EditorState::selected_object() {
    return scene_.object(selected_);
}

bool EditorState::delete_selected() {
    finish_object_edit();
    if (!history_.erase(selected_)) {
        return false;
    }
    selected_ = 0;
    return true;
}

void EditorState::begin_object_edit(const iso::SceneObject& before) {
    if (!pending_before_) {
        pending_before_ = before;
    }
}

void EditorState::finish_object_edit() {
    if (!pending_before_) {
        return;
    }
    if (scene_.object(pending_before_->id) != nullptr) {
        history_.record_object_change(*pending_before_);
    }
    pending_before_.reset();
}

bool EditorState::undo() {
    finish_object_edit();
    return history_.undo();
}

bool EditorState::redo() {
    finish_object_edit();
    return history_.redo();
}

bool EditorState::can_undo() const noexcept {
    return history_.can_undo() || pending_before_.has_value();
}

bool EditorState::can_redo() const noexcept {
    return history_.can_redo();
}

void EditorState::save(const std::filesystem::path& path) {
    finish_object_edit();
    history_.end_stroke();
    scene_.save_file(path);
}

void EditorState::load(const std::filesystem::path& path) {
    iso::Scene loaded = iso::Scene::load_file(path); // throws before anything changes
    finish_object_edit();
    history_.end_stroke();
    scene_ = std::move(loaded);
    history_.clear();
    selected_ = 0;
    level_ = 0;
}

} // namespace phantasm::editor
