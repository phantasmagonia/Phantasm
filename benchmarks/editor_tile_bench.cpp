#include <phantasm/iso/iso_grid.hpp>
#include <phantasm/iso/tile_map.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>

namespace {

constexpr int kWidth = 4096;
constexpr int kHeight = 4096;
constexpr phantasm::Rect kView{-500.0F, 300.0F, 1000.0F, 750.0F};
constexpr phantasm::iso::IsoGrid kGrid{};

void BM_EditorAllTiles(benchmark::State& state) {
    const auto map = phantasm::iso::TileMap::blank(kWidth, kHeight, 1);
    for (auto _ : state) {
        std::size_t count = 0;
        for (int y = 0; y < map.height(); ++y) {
            for (int x = 0; x < map.width(); ++x) {
                count += map.at({x, y, 0}) == phantasm::iso::TileMap::kEmpty;
            }
        }
        benchmark::DoNotOptimize(count);
    }
}

void BM_EditorVisibleTiles(benchmark::State& state) {
    const auto map = phantasm::iso::TileMap::blank(kWidth, kHeight, 1);
    for (auto _ : state) {
        const auto bounds = phantasm::iso::visible_tile_bounds(
            kGrid, kView, 0, map.width(), map.height());
        std::size_t count = 0;
        for (int y = bounds.first_y; y < bounds.last_y; ++y) {
            for (int x = bounds.first_x; x < bounds.last_x; ++x) {
                count += map.at({x, y, 0}) == phantasm::iso::TileMap::kEmpty;
            }
        }
        benchmark::DoNotOptimize(count);
    }
}

} // namespace

BENCHMARK(BM_EditorAllTiles)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_EditorVisibleTiles)->Unit(benchmark::kMillisecond);
