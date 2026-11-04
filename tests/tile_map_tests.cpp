#include <phantasm/iso/tile_map.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>

using phantasm::iso::TileCoord;
using phantasm::iso::TileMap;

TEST_CASE("A single level map is parsed row by row") {
    const TileMap map = TileMap::parse("level 0\n"
                                       "#.#\n"
                                       "...\n");

    CHECK(map.width() == 3);
    CHECK(map.height() == 2);
    CHECK(map.level_count() == 1);
    CHECK(map.at(TileCoord{0, 0, 0}) == '#');
    CHECK(map.at(TileCoord{1, 0, 0}) == '.');
    CHECK(map.at(TileCoord{2, 1, 0}) == '.');
}

TEST_CASE("Multiple levels share the same width and height") {
    const TileMap map = TileMap::parse("level 0\n"
                                       "##\n"
                                       "##\n"
                                       "level 1\n"
                                       ".\n");

    CHECK(map.level_count() == 2);
    CHECK(map.width() == 2);
    CHECK(map.height() == 2);
    CHECK(map.at(TileCoord{0, 0, 1}) == '.');
}

TEST_CASE("Short rows and missing rows are padded with empty tiles") {
    const TileMap map = TileMap::parse("level 0\n"
                                       "###\n"
                                       "#\n"
                                       "level 1\n"
                                       "..\n"
                                       "..\n"
                                       "..\n");

    CHECK(map.at(TileCoord{2, 1, 0}) == TileMap::kEmpty);
    CHECK(map.at(TileCoord{0, 2, 0}) == TileMap::kEmpty);
    CHECK(map.height() == 3);
}

TEST_CASE("Out-of-bounds lookups return empty instead of crashing") {
    const TileMap map = TileMap::parse("level 0\n#\n");

    CHECK(map.at(TileCoord{-1, 0, 0}) == TileMap::kEmpty);
    CHECK(map.at(TileCoord{0, 5, 0}) == TileMap::kEmpty);
    CHECK(map.at(TileCoord{0, 0, 3}) == TileMap::kEmpty);
}

TEST_CASE("Windows line endings are accepted") {
    const TileMap map = TileMap::parse("level 0\r\n#.\r\n");

    CHECK(map.width() == 2);
    CHECK(map.at(TileCoord{1, 0, 0}) == '.');
}

TEST_CASE("Rows before any level header are rejected with the line number") {
    CHECK_THROWS_WITH(TileMap::parse("#.#\n"), Catch::Matchers::ContainsSubstring("line 1"));
}

TEST_CASE("Levels must be numbered 0, 1, 2 in order") {
    CHECK_THROWS_WITH(TileMap::parse("level 0\n#\nlevel 2\n#\n"),
                      Catch::Matchers::ContainsSubstring("line 3"));
}

TEST_CASE("A map without tiles is rejected") {
    CHECK_THROWS_AS(TileMap::parse(""), std::runtime_error);
    CHECK_THROWS_AS(TileMap::parse("level 0\n"), std::runtime_error);
}

TEST_CASE("find returns every tile with a given character") {
    const TileMap map = TileMap::parse("level 0\n"
                                       "S.S\n"
                                       "level 1\n"
                                       "..S\n");

    const auto found = map.find('S');

    REQUIRE(found.size() == 3);
    CHECK(found[0].x == 0);
    CHECK(found[2].level == 1);
}

TEST_CASE("A blank map can be painted and serialized across levels") {
    TileMap map = TileMap::blank(3, 2, 2);
    map.set(TileCoord{1, 0, 1}, '#');
    const TileMap loaded = TileMap::parse(map.serialize());
    CHECK(loaded.width() == 3);
    CHECK(loaded.height() == 2);
    CHECK(loaded.level_count() == 2);
    CHECK(loaded.at(TileCoord{1, 0, 1}) == '#');
    CHECK(loaded.at(TileCoord{1, 0, 0}) == TileMap::kEmpty);
    CHECK_THROWS_AS(map.set(TileCoord{3, 0, 0}, '#'), std::out_of_range);
    CHECK_THROWS_AS(map.set(TileCoord{0, 0, 0}, '\n'), std::invalid_argument);
    CHECK_THROWS_AS(TileMap::blank(0, 2, 1), std::invalid_argument);
}
