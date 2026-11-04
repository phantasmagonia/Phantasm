#include <phantasm/editor/editor_state.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>

using phantasm::editor::EditorState;
using phantasm::editor::Tool;
using phantasm::iso::GridPos;
using phantasm::iso::Scene;
using phantasm::iso::TileCoord;
using phantasm::iso::TileMap;

namespace {
EditorState make_state() {
    return EditorState{Scene{TileMap::blank(8, 8, 2)}};
}
} // namespace

TEST_CASE("A paint drag is one undo step") {
    EditorState state = make_state();
    state.set_tool(Tool::wall);

    state.pointer_down(TileCoord{1, 1, 0}, true);
    state.pointer_down(TileCoord{2, 1, 0}, false);
    state.pointer_up();

    CHECK(state.scene().map().at(TileCoord{1, 1, 0}) == '#');
    CHECK(state.scene().map().at(TileCoord{2, 1, 0}) == '#');
    CHECK(state.undo());
    CHECK(state.scene().map().at(TileCoord{1, 1, 0}) == TileMap::kEmpty);
    CHECK(state.scene().map().at(TileCoord{2, 1, 0}) == TileMap::kEmpty);
    CHECK_FALSE(state.can_undo());
}

TEST_CASE("Floor and erase tools paint and clear tiles on the current level") {
    EditorState state = make_state();
    state.set_level(1);
    state.set_tool(Tool::floor);
    state.pointer_down(TileCoord{3, 3, 0}, true); // level comes from the state, not the tile
    state.pointer_up();
    CHECK(state.scene().map().at(TileCoord{3, 3, 1}) == '.');
    CHECK(state.scene().map().at(TileCoord{3, 3, 0}) == TileMap::kEmpty);

    state.set_tool(Tool::erase);
    state.pointer_down(TileCoord{3, 3, 1}, true);
    state.pointer_up();
    CHECK(state.scene().map().at(TileCoord{3, 3, 1}) == TileMap::kEmpty);
}

TEST_CASE("Pointers outside the map are ignored") {
    EditorState state = make_state();
    state.set_tool(Tool::wall);

    state.pointer_down(TileCoord{-1, 0, 0}, true);
    state.pointer_down(TileCoord{8, 8, 0}, false);
    state.pointer_up();

    CHECK_FALSE(state.can_undo());
}

TEST_CASE("Levels are clamped to the map") {
    EditorState state = make_state();
    state.set_level(5);
    CHECK(state.level() == 1);
    state.set_level(-3);
    CHECK(state.level() == 0);
}

TEST_CASE("Placing creates a selected entity at the tile center with the placement image") {
    EditorState state = make_state();
    state.set_tool(Tool::place);
    state.set_placement_asset("art/crate.png");

    state.pointer_down(TileCoord{4, 2, 0}, true);
    state.pointer_down(TileCoord{4, 2, 0}, false); // holding does not place again

    REQUIRE(state.selected_object() != nullptr);
    CHECK(state.scene().entities().size() == 1);
    CHECK(state.selected_object()->position.x == 4.5F);
    CHECK(state.selected_object()->position.y == 2.5F);
    CHECK(state.selected_object()->asset == "art/crate.png");
}

TEST_CASE("The select tool picks the entity on the clicked tile, or clears selection") {
    EditorState state = make_state();
    const auto id = state.scene().create(GridPos{2.5F, 2.5F, 0}, "barrel");
    state.set_tool(Tool::select);

    state.pointer_down(TileCoord{2, 2, 0}, true);
    CHECK(state.selected() == id);

    state.pointer_down(TileCoord{6, 6, 0}, true);
    CHECK(state.selected() == 0);
}

TEST_CASE("Inspector edits become one undo step when finished") {
    EditorState state = make_state();
    const auto id = state.scene().create(GridPos{1.0F, 1.0F, 0}, "before");
    state.select(id);

    state.begin_object_edit(*state.selected_object());
    state.selected_object()->name = "midway";
    state.begin_object_edit(*state.selected_object()); // already editing: keeps the first snapshot
    state.selected_object()->name = "after";
    state.finish_object_edit();

    CHECK(state.undo());
    CHECK(state.scene().object(id)->name == "before");
}

TEST_CASE("Changing selection finishes a pending inspector edit") {
    EditorState state = make_state();
    const auto a = state.scene().create(GridPos{1.0F, 1.0F, 0}, "a");
    const auto b = state.scene().create(GridPos{2.0F, 2.0F, 0}, "b");
    state.select(a);
    state.begin_object_edit(*state.selected_object());
    state.selected_object()->name = "renamed";

    state.select(b);

    CHECK(state.undo());
    CHECK(state.scene().object(a)->name == "a");
}

TEST_CASE("Deleting the selection is undoable and clears selection") {
    EditorState state = make_state();
    const auto id = state.scene().create(GridPos{1.0F, 1.0F, 0}, "doomed");
    state.select(id);

    CHECK(state.delete_selected());

    CHECK(state.selected() == 0);
    CHECK(state.scene().object(id) == nullptr);
    CHECK(state.undo());
    CHECK(state.scene().object(id) != nullptr);
}

TEST_CASE("Saving and loading round-trips the scene and resets editor state") {
    const auto path = std::filesystem::temp_directory_path() / "phantasm_editor_state_test.phscene";
    EditorState state = make_state();
    state.set_tool(Tool::wall);
    state.pointer_down(TileCoord{0, 0, 0}, true);
    state.pointer_up();
    state.save(path);

    EditorState other = make_state();
    other.set_level(1);
    other.load(path);

    CHECK(other.scene().map().at(TileCoord{0, 0, 0}) == '#');
    CHECK(other.level() == 0);
    CHECK_FALSE(other.can_undo());
    std::filesystem::remove(path);
}

TEST_CASE("Loading a missing file throws and keeps the current scene") {
    EditorState state = make_state();
    state.set_tool(Tool::wall);
    state.pointer_down(TileCoord{1, 0, 0}, true);
    state.pointer_up();

    CHECK_THROWS(state.load("/definitely/not/here.phscene"));
    CHECK(state.scene().map().at(TileCoord{1, 0, 0}) == '#');
}
