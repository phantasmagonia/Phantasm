#include <phantasm/iso/scene.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>

using phantasm::iso::GridPos;
using phantasm::iso::Scene;
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
