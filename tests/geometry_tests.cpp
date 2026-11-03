#include <phantasm/geometry.hpp>

#include <catch2/catch_test_macros.hpp>

using phantasm::intersects;
using phantasm::Rect;

TEST_CASE("Overlapping rects intersect") {
    CHECK(intersects(Rect{0, 0, 10, 10}, Rect{5, 5, 10, 10}));
    CHECK(intersects(Rect{0, 0, 10, 10}, Rect{2, 2, 2, 2}));
}

TEST_CASE("Separated rects do not intersect") {
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{20, 0, 5, 5}));
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{0, -20, 5, 5}));
}

TEST_CASE("Rects that only touch at an edge do not intersect") {
    CHECK_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{10, 0, 5, 5}));
}
