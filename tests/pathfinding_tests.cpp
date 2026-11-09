#include <phantasm/pathfinding.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <random>
#include <vector>

using phantasm::Cell;
using phantasm::NavGrid;
using phantasm::PathOptions;
using phantasm::find_path;

namespace {

// Each step moves to an 8-way neighbor.
bool steps_are_adjacent(const std::vector<Cell>& path) {
    for (std::size_t i = 1; i < path.size(); ++i) {
        const int dx = std::abs(path[i].x - path[i - 1].x);
        const int dy = std::abs(path[i].y - path[i - 1].y);
        if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) {
            return false;
        }
    }
    return true;
}

bool avoids_blocked(const NavGrid& grid, const std::vector<Cell>& path) {
    for (const Cell cell : path) {
        if (!grid.passable(cell)) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("A path from a cell to itself is just that cell") {
    const NavGrid grid{5, 5};
    CHECK(find_path(grid, {2, 2}, {2, 2}) == std::vector<Cell>{{2, 2}});
}

TEST_CASE("In the open, a path is a straight line of adjacent cells") {
    const NavGrid grid{10, 3};

    const auto path = find_path(grid, {0, 1}, {9, 1});

    REQUIRE(path.size() == 10);
    CHECK(path.front() == Cell{0, 1});
    CHECK(path.back() == Cell{9, 1});
    CHECK(steps_are_adjacent(path));
}

TEST_CASE("Diagonal movement reaches a diagonal goal in the fewest steps") {
    const NavGrid grid{6, 6};

    CHECK(find_path(grid, {0, 0}, {5, 5}).size() == 6);
    CHECK(find_path(grid, {0, 0}, {5, 5}, PathOptions{.diagonal = false}).size() == 11);
}

TEST_CASE("Paths go around walls") {
    NavGrid grid{7, 7};
    for (int y = 0; y < 6; ++y) {
        grid.set_cost({3, y}, 0); // wall with a gap at the bottom row
    }

    const auto path = find_path(grid, {0, 0}, {6, 0});

    REQUIRE_FALSE(path.empty());
    CHECK(avoids_blocked(grid, path));
    CHECK(steps_are_adjacent(path));
    CHECK(path.back() == Cell{6, 0});
}

TEST_CASE("No path exists to a blocked, enclosed, or outside goal") {
    NavGrid grid{7, 7};
    grid.set_cost({6, 6}, 0);
    for (const Cell wall : {Cell{2, 1}, Cell{1, 2}, Cell{3, 2}, Cell{2, 3}, Cell{1, 1}, Cell{3, 1},
                            Cell{1, 3}, Cell{3, 3}}) {
        grid.set_cost(wall, 0); // (2, 2) fully enclosed
    }

    CHECK(find_path(grid, {0, 0}, {6, 6}).empty());
    CHECK(find_path(grid, {0, 0}, {2, 2}).empty());
    CHECK(find_path(grid, {0, 0}, {9, 9}).empty());
    CHECK(find_path(grid, {-1, 0}, {0, 0}).empty());
}

TEST_CASE("Diagonal steps never cut between two blocked corners") {
    NavGrid grid{3, 3};
    grid.set_cost({1, 0}, 0);
    grid.set_cost({0, 1}, 0);

    // (0,0) -> (1,1) would squeeze between the two walls; there is no other way out.
    CHECK(find_path(grid, {0, 0}, {2, 2}).empty());
}

TEST_CASE("Paths prefer cheap terrain when the detour costs less") {
    NavGrid grid{5, 3};
    for (int x = 1; x < 4; ++x) {
        grid.set_cost({x, 1}, 9); // swamp along the straight line
    }

    const auto path = find_path(grid, {0, 1}, {4, 1});

    REQUIRE_FALSE(path.empty());
    for (const Cell cell : path) {
        CHECK(grid.cost(cell) == 1); // goes around through the dry rows
    }
}

namespace {

float step_cost(const NavGrid& grid, Cell from, Cell to) {
    const bool diagonal = from.x != to.x && from.y != to.y;
    return (diagonal ? 1.41421356F : 1.0F) * static_cast<float>(grid.cost(to));
}

float path_cost(const NavGrid& grid, const std::vector<Cell>& path) {
    float total = 0.0F;
    for (std::size_t i = 1; i < path.size(); ++i) {
        total += step_cost(grid, path[i - 1], path[i]);
    }
    return total;
}

// Independent reference: plain Dijkstra over all cells, same movement rules.
float reference_cost(const NavGrid& grid, Cell start, Cell goal) {
    const int w = grid.width();
    std::vector<float> best(static_cast<std::size_t>(w * grid.height()), 1e30F);
    std::vector<bool> done(best.size(), false);
    best[static_cast<std::size_t>(start.y * w + start.x)] = 0.0F;
    for (;;) {
        int current = -1;
        for (std::size_t i = 0; i < best.size(); ++i) {
            if (!done[i] && best[i] < 1e29F && (current < 0 || best[i] < best[static_cast<std::size_t>(current)])) {
                current = static_cast<int>(i);
            }
        }
        if (current < 0) {
            return -1.0F; // unreachable
        }
        done[static_cast<std::size_t>(current)] = true;
        const Cell here{current % w, current / w};
        if (here == goal) {
            return best[static_cast<std::size_t>(current)];
        }
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const Cell next{here.x + dx, here.y + dy};
                if ((dx == 0 && dy == 0) || !grid.passable(next)) {
                    continue;
                }
                if (dx != 0 && dy != 0 &&
                    (!grid.passable({here.x + dx, here.y}) || !grid.passable({here.x, here.y + dy}))) {
                    continue;
                }
                auto& slot = best[static_cast<std::size_t>(next.y * w + next.x)];
                slot = std::min(slot, best[static_cast<std::size_t>(current)] + step_cost(grid, here, next));
            }
        }
    }
}

} // namespace

TEST_CASE("Paths are as cheap as an exhaustive search finds, on random terrain") {
    std::mt19937 rng{2026};
    std::uniform_int_distribution<int> coord{0, 15};
    std::uniform_int_distribution<int> terrain{0, 9};
    for (int trial = 0; trial < 200; ++trial) {
        NavGrid grid{16, 16};
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                const int roll = terrain(rng);
                grid.set_cost({x, y}, roll < 2 ? 0 : roll < 7 ? 1 : static_cast<std::uint8_t>(roll - 4));
            }
        }
        const Cell start{coord(rng), coord(rng)};
        const Cell goal{coord(rng), coord(rng)};
        grid.set_cost(start, 1);
        grid.set_cost(goal, 1);

        const auto path = find_path(grid, start, goal);
        const float expected = reference_cost(grid, start, goal);

        if (expected < 0.0F) {
            CHECK(path.empty());
        } else {
            REQUIRE_FALSE(path.empty());
            CHECK(steps_are_adjacent(path));
            CHECK(avoids_blocked(grid, path));
            CHECK(path_cost(grid, path) == Catch::Approx(expected).margin(1e-3));
        }
    }
}
