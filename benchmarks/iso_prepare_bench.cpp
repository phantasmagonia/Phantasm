#include "compare/workload.hpp"

#include <phantasm/sprite.hpp>
#include <phantasm/sprite_batch.hpp>
#include <phantasm/task_pool.hpp>
#include <phantasm/texture.hpp>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <vector>

namespace {

void BM_IsoPrepareGather(benchmark::State& state) {
    compare::IsoScene scene{static_cast<std::size_t>(state.range(0))};
    std::vector<phantasm::Sprite> sprites;
    for (const auto& object : scene.objects(false)) {
        sprites.push_back({.position = {object.x, object.y},
                           .size = {object.region.w, object.region.h},
                           .uv = {object.region.u / compare::kAtlasW,
                                  object.region.v / compare::kAtlasH,
                                  object.region.w / compare::kAtlasW,
                                  object.region.h / compare::kAtlasH},
                           .layer = 1, .depth = object.depth});
    }
    const phantasm::Texture texture{nullptr, nullptr, 1, 1};
    phantasm::TaskPool pool{static_cast<std::size_t>(std::max<std::int64_t>(state.range(1), 0))};
    phantasm::SpriteQueue queue;
    std::vector<phantasm::SpriteInstance> output(sprites.size());
    constexpr phantasm::Rect view{0, 0, compare::kWindowWidth, compare::kWindowHeight};
    for (auto _ : state) {
        queue.clear();
        if (state.range(1) < 0) {
            for (const auto& sprite : sprites) {
                const auto rect = phantasm::sprite_world_rect(sprite);
                if (phantasm::intersects(rect, view)) {
                    queue.push(texture, sprite.layer, sprite.depth,
                               phantasm::pack_instance(rect, sprite.uv, sprite.tint));
                }
            }
        } else {
            queue.append(texture, sprites, view, &pool);
        }
        benchmark::DoNotOptimize(queue.sort().size());
        queue.write_sorted(output, state.range(1) > 0 ? &pool : nullptr);
        benchmark::DoNotOptimize(output.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(sprites.size()));
}

void arguments(benchmark::Benchmark* bench) {
    for (const std::int64_t count : {10'000, 100'000, 1'000'000}) {
        for (const std::int64_t workers : {-1, 0, 3}) {
            bench->Args({count, workers});
        }
    }
    bench->ArgNames({"characters", "workers"})->Unit(benchmark::kMillisecond)->UseRealTime();
}

} // namespace

BENCHMARK(BM_IsoPrepareGather)->Apply(arguments);
