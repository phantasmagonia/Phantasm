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

using phantasm::FlowField;

TEST_CASE("A flow field's goal has distance zero and no next step") {
    const NavGrid grid{5, 5};
    const std::vector<Cell> goals{{2, 2}};

    const auto field = FlowField::build(grid, goals);

    CHECK(field.reachable({2, 2}));
    CHECK(field.distance({2, 2}) == 0.0F);
    CHECK_FALSE(field.next({2, 2}).has_value());
}

TEST_CASE("Every next step moves strictly closer to the goal") {
    NavGrid grid{8, 8};
    for (int y = 0; y < 6; ++y) {
        grid.set_cost({4, y}, 0);
    }
    const std::vector<Cell> goals{{7, 0}};
    const auto field = FlowField::build(grid, goals);

    int checked = 0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const Cell cell{x, y};
            if (!field.reachable(cell) || cell == goals[0]) {
                continue;
            }
            const auto step = field.next(cell);
            REQUIRE(step.has_value());
            CHECK(field.distance(*step) < field.distance(cell));
            CHECK(std::abs(step->x - x) <= 1);
            CHECK(std::abs(step->y - y) <= 1);
            ++checked;
        }
    }
    // Every open cell can reach the goal around the wall's gap (8x8 minus 6 wall cells).
    CHECK(checked == 64 - 6 - 1);
}

TEST_CASE("Cells that cannot reach any goal have no next step") {
    NavGrid grid{5, 5};
    for (int y = 0; y < 5; ++y) {
        grid.set_cost({2, y}, 0); // a full wall splits the grid
    }
    const std::vector<Cell> goals{{4, 2}};

    const auto field = FlowField::build(grid, goals);

    CHECK_FALSE(field.reachable({0, 2}));
    CHECK_FALSE(field.next({0, 2}).has_value());
    CHECK_FALSE(field.reachable({2, 2})); // the wall itself
    CHECK_FALSE(field.reachable({9, 9})); // outside the grid
}

TEST_CASE("With several goals each cell heads for the nearest one") {
    const NavGrid grid{11, 1};
    const std::vector<Cell> goals{{0, 0}, {10, 0}};

    const auto field = FlowField::build(grid, goals);

    CHECK(field.distance({3, 0}) == 3.0F);
    CHECK(field.next({3, 0}) == Cell{2, 0});
    CHECK(field.distance({8, 0}) == 2.0F);
    CHECK(field.next({8, 0}) == Cell{9, 0});
}

TEST_CASE("Following a flow field reaches a goal at the cost A* finds, on random terrain") {
    std::mt19937 rng{7};
    std::uniform_int_distribution<int> coord{0, 15};
    std::uniform_int_distribution<int> terrain{0, 9};
    for (int trial = 0; trial < 50; ++trial) {
        NavGrid grid{16, 16};
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                const int roll = terrain(rng);
                grid.set_cost({x, y}, roll < 2 ? 0 : roll < 7 ? 1 : static_cast<std::uint8_t>(roll - 4));
            }
        }
        const Cell goal{coord(rng), coord(rng)};
        grid.set_cost(goal, 1);
        const std::vector<Cell> goals{goal};
        const auto field = FlowField::build(grid, goals);

        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                const Cell start{x, y};
                const auto path = find_path(grid, start, goal);
                CHECK(field.reachable(start) == !path.empty());
                if (path.empty()) {
                    continue;
                }
                CHECK(field.distance(start) == Catch::Approx(path_cost(grid, path)).margin(1e-3));
                // Walk the field: it must arrive, and its route costs the same.
                std::vector<Cell> walked{start};
                while (walked.size() <= 256 && walked.back() != goal) {
                    const auto step = field.next(walked.back());
                    REQUIRE(step.has_value());
                    walked.push_back(*step);
                }
                CHECK(walked.back() == goal);
                CHECK(path_cost(grid, walked) == Catch::Approx(path_cost(grid, path)).margin(1e-3));
            }
        }
    }
}
