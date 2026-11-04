#pragma once

#include <phantasm/editor/editor_state.hpp>
#include <phantasm/entity_store.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace phantasm::editor {

struct EditorOptions {
    std::optional<std::filesystem::path> scene{}; // opened at startup if set
    int width = 1280;
    int height = 800;
    bool hidden = false; // for tests and automation
};

// The desktop isometric editor: window, Dear ImGui UI, and viewport around an EditorState.
// Games extend it with panels and component inspectors; callbacks draw with ImGui
// (link phantasm_editor_lib and include <imgui.h>).
class Editor {
public:
    // Panel contents; the editor opens and closes the window around the callback.
    using PanelFn = std::function<void(EditorState&)>;
    // Drawn below the built-in inspector while an entity is selected.
    using InspectorFn = std::function<void(EntityStore&, Entity)>;

    explicit Editor(const EditorOptions& options = {});
    ~Editor();

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;
    Editor(Editor&&) = delete;
    Editor& operator=(Editor&&) = delete;

    void add_panel(std::string title, PanelFn draw);
    void add_inspector(std::string title, InspectorFn draw);

    // Runs one editor frame; returns false once the user closes the window.
    bool frame();
    // Calls frame() until it returns false.
    void run();

    [[nodiscard]] EditorState& state() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace phantasm::editor
