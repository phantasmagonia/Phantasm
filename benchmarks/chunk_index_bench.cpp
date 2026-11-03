#include <phantasm/chunk_index.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

namespace {

constexpr phantasm::Rect kView{8192, 8192, 1024, 1024};

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

} // namespace

BENCHMARK(BM_LinearVisibleQuery)->Arg(320)->Arg(1024);
BENCHMARK(BM_ChunkVisibleQuery)->Arg(320)->Arg(1024);
