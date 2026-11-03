#include <phantasm/chunk_index.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <random>
#include <vector>

namespace {

constexpr phantasm::Rect kView{8192, 8192, 1024, 1024};
constexpr phantasm::Rect kDenseView{-160, -90, 320, 180};

std::vector<phantasm::Rect> make_world(int side) {
    std::vector<phantasm::Rect> bounds;
    bounds.reserve(static_cast<std::size_t>(side) * static_cast<std::size_t>(side));
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            bounds.push_back({static_cast<float>(x * 32), static_cast<float>(y * 32), 16, 16});
        }
    }
    return bounds;
}

std::vector<phantasm::Rect> make_dense_world() {
    std::mt19937 rng{12345};
    std::uniform_real_distribution<float> x{-1200.0F, 1200.0F};
    std::uniform_real_distribution<float> y{-700.0F, 700.0F};
    std::vector<phantasm::Rect> bounds;
    bounds.reserve(1'000'000);
    for (std::size_t i = 0; i < 1'000'000; ++i) {
        bounds.push_back({x(rng) - 32.0F, y(rng) - 64.0F, 64.0F, 64.0F});
    }
    return bounds;
}

void BM_LinearVisibleQuery(benchmark::State& state) {
    const auto bounds = make_world(static_cast<int>(state.range(0)));
    for (auto _ : state) {
        std::uint32_t visible = 0;
        for (const auto& rect : bounds) {
            visible += phantasm::intersects(rect, kView);
        }
        benchmark::DoNotOptimize(visible);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(bounds.size()));
}

void BM_ChunkVisibleQuery(benchmark::State& state) {
    const auto bounds = make_world(static_cast<int>(state.range(0)));
    phantasm::ChunkIndex index{256};
    for (std::uint32_t id = 0; id < bounds.size(); ++id) {
        index.upsert(id, bounds[id]);
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(index.query(kView));
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(bounds.size()));
}

void BM_DenseLinearQuery(benchmark::State& state) {
    const auto bounds = make_dense_world();
    for (auto _ : state) {
        std::uint32_t count = 0;
        for (const auto& rect : bounds) {
            count += phantasm::intersects(rect, kDenseView);
        }
        benchmark::DoNotOptimize(count);
    }
}

void BM_DenseChunkQuery(benchmark::State& state) {
    const auto bounds = make_dense_world();
    phantasm::ChunkIndex index{static_cast<float>(state.range(0))};
    for (std::uint32_t id = 0; id < bounds.size(); ++id) {
        index.upsert(id, bounds[id]);
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(index.query(kDenseView));
    }
}

} // namespace

BENCHMARK(BM_LinearVisibleQuery)->Arg(320)->Arg(1024);
BENCHMARK(BM_ChunkVisibleQuery)->Arg(320)->Arg(1024);
BENCHMARK(BM_DenseLinearQuery);
BENCHMARK(BM_DenseChunkQuery)->Arg(64)->Arg(128)->Arg(256);
