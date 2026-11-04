#include <phantasm/iso/scene.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <string>

namespace {

phantasm::iso::Scene make_scene(int count) {
    phantasm::iso::Scene scene{phantasm::iso::TileMap::blank(160, 160, 2)};
    for (int i = 0; i < count; ++i) {
        (void)scene.create(phantasm::iso::GridPos{static_cast<float>(i % 160),
                                                   static_cast<float>((i / 160) % 160), i % 2},
                           "object " + std::to_string(i));
    }
    return scene;
}

void BM_SceneSerialize(benchmark::State& state) {
    const auto scene = make_scene(static_cast<int>(state.range(0)));
    for (auto _ : state) {
        const auto text = scene.serialize();
        benchmark::DoNotOptimize(text.data());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_SceneParse(benchmark::State& state) {
    const auto text = make_scene(static_cast<int>(state.range(0))).serialize();
    for (auto _ : state) {
        const auto scene = phantasm::iso::Scene::parse(text);
        benchmark::DoNotOptimize(scene.entities().size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

} // namespace

BENCHMARK(BM_SceneSerialize)->Arg(10'000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SceneParse)->Arg(10'000)->Unit(benchmark::kMillisecond);
