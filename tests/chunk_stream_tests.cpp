#include <phantasm/chunk_stream.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <memory>
#include <vector>

using phantasm::ChunkCoord;
using phantasm::ChunkStream;
using phantasm::Rect;

TEST_CASE("Chunk streaming loads visible chunks once and unloads departed chunks") {
    ChunkStream<int> stream{32.0F};
    int calls = 0;
    auto load = [&](ChunkCoord chunk) { return ++calls + static_cast<int>(chunk.x); };

    CHECK(stream.update(Rect{0, 0, 64, 32}, 10, load) == 2);
    CHECK(stream.size() == 2);
    CHECK(stream.find({0, 0}) != nullptr);
    CHECK(stream.update(Rect{0, 0, 64, 32}, 10, load) == 0);
    CHECK(calls == 2);

    CHECK(stream.update(Rect{32, 0, 64, 32}, 10, load) == 1);
    CHECK(stream.size() == 2);
    CHECK(stream.find({0, 0}) == nullptr);
    CHECK(stream.find({2, 0}) != nullptr);
}

TEST_CASE("Chunk streaming handles negative coordinates and prefetch margins") {
    ChunkStream<int> stream{32.0F, 1};
    stream.update(Rect{-8, -8, 8, 8}, 20, [](ChunkCoord) { return 1; });

    CHECK(stream.size() == 9);
    CHECK(stream.find({-2, -2}) != nullptr);
    CHECK(stream.find({0, 0}) != nullptr);
    CHECK(stream.find({1, 0}) == nullptr);
}

TEST_CASE("Chunk streaming limits loads per update and retries failed loads") {
    ChunkStream<int> stream{32.0F};
    int attempts = 0;
    auto load = [&](ChunkCoord) -> int {
        if (++attempts == 1) {
            throw std::runtime_error("temporary load failure");
        }
        return attempts;
    };

    CHECK_THROWS_AS(stream.update(Rect{0, 0, 64, 32}, 1, load), std::runtime_error);
    CHECK(stream.size() == 0);
    CHECK(stream.update(Rect{0, 0, 64, 32}, 1, load) == 1);
    CHECK(stream.size() == 1);
    CHECK(stream.update(Rect{0, 0, 64, 32}, 1, load) == 1);
    CHECK(stream.size() == 2);
}

TEST_CASE("Chunk streaming loads the closest chunk first and owns move-only data") {
    ChunkStream<std::unique_ptr<int>> stream{32.0F};
    std::vector<ChunkCoord> order;
    stream.update(Rect{0, 0, 96, 32}, 1, [&](ChunkCoord coord) {
        order.push_back(coord);
        return std::make_unique<int>(static_cast<int>(coord.x));
    });

    REQUIRE(order.size() == 1);
    CHECK(order.front() == (ChunkCoord{1, 0}));
    REQUIRE(stream.find({1, 0}) != nullptr);
    CHECK(**stream.find({1, 0}) == 1);
}

TEST_CASE("Chunk streaming validates settings and preserves data for empty views") {
    CHECK_THROWS_AS((ChunkStream<int>{0.0F}), std::invalid_argument);
    CHECK_THROWS_AS((ChunkStream<int>{32.0F, -1}), std::invalid_argument);
    ChunkStream<int> stream{32.0F};
    stream.update(Rect{0, 0, 32, 32}, 1, [](ChunkCoord) { return 7; });
    CHECK(stream.update(Rect{0, 0, 0, 0}, 1, [](ChunkCoord) { return 8; }) == 0);
    CHECK(*stream.find({0, 0}) == 7);
    CHECK_THROWS_AS(stream.update(Rect{0, 0, 1.0e30F, 32}, 1,
                                  [](ChunkCoord) { return 0; }), std::invalid_argument);
}
