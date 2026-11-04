#include <phantasm/entity_store.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

struct Position { float x, y; };
struct Velocity { float x, y; };
struct Visible { float depth, width, height, tint; };
struct Stunned {};

void populate(phantasm::EntityStore& store, std::size_t count,
              std::vector<phantasm::Entity>* handles = nullptr) {
    if (handles != nullptr) {
        handles->reserve(count);
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto entity = store.create(Position{static_cast<float>(i % 1600),
                                                   static_cast<float>(i / 1600)},
                                         Velocity{1.0F, 0.5F}, Visible{1, 8, 12, 1});
        if (handles != nullptr) {
            handles->push_back(entity);
        }
    }
}

void BM_EntityUpdate(benchmark::State& state) {
    phantasm::EntityStore store;
    populate(store, static_cast<std::size_t>(state.range(0)));
    state.counters["array_bytes_per_entity"] =
        static_cast<double>(store.storage_bytes()) / static_cast<double>(store.size());
    for (auto _ : state) {
        store.each_chunk<Position, Velocity>([](auto, auto positions, auto velocities) {
            for (std::size_t i = 0; i < positions.size(); ++i) {
                positions[i].x += velocities[i].x;
                positions[i].y += velocities[i].y;
            }
        });
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_EntityExtract(benchmark::State& state) {
    phantasm::EntityStore store;
    populate(store, static_cast<std::size_t>(state.range(0)));
    std::vector<float> depths(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state) {
        std::size_t written = 0;
        store.each_chunk<Position, Visible>([&](auto, auto positions, auto visible) {
            for (std::size_t i = 0; i < positions.size(); ++i) {
                depths[written++] = positions[i].y + visible[i].depth;
            }
        });
        benchmark::DoNotOptimize(depths.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_EntityFlagStructural(benchmark::State& state) {
    phantasm::EntityStore store;
    std::vector<phantasm::Entity> handles;
    populate(store, static_cast<std::size_t>(state.range(0)), &handles);
    const std::size_t changed = handles.size() / 100;
    bool active = false;
    for (auto _ : state) {
        for (std::size_t i = 0; i < changed; ++i) {
            if (active) {
                benchmark::DoNotOptimize(store.remove<Stunned>(handles[i]));
            } else {
                benchmark::DoNotOptimize(store.add(handles[i], Stunned{}));
            }
        }
        active = !active;
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(changed));
}

void BM_EntityFlagSidecar(benchmark::State& state) {
    phantasm::EntityStore store;
    std::vector<phantasm::Entity> handles;
    populate(store, static_cast<std::size_t>(state.range(0)), &handles);
    phantasm::EntityFlags flags{store};
    const std::size_t changed = handles.size() / 100;
    bool active = false;
    for (auto _ : state) {
        for (std::size_t i = 0; i < changed; ++i) {
            benchmark::DoNotOptimize(flags.set(handles[i], 1, active));
        }
        active = !active;
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(changed));
}

void BM_EntityCreateDestroy(benchmark::State& state) {
    const auto count = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        phantasm::EntityStore store;
        std::vector<phantasm::Entity> handles;
        populate(store, count, &handles);
        for (const auto entity : handles) {
            store.destroy(entity);
        }
        benchmark::DoNotOptimize(store.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

} // namespace

BENCHMARK(BM_EntityUpdate)->Arg(100'000)->Arg(1'000'000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_EntityExtract)->Arg(100'000)->Arg(1'000'000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_EntityFlagStructural)->Arg(1'000'000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_EntityFlagSidecar)->Arg(1'000'000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_EntityCreateDestroy)->Arg(100'000)->Arg(1'000'000)->Unit(benchmark::kMillisecond);
