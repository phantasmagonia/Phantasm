#include <phantasm/sprite_batch.hpp>
#include <phantasm/texture.hpp>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <random>
#include <vector>

namespace {

using phantasm::SpriteDraw;
using phantasm::Texture;

// Textures with null GPU handles: sorting only compares their addresses.
const std::deque<Texture>& textures(std::size_t count) {
    static std::deque<Texture> pool;
    while (pool.size() < count) {
        pool.emplace_back(nullptr, nullptr, 1, 1);
    }
    return pool;
}

enum class Order { Random, Sorted, MostlySorted, Uniform };

std::vector<SpriteDraw> make_sprites(std::size_t count, std::size_t texture_count, Order order) {
    std::mt19937 rng{42};
    std::uniform_real_distribution<float> depth{0.0F, 10'000.0F};
    std::uniform_int_distribution<std::size_t> pick{0, texture_count - 1};
    const auto& pool = textures(texture_count);

    std::vector<SpriteDraw> sprites(count);
    for (SpriteDraw& sprite : sprites) {
        sprite.texture = &pool[pick(rng)];
        sprite.depth = order == Order::Uniform ? 0.0F : depth(rng);
    }
    if (order == Order::Sorted || order == Order::MostlySorted) {
        std::sort(sprites.begin(), sprites.end(),
                  [](const SpriteDraw& a, const SpriteDraw& b) { return a.depth < b.depth; });
    }
    if (order == Order::MostlySorted) {
        // Frame-to-frame coherence: ~1% of sprites moved since last frame.
        std::uniform_int_distribution<std::size_t> index{0, count - 1};
        for (std::size_t i = 0; i < count / 100; ++i) {
            std::swap(sprites[index(rng)], sprites[index(rng)]);
        }
    }
    return sprites;
}

void run_sort_and_group(benchmark::State& state, Order order) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const auto texture_count = static_cast<std::size_t>(state.range(1));
    const std::vector<SpriteDraw> source = make_sprites(count, texture_count, order);
    std::vector<SpriteDraw> sprites;

    for (auto _ : state) {
        state.PauseTiming();
        sprites = source;
        state.ResumeTiming();
        auto runs = phantasm::sort_and_group(sprites);
        benchmark::DoNotOptimize(runs.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.counters["draw_calls"] =
        static_cast<double>(phantasm::sort_and_group(sprites).size());
}

void BM_SortAndGroup_Random(benchmark::State& state) {
    run_sort_and_group(state, Order::Random);
}
void BM_SortAndGroup_MostlySorted(benchmark::State& state) {
    run_sort_and_group(state, Order::MostlySorted);
}

// No ordering needed (one depth, one texture): the "plain" case of the engine comparison.
void BM_SortAndGroup_Uniform(benchmark::State& state) {
    run_sort_and_group(state, Order::Uniform);
}

void sprite_args(benchmark::Benchmark* bench) {
    for (const std::int64_t count : {1'000, 10'000, 100'000, 1'000'000}) {
        for (const std::int64_t texture_count : {1, 16}) {
            bench->Args({count, texture_count});
        }
    }
    bench->ArgNames({"sprites", "textures"})->Unit(benchmark::kMicrosecond);
}

} // namespace

BENCHMARK(BM_SortAndGroup_Random)->Apply(sprite_args);
BENCHMARK(BM_SortAndGroup_MostlySorted)->Apply(sprite_args);
BENCHMARK(BM_SortAndGroup_Uniform)->Apply(sprite_args);
