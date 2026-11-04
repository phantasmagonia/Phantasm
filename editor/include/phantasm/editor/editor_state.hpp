#pragma once

#include <phantasm/iso/scene.hpp>
#include <phantasm/iso/scene_history.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace phantasm::editor {

enum class Tool { floor, wall, erase, place, select };

// Editing logic without any window or UI: tools, selection, inspector edit tracking,
// undo/redo, and files. UIs (the desktop editor, a future touch editor) drive it.
class EditorState {
public:
    explicit EditorState(iso::Scene scene = iso::Scene{iso::TileMap::blank(32, 32, 2)});

    EditorState(const EditorState&) = delete;
    EditorState& operator=(const EditorState&) = delete;
    EditorState(EditorState&&) = delete; // history_ refers to scene_
    EditorState& operator=(EditorState&&) = delete;

    [[nodiscard]] iso::Scene& scene() noexcept { return scene_; }
    [[nodiscard]] const iso::Scene& scene() const noexcept { return scene_; }

    [[nodiscard]] Tool tool() const noexcept { return tool_; }
    void set_tool(Tool tool);
    [[nodiscard]] int level() const noexcept { return level_; }
    void set_level(int level) noexcept; // clamped to the map's levels
    [[nodiscard]] const std::string& placement_asset() const noexcept { return placement_asset_; }
    void set_placement_asset(std::string asset) { placement_asset_ = std::move(asset); }

    // Pointer held over `tile` (only x and y are used; the level is level()).
    // `pressed` is true on the frame the pointer went down. Painting continues while held;
    // placing and selecting happen only on press. Tiles outside the map are ignored.
    void pointer_down(iso::TileCoord tile, bool pressed);
    // Ends the current paint stroke (one undo step).
    void pointer_up();

    [[nodiscard]] std::uint64_t selected() const noexcept { return selected_; }
    void select(std::uint64_t id); // finishes a pending inspector edit first
    [[nodiscard]] iso::SceneObject* selected_object();
    bool delete_selected();

    // Inspector edits: call begin with the object's state before a widget changes it, and
    // finish when the widget is released. Repeated begins keep the first snapshot.
    void begin_object_edit(const iso::SceneObject& before);
    void finish_object_edit();

    bool undo();
    bool redo();
    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;

    // Throw on failure; a failed load keeps the current scene.
    void save(const std::filesystem::path& path);
    void load(const std::filesystem::path& path);

private:
    [[nodiscard]] bool on_map(iso::TileCoord tile) const noexcept;
    [[nodiscard]] std::uint64_t object_at(iso::TileCoord tile) const;

    iso::Scene scene_;
    iso::SceneHistory history_{scene_};
    Tool tool_ = Tool::floor;
    int level_ = 0;
    std::string placement_asset_;
    std::uint64_t selected_ = 0;
    std::optional<iso::SceneObject> pending_before_;
};

} // namespace phantasm::editor
