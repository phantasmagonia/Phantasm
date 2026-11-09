#include <phantasm/iso/visibility.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace phantasm::iso;

TEST_CASE("An outdoor viewer sees every floor and roof") {
    FloorVisibility visibility;
    visibility.set_role('R', TileRole::Roof);
    const FloorVisibility::Focus focus{{2.0F, 2.0F, 0}, false};
    CHECK(visibility.opacity({2, 2, 1}, 'R', focus) == 1.0F);
    CHECK(visibility.opacity({2, 2, 0}, 'R', focus) == 1.0F);
}

TEST_CASE("An indoor viewer hides roofs and all floors above the current floor") {
    FloorVisibility visibility;
    visibility.set_role('R', TileRole::Roof);
    const FloorVisibility::Focus focus{{2.0F, 2.0F, 1}, true};
    CHECK(visibility.opacity({2, 2, 2}, '.', focus) == 0.0F);
    CHECK(visibility.opacity({2, 2, 1}, 'R', focus) == 0.0F);
    CHECK(visibility.opacity({2, 2, 0}, 'R', focus) == 1.0F);
    CHECK(visibility.opacity({2, 2, 1}, '.', focus) == 1.0F);
}

TEST_CASE("Nearby foreground walls fade without fading floors or distant walls") {
    FloorVisibility visibility;
    visibility.set_role('#', TileRole::Wall);
    const FloorVisibility::Focus focus{{2.0F, 2.0F, 0}, true};
    CHECK(visibility.opacity({3, 2, 0}, '#', focus) == 0.35F);
    CHECK(visibility.opacity({3, 2, 0}, '.', focus) == 1.0F);
    CHECK(visibility.opacity({1, 2, 0}, '#', focus) == 1.0F);
    CHECK(visibility.opacity({4, 2, 0}, '#', focus) == 1.0F);
    CHECK(visibility.opacity({3, 2, 0}, '#', focus, 1.5F, 0.5F) == 0.5F);
}

TEST_CASE("A wall diagonally in front of a viewer standing at a tile center fades") {
    FloorVisibility visibility;
    visibility.set_role('#', TileRole::Wall);
    // Viewer at the center of tile (2, 2); tile (3, 2) is one step toward the camera.
    const FloorVisibility::Focus focus{{2.5F, 2.5F, 0}, true};

    CHECK(visibility.opacity({3, 2, 0}, '#', focus) == 0.35F);
    CHECK(visibility.opacity({2, 3, 0}, '#', focus) == 0.35F);
    CHECK(visibility.opacity({1, 2, 0}, '#', focus) == 1.0F); // behind the viewer
}

TEST_CASE("Walls in front of an outdoor viewer fade so the viewer stays visible") {
    FloorVisibility visibility;
    visibility.set_role('#', TileRole::Wall);
    visibility.set_role('R', TileRole::Roof);
    const FloorVisibility::Focus outdoors{{2.5F, 2.5F, 0}, false};

    CHECK(visibility.opacity({3, 2, 0}, '#', outdoors) == 0.35F); // wall hiding the viewer
    CHECK(visibility.opacity({3, 2, 1}, 'R', outdoors) == 1.0F);  // roofs stay outdoors
    CHECK(visibility.opacity({1, 2, 0}, '#', outdoors) == 1.0F);  // wall behind the viewer
}
