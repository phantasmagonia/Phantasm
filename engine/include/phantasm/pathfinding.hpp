#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace phantasm {

struct Cell {
    int x = 0;
    int y = 0;
    friend bool operator==(Cell, Cell) = default;
};

// Movement cost per cell: 0 = blocked, 1 = normal, higher = slower terrain.
class NavGrid {
public:
    NavGrid(int width, int height, std::uint8_t cost = 1);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] bool contains(Cell cell) const noexcept;
    void set_cost(Cell cell, std::uint8_t cost); // ignored outside the grid
    [[nodiscard]] std::uint8_t cost(Cell cell) const noexcept; // 0 outside the grid
    [[nodiscard]] bool passable(Cell cell) const noexcept { return cost(cell) != 0; }

private:
    int width_;
    int height_;
    std::vector<std::uint8_t> costs_;
};

struct PathOptions {
    // 8-way movement; a diagonal step never cuts between two blocked orthogonal cells.
    bool diagonal = true;
};

// Cheapest path from start to goal, inclusive of both; empty if there is none.
[[nodiscard]] std::vector<Cell> find_path(const NavGrid& grid, Cell start, Cell goal,
                                          PathOptions options = {});

// Distance-to-goal and best next step for every cell, for many agents sharing goals
// (e.g. monsters chasing players). Same movement rules as find_path.
class FlowField {
public:
    [[nodiscard]] static FlowField build(const NavGrid& grid, std::span<const Cell> goals,
                                         PathOptions options = {});

    // Cost of the cheapest route to the nearest goal; infinity if unreachable or outside.
    [[nodiscard]] float distance(Cell cell) const noexcept;
    [[nodiscard]] bool reachable(Cell cell) const noexcept;
    // The neighbor one step closer to a goal; nullopt at a goal or when unreachable.
    [[nodiscard]] std::optional<Cell> next(Cell cell) const noexcept;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<float> distance_;
    std::vector<int> next_; // index of the next cell, -1 for none
};

} // namespace phantasm
