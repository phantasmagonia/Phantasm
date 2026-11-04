#include <phantasm/editor/editor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <exception>
#include <memory>

using phantasm::editor::Editor;
using phantasm::editor::EditorState;
using phantasm::iso::GridPos;

namespace {

struct Health {
    int value = 100;
};

std::unique_ptr<Editor> make_editor() {
    try {
        return std::make_unique<Editor>(phantasm::editor::EditorOptions{.hidden = true});
    } catch (const std::exception&) {
        return nullptr; // no display or GPU (e.g. headless CI)
    }
}

} // namespace

TEST_CASE("Game panels are drawn every frame with the editor state") {
    auto editor = make_editor();
    if (!editor) {
        SKIP("No display or GPU available");
    }
    int calls = 0;
    const EditorState* seen = nullptr;
    editor->add_panel("Stats", [&](EditorState& state) {
        ++calls;
        seen = &state;
    });

    REQUIRE(editor->frame());
    REQUIRE(editor->frame());

    CHECK(calls == 2);
    CHECK(seen == &editor->state());
}

TEST_CASE("Game inspectors run for the selected entity and can edit its components") {
    auto editor = make_editor();
    if (!editor) {
        SKIP("No display or GPU available");
    }
    auto& scene = editor->state().scene();
    const auto id = scene.create(GridPos{1.0F, 1.0F, 0}, "guard");
    const auto entity = scene.runtime_entity(id);
    REQUIRE(scene.entities().add(entity, Health{}));

    int calls = 0;
    editor->add_inspector("Health", [&](phantasm::EntityStore& store, phantasm::Entity selected) {
        ++calls;
        if (auto* health = store.get<Health>(selected)) {
            health->value = 42;
        }
    });

    REQUIRE(editor->frame());
    CHECK(calls == 0); // nothing selected yet

    editor->state().select(id);
    REQUIRE(editor->frame());
    CHECK(calls == 1);
    CHECK(scene.entities().get<Health>(entity)->value == 42);
}
