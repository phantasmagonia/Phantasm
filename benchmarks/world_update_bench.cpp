#include <phantasm/task_pool.hpp>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace {

struct Body {
    float x;
    float y;
    float vx;
    float vy;
};

std::vector<Body> make_bodies(std::size_t count) {
    std::vector<Body> bodies;
    bodies.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto x = static_cast<float>(i % 2400) - 1200.0F;
        const auto y = static_cast<float>((i / 2400) % 1400) - 700.0F;
        bodies.push_back({x, y, 65.0F, -45.0F});
    }
    return bodies;
}

void update_range(std::vector<Body>& bodies, std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) {
        Body& body = bodies[i];
        body.x += body.vx * 0.016F;
        body.y += body.vy * 0.016F;
        if (body.x < -1200.0F || body.x > 1200.0F) {
            body.vx = -body.vx;
            body.x = std::clamp(body.x, -1200.0F, 1200.0F);
        }
        if (body.y < -700.0F || body.y > 700.0F) {
            body.vy = -body.vy;
            body.y = std::clamp(body.y, -700.0F, 700.0F);
        }
    }
}

void BM_SequentialWorldUpdate(benchmark::State& state) {
    auto bodies = make_bodies(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state) {
        update_range(bodies, 0, bodies.size());
        benchmark::DoNotOptimize(bodies.data());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_ParallelWorldUpdate(benchmark::State& state) {
    auto bodies = make_bodies(static_cast<std::size_t>(state.range(0)));
    phantasm::TaskPool pool{3};
    for (auto _ : state) {
        pool.parallel_for(bodies.size(), static_cast<std::size_t>(state.range(1)),
                          [&](std::size_t begin, std::size_t end) {
                              update_range(bodies, begin, end);
                          });
        benchmark::DoNotOptimize(bodies.data());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

} // namespace

BENCHMARK(BM_SequentialWorldUpdate)->Arg(100'000)->Arg(1'000'000)->UseRealTime();
BENCHMARK(BM_ParallelWorldUpdate)
    ->Args({100'000, 8'192})
    ->Args({1'000'000, 8'192})
    ->Args({1'000'000, 32'768})
    ->Args({1'000'000, 65'536})
    ->UseRealTime();
