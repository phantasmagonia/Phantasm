#include <phantasm/iso/iso_grid.hpp>
#include <phantasm/iso/tile_map.hpp>

#include <benchmark/benchmark.h>

#include <random>
#include <string>
#include <vector>

namespace {

using phantasm::Vec2;
using phantasm::iso::GridPos;
using phantasm::iso::IsoGrid;
using phantasm::iso::TileCoord;
using phantasm::iso::TileMap;

constexpr std::size_t kPointCount = 100'000;

std::vector<Vec2> random_world_points() {
    std::mt19937 rng{7};
    std::uniform_real_distribution<float> coord{-50'000.0F, 50'000.0F};
    std::vector<Vec2> points(kPointCount);
    for (Vec2& point : points) {
        point = {coord(rng), coord(rng)};
    }
    return points;
}

std::string make_map_text(int size, int levels) {
    std::string text;
    const std::string row(static_cast<std::size_t>(size), '.');
    for (int level = 0; level < levels; ++level) {
        text += "level " + std::to_string(level) + "\n";
        for (int y = 0; y < size; ++y) {
            text += row;
            text += '\n';
        }
    }
    return text;
}

void BM_GridToWorld(benchmark::State& state) {
    const IsoGrid grid{};
    const auto points = random_world_points();
    for (auto _ : state) {
        for (const Vec2& point : points) {
            benchmark::DoNotOptimize(grid.grid_to_world(GridPos{point.x, point.y, 1}));
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(kPointCount));
}

void BM_PickTile(benchmark::State& state) {
    const IsoGrid grid{};
    const auto points = random_world_points();
    for (auto _ : state) {
        for (const Vec2& point : points) {
            benchmark::DoNotOptimize(grid.pick_tile(point, 0));
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(kPointCount));
}

void BM_TileMapParse(benchmark::State& state) {
    const auto size = static_cast<int>(state.range(0));
    const std::string text = make_map_text(size, 4);
    for (auto _ : state) {
        benchmark::DoNotOptimize(TileMap::parse(text));
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(text.size()));
}

void BM_TileMapRandomAccess(benchmark::State& state) {
    const TileMap map = TileMap::parse(make_map_text(1024, 4));
    std::mt19937 rng{3};
    std::uniform_int_distribution<int> coord{0, 1023};
    std::uniform_int_distribution<int> level{0, 3};
    std::vector<TileCoord> lookups(kPointCount);
    for (TileCoord& tile : lookups) {
        tile = {coord(rng), coord(rng), level(rng)};
    }
    for (auto _ : state) {
        for (const TileCoord& tile : lookups) {
            benchmark::DoNotOptimize(map.at(tile));
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(kPointCount));
}

} // namespace

BENCHMARK(BM_GridToWorld);
BENCHMARK(BM_PickTile);
BENCHMARK(BM_TileMapParse)->Arg(256)->Arg(1024)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_TileMapRandomAccess);
