#include <phantasm/pathfinding.hpp>

#include <benchmark/benchmark.h>

#include <random>
#include <vector>

namespace {

using phantasm::Cell;
using phantasm::FlowField;
using phantasm::NavGrid;

constexpr int kSize = 512;

NavGrid make_grid() {
    NavGrid grid{kSize, kSize};
    std::mt19937 rng{5};
    std::uniform_int_distribution<int> roll{0, 9};
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const int r = roll(rng);
            grid.set_cost({x, y}, r < 2 ? 0 : r < 8 ? 1 : 3);
        }
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) { // keep both corners open so a route exists
            grid.set_cost({i, j}, 1);
            grid.set_cost({kSize - 1 - i, kSize - 1 - j}, 1);
        }
    }
    return grid;
}

const NavGrid& grid() {
    static const NavGrid instance = make_grid();
    return instance;
}

// One agent crossing the whole map (worst case for A*).
void BM_AStarAcrossMap(benchmark::State& state) {
    const auto probe = phantasm::find_path(grid(), {0, 0}, {kSize - 1, kSize - 1});
    if (probe.empty()) {
        state.SkipWithError("No path across the benchmark map; the measurement would be meaningless");
        return;
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(phantasm::find_path(grid(), {0, 0}, {kSize - 1, kSize - 1}));
    }
    state.counters["path_cells"] = static_cast<double>(probe.size());
}

// One shared field toward a player; every agent then reads its next step.
void BM_FlowFieldBuild(benchmark::State& state) {
    const std::vector<Cell> goals{{kSize / 2, kSize / 2}};
    for (auto _ : state) {
        benchmark::DoNotOptimize(FlowField::build(grid(), goals));
    }
}

void BM_FlowFieldAgentSteps(benchmark::State& state) {
    const std::vector<Cell> goals{{kSize / 2, kSize / 2}};
    const FlowField field = FlowField::build(grid(), goals);
    const auto agents = static_cast<std::size_t>(state.range(0));
    std::mt19937 rng{9};
    std::uniform_int_distribution<int> coord{0, kSize - 1};
    std::vector<Cell> positions(agents);
    for (Cell& p : positions) {
        p = {coord(rng), coord(rng)};
    }
    for (auto _ : state) {
        for (Cell& p : positions) {
            auto next = field.next(p);
            benchmark::DoNotOptimize(next);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

} // namespace

BENCHMARK(BM_AStarAcrossMap)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_FlowFieldBuild)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_FlowFieldAgentSteps)->Arg(10'000)->Unit(benchmark::kMicrosecond);
