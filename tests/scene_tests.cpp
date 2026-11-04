#include <phantasm/iso/scene.hpp>
#include <phantasm/iso/scene_history.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>

using phantasm::iso::GridPos;
using phantasm::iso::Scene;
using phantasm::iso::SceneHistory;
using phantasm::iso::TileCoord;
using phantasm::iso::TileMap;

TEST_CASE("Scene round trip keeps object IDs and the next ID after deletion") {
    Scene scene{TileMap::blank(4, 3, 2)};
    scene.map().set(TileCoord{2, 1, 1}, '#');
    const auto removed = scene.create(GridPos{0.5F, 1.0F, 0}, "removed");
    const auto kept = scene.create(GridPos{2.25F, 1.5F, 1}, "door \"A\"");
    const auto removed_highest = scene.create(GridPos{0.0F, 0.0F, 0}, "later removed");
    REQUIRE(scene.erase(removed));
    REQUIRE(scene.erase(removed_highest));
    REQUIRE(scene.object(kept) != nullptr);
    scene.object(kept)->position.x = 2.5F;

    Scene loaded = Scene::parse(scene.serialize());
    CHECK(loaded.map().at(TileCoord{2, 1, 1}) == '#');
    CHECK(loaded.object(removed) == nullptr);
    REQUIRE(loaded.object(kept) != nullptr);
    CHECK(loaded.object(kept)->position.x == 2.5F);
    CHECK(loaded.object(kept)->name == "door \"A\"");
    CHECK(loaded.create(GridPos{1.0F, 2.0F, 0}, "new") > removed_highest);
}

TEST_CASE("Scene IDs survive a file round trip") {
    const auto path = std::filesystem::temp_directory_path() / "phantasm_scene_test.scene";
    Scene scene{TileMap::blank(2, 2, 1)};
    const auto id = scene.create(GridPos{1.0F, 1.0F, 0}, "hero");
    scene.save_file(path);
    const Scene loaded = Scene::load_file(path);
    CHECK(loaded.object(id) != nullptr);
    std::filesystem::remove(path);
}

TEST_CASE("Scene saves asset references and loads version 1 scenes") {
    Scene scene{TileMap::blank(2, 2, 1)};
    const auto id = scene.create(GridPos{1.0F, 1.0F, 0}, "tree", "art/tree house \"north\".png", 1.5F);
    const Scene loaded = Scene::parse(scene.serialize());
    REQUIRE(loaded.object(id) != nullptr);
    CHECK(loaded.object(id)->asset == "art/tree house \"north\".png");
    CHECK(loaded.object(id)->scale == 1.5F);

    const std::string old = "phantasm-scene 1\nmap-bytes 10\nlevel 0\n#\n"
                            "entities 1 next-id 2\n1 0 0 0 \"old\"\n";
    const Scene legacy = Scene::parse(old);
    REQUIRE(legacy.object(1) != nullptr);
    CHECK(legacy.object(1)->asset.empty());
    CHECK(legacy.object(1)->scale == 1.0F);
    CHECK_THROWS_AS(scene.create(GridPos{0.0F, 0.0F, 0}, "bad", "image.png", 0.0F),
                    std::invalid_argument);
}

TEST_CASE("Undo and redo group a tile stroke and preserve the next stable ID") {
    Scene scene{TileMap::blank(3, 3, 1)};
    SceneHistory history{scene};
    history.paint(TileCoord{0, 0, 0}, '.');
    history.paint(TileCoord{1, 0, 0}, '#');
    history.paint(TileCoord{0, 0, 0}, '#');
    history.end_stroke();
    CHECK(history.can_undo());
    CHECK(history.undo());
    CHECK(scene.map().at(TileCoord{0, 0, 0}) == TileMap::kEmpty);
    CHECK(scene.map().at(TileCoord{1, 0, 0}) == TileMap::kEmpty);
    CHECK_FALSE(history.undo());
    CHECK(history.redo());
    CHECK(scene.map().at(TileCoord{0, 0, 0}) == '#');

    const auto id = history.create(GridPos{1.0F, 1.0F, 0}, "tree", "tree.png");
    const auto old_runtime = scene.runtime_entity(id);
    CHECK(history.undo());
    CHECK(scene.object(id) == nullptr);
    CHECK(history.redo());
    REQUIRE(scene.object(id) != nullptr);
    CHECK_FALSE(scene.entities().alive(old_runtime));
    CHECK(scene.object(id)->asset == "tree.png");
    CHECK(history.create(GridPos{0.0F, 0.0F, 0}, "next") > id);
}

TEST_CASE("Undo covers inspector edits and deletion; new edits discard redo") {
    Scene scene{TileMap::blank(2, 2, 1)};
    SceneHistory history{scene};
    const auto id = history.create(GridPos{0.0F, 0.0F, 0}, "before");
    const auto before = *scene.object(id);
    scene.object(id)->name = "after";
    scene.object(id)->asset = "hero.png";
    history.record_object_change(before);
    CHECK(history.undo());
    CHECK(scene.object(id)->name == "before");
    CHECK(scene.object(id)->asset.empty());
    CHECK(history.redo());
    CHECK(scene.object(id)->asset == "hero.png");
    CHECK(history.erase(id));
    CHECK(scene.object(id) == nullptr);
    CHECK(history.undo());
    CHECK(scene.object(id)->asset == "hero.png");
    CHECK(history.undo());
    history.paint(TileCoord{0, 0, 0}, '.');
    history.end_stroke();
    CHECK_FALSE(history.can_redo());
    history.clear();
    CHECK_FALSE(history.can_undo());
    CHECK_FALSE(history.can_redo());
}

TEST_CASE("Scene rejects duplicate IDs, bad versions and truncated data") {
    Scene scene{TileMap::blank(2, 2, 1)};
    (void)scene.create(GridPos{0.0F, 0.0F, 0}, "a");
    (void)scene.create(GridPos{1.0F, 1.0F, 0}, "b");
    std::string valid = scene.serialize();
    std::string duplicate = valid;
    const auto second = duplicate.find("\n2 ");
    REQUIRE(second != std::string::npos);
    duplicate[second + 1] = '1';
    CHECK_THROWS_AS(Scene::parse(duplicate), std::runtime_error);
    valid.replace(0, std::string{"phantasm-scene 1"}.size(), "phantasm-scene 9");
    CHECK_THROWS_AS(Scene::parse(valid), std::runtime_error);
    CHECK_THROWS_AS(Scene::parse("phantasm-scene 1\nmap-bytes 20\nshort"), std::runtime_error);
}
