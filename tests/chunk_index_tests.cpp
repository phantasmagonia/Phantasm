#include <phantasm/chunk_index.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

using phantasm::ChunkIndex;
using phantasm::Rect;

TEST_CASE("Chunk queries include only intersecting objects") {
    ChunkIndex index{32.0F};
    index.upsert(1, Rect{0, 0, 8, 8});
    index.upsert(2, Rect{33, 0, 8, 8});
    index.upsert(3, Rect{-8, -8, 8, 8});

    CHECK(index.query(Rect{0, 0, 32, 32}) == std::vector<std::uint32_t>{1});
    CHECK(index.query(Rect{-8, -8, 16, 16}) ==
          std::vector<std::uint32_t>{1, 3});
}

TEST_CASE("Objects spanning chunks appear once and are tested against the view") {
    ChunkIndex index{32.0F};
    index.upsert(7, Rect{30, 0, 10, 10});

    CHECK(index.query(Rect{31, 0, 4, 4}) == std::vector<std::uint32_t>{7});
    CHECK(index.query(Rect{40, 0, 4, 4}).empty());
}

TEST_CASE("Moving and removing objects update chunk membership") {
    ChunkIndex index{32.0F};
    index.upsert(7, Rect{0, 0, 8, 8});
    index.upsert(7, Rect{64, 0, 8, 8});

    CHECK(index.query(Rect{0, 0, 32, 32}).empty());
    CHECK(index.query(Rect{64, 0, 32, 32}) == std::vector<std::uint32_t>{7});
    CHECK(index.erase(7));
    CHECK_FALSE(index.erase(7));
    CHECK(index.query(Rect{64, 0, 32, 32}).empty());
}

TEST_CASE("Updating bounds within one chunk refreshes query geometry") {
    ChunkIndex index{64.0F};
    index.upsert(4, Rect{0, 0, 8, 8});
    index.upsert(4, Rect{40, 0, 8, 8});

    CHECK(index.query(Rect{0, 0, 16, 16}).empty());
    CHECK(index.query(Rect{40, 0, 8, 8}) == std::vector<std::uint32_t>{4});
}

TEST_CASE("Chunk index rejects invalid geometry") {
    CHECK_THROWS_AS(ChunkIndex{0.0F}, std::invalid_argument);
    ChunkIndex index{32.0F};
    CHECK_THROWS_AS(index.upsert(1, Rect{0, 0, 0, 5}), std::invalid_argument);
    CHECK(index.query(Rect{0, 0, 0, 5}).empty());
}

TEST_CASE("Chunk index rejects unsupported coordinates without losing objects") {
    ChunkIndex index{32.0F};
    index.upsert(1, Rect{0, 0, 8, 8});
    const Rect distant{1.0e30F, 1.0e30F, 8, 8};

    CHECK_THROWS_AS(index.upsert(1, distant), std::invalid_argument);
    CHECK(index.size() == 1);
    CHECK(index.query(Rect{0, 0, 32, 32}) == std::vector<std::uint32_t>{1});
    CHECK_THROWS_AS(index.query(distant), std::invalid_argument);
}

TEST_CASE("Chunk index bounds the chunks visited by one operation") {
    ChunkIndex index{1.0F};
    index.upsert(1, Rect{0, 0, 1, 1});
    const Rect oversized{-1000, -1000, 2000, 2000};

    CHECK_THROWS_AS(index.upsert(1, oversized), std::length_error);
    CHECK(index.query(Rect{0, 0, 1, 1}) == std::vector<std::uint32_t>{1});
    CHECK_THROWS_AS(index.query(oversized), std::length_error);
}
