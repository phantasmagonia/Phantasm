#include <phantasm/sprite_batch.hpp>
#include <phantasm/texture.hpp>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <random>
#include <vector>

namespace {

using phantasm::SpriteInstance;
using phantasm::SpriteQueue;
using phantasm::Texture;

// Textures with null GPU handles: sorting only uses their ids and addresses.
const std::deque<Texture>& textures(std::size_t count) {
    static std::deque<Texture> pool;
    while (pool.size() < count) {
        pool.emplace_back(nullptr, nullptr, 1, 1);
    }
    return pool;
}

enum class Order { Random, MostlySorted, Uniform };

struct Input {
    const Texture* texture;
    float depth;
};

std::vector<Input> make_inputs(std::size_t count, std::size_t texture_count, Order order) {
    std::mt19937 rng{42};
    std::uniform_real_distribution<float> depth{0.0F, 10'000.0F};
    std::uniform_int_distribution<std::size_t> pick{0, texture_count - 1};
    const auto& pool = textures(texture_count);
    std::vector<Input> inputs(count);
    for (Input& input : inputs) {
        input = {&pool[pick(rng)], order == Order::Uniform ? 0.0F : depth(rng)};
    }
    if (order == Order::MostlySorted) {
        // Frame-to-frame coherence: sorted, then ~1% of sprites moved.
        std::sort(inputs.begin(), inputs.end(),
                  [](const Input& a, const Input& b) { return a.depth < b.depth; });
        std::uniform_int_distribution<std::size_t> index{0, count - 1};
        for (std::size_t i = 0; i < count / 100; ++i) {
            std::swap(inputs[index(rng)], inputs[index(rng)]);
        }
    }
    return inputs;
}

// The renderer's per-frame CPU path: queue every sprite, sort, write the sorted
// instances to (simulated) GPU memory.
void run_queue(benchmark::State& state, Order order) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const auto texture_count = static_cast<std::size_t>(state.range(1));
    const auto inputs = make_inputs(count, texture_count, order);
    std::vector<SpriteInstance> gpu_memory(count);
    SpriteQueue queue;
    std::size_t draw_calls = 0;

    for (auto _ : state) {
        queue.clear();
        for (const Input& input : inputs) {
            queue.push(*input.texture, 0, input.depth, SpriteInstance{});
        }
        draw_calls = queue.sort().size();
        queue.write_sorted(gpu_memory);
        benchmark::DoNotOptimize(gpu_memory.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.counters["draw_calls"] = static_cast<double>(draw_calls);
}

void BM_SpriteQueue_Random(benchmark::State& state) {
    run_queue(state, Order::Random);
}
void BM_SpriteQueue_MostlySorted(benchmark::State& state) {
    run_queue(state, Order::MostlySorted);
}
// No ordering needed (one depth, one texture): the "plain" case of the engine comparison.
void BM_SpriteQueue_Uniform(benchmark::State& state) {
    run_queue(state, Order::Uniform);
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

BENCHMARK(BM_SpriteQueue_Random)->Apply(sprite_args);
BENCHMARK(BM_SpriteQueue_MostlySorted)->Apply(sprite_args);
BENCHMARK(BM_SpriteQueue_Uniform)->Apply(sprite_args);
