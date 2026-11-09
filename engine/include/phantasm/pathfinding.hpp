#pragma once

#include <cstdint>
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

} // namespace phantasm
