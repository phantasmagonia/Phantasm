#include <phantasm/pathfinding.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <limits>
#include <queue>

namespace phantasm {

NavGrid::NavGrid(int width, int height, std::uint8_t cost)
    : width_{width}, height_{height},
      costs_(static_cast<std::size_t>(width > 0 && height > 0 ? width * height : 0), cost) {}

bool NavGrid::contains(Cell cell) const noexcept {
    return cell.x >= 0 && cell.y >= 0 && cell.x < width_ && cell.y < height_;
}

void NavGrid::set_cost(Cell cell, std::uint8_t cost) {
    if (contains(cell)) {
        costs_[static_cast<std::size_t>(cell.y * width_ + cell.x)] = cost;
    }
}

std::uint8_t NavGrid::cost(Cell cell) const noexcept {
    return contains(cell) ? costs_[static_cast<std::size_t>(cell.y * width_ + cell.x)] : 0;
}

namespace {

constexpr float kDiagonal = 1.41421356F;
constexpr int kNoParent = -1;

struct Step {
    int dx;
    int dy;
    float length;
};

constexpr std::array<Step, 8> kSteps{{
    {1, 0, 1.0F}, {-1, 0, 1.0F}, {0, 1, 1.0F}, {0, -1, 1.0F},
    {1, 1, kDiagonal}, {1, -1, kDiagonal}, {-1, 1, kDiagonal}, {-1, -1, kDiagonal},
}};

// Lower bound on the remaining cost (cheapest terrain costs 1 per unit of distance).
float estimate(Cell from, Cell to, bool diagonal) noexcept {
    const auto dx = static_cast<float>(std::abs(to.x - from.x));
    const auto dy = static_cast<float>(std::abs(to.y - from.y));
    return diagonal ? dx + dy + (kDiagonal - 2.0F) * std::min(dx, dy) : dx + dy;
}

struct OpenEntry {
    float priority;
    float estimate; // tie-break: prefer entries closer to the goal
    int index;
    bool operator>(const OpenEntry& other) const noexcept {
        if (priority != other.priority) {
            return priority > other.priority;
        }
        if (estimate != other.estimate) {
            return estimate > other.estimate;
        }
        return index > other.index;
    }
};

} // namespace

std::vector<Cell> find_path(const NavGrid& grid, Cell start, Cell goal, PathOptions options) {
    if (!grid.passable(start) || !grid.passable(goal)) {
        return {};
    }
    const int width = grid.width();
    const auto cell_count = static_cast<std::size_t>(width * grid.height());
    const auto index_of = [width](Cell cell) { return cell.y * width + cell.x; };
    const auto cell_at = [width](int index) { return Cell{index % width, index / width}; };

    std::vector<float> cost(cell_count, std::numeric_limits<float>::infinity());
    std::vector<int> parent(cell_count, kNoParent);
    std::vector<bool> closed(cell_count, false);
    std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<>> open;

    const int start_index = index_of(start);
    const int goal_index = index_of(goal);
    cost[static_cast<std::size_t>(start_index)] = 0.0F;
    open.push({estimate(start, goal, options.diagonal), estimate(start, goal, options.diagonal),
               start_index});
    const std::size_t step_count = options.diagonal ? kSteps.size() : 4;

    while (!open.empty()) {
        const int current = open.top().index;
        open.pop();
        if (closed[static_cast<std::size_t>(current)]) {
            continue; // stale entry: a cheaper route was found later
        }
        if (current == goal_index) {
            break;
        }
        closed[static_cast<std::size_t>(current)] = true;
        const Cell here = cell_at(current);
        for (std::size_t s = 0; s < step_count; ++s) {
            const Step step = kSteps[s];
            const Cell next{here.x + step.dx, here.y + step.dy};
            if (!grid.passable(next)) {
                continue;
            }
            if (step.dx != 0 && step.dy != 0 &&
                (!grid.passable({here.x + step.dx, here.y}) ||
                 !grid.passable({here.x, here.y + step.dy}))) {
                continue; // no squeezing between blocked corners
            }
            const int next_index = index_of(next);
            const float next_cost = cost[static_cast<std::size_t>(current)] +
                                    step.length * static_cast<float>(grid.cost(next));
            if (next_cost < cost[static_cast<std::size_t>(next_index)]) {
                cost[static_cast<std::size_t>(next_index)] = next_cost;
                parent[static_cast<std::size_t>(next_index)] = current;
                const float remaining = estimate(next, goal, options.diagonal);
                open.push({next_cost + remaining, remaining, next_index});
            }
        }
    }

    if (parent[static_cast<std::size_t>(goal_index)] == kNoParent && goal_index != start_index) {
        return {};
    }
    std::vector<Cell> path;
    for (int index = goal_index; index != kNoParent; index = parent[static_cast<std::size_t>(index)]) {
        path.push_back(cell_at(index));
    }
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace phantasm
