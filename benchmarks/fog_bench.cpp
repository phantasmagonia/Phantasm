#include <phantasm/iso/fog.hpp>

#include <benchmark/benchmark.h>

#include <random>
#include <vector>

namespace {

using phantasm::iso::TileCoord;
using phantasm::iso::TileFog;
using phantasm::iso::TileMap;

constexpr int kMapSize = 1024;
constexpr int kViewRadius = 24;

TileMap make_world() {
    TileMap world = TileMap::blank(kMapSize, kMapSize, 1);
    std::mt19937 rng{11};
    std::uniform_real_distribution<float> roll{0.0F, 1.0F};
    for (int y = 0; y < kMapSize; ++y) {
        for (int x = 0; x < kMapSize; ++x) {
            world.set({x, y, 0}, roll(rng) < 0.2F ? '#' : '.');
        }
    }
    return world;
}

// One frame of fog for N players (e.g. a server updating every connected player).
void BM_FogFrame(benchmark::State& state) {
    static const TileMap world = make_world();
    const auto viewers = static_cast<std::size_t>(state.range(0));
    std::mt19937 rng{3};
    std::uniform_int_distribution<int> coord{kViewRadius, kMapSize - kViewRadius - 1};
    std::vector<TileCoord> positions(viewers);
    for (TileCoord& p : positions) {
        p = {coord(rng), coord(rng), 0};
    }
    TileFog fog;
    fog.set_opaque('#', true);
    for (auto _ : state) {
        fog.begin_frame();
        for (const TileCoord& p : positions) {
            fog.add_viewer(world, p, kViewRadius);
        }
    }
    state.counters["viewers"] = static_cast<double>(viewers);
}

} // namespace

BENCHMARK(BM_FogFrame)->Arg(1)->Arg(64)->Unit(benchmark::kMicrosecond);
