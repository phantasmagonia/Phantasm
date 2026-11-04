#include <phantasm/physics/world.hpp>

#include <benchmark/benchmark.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <utility>
#include <vector>

namespace {

using phantasm::physics::BodyDef;
using phantasm::physics::BodyType;
using phantasm::physics::Box;
using phantasm::physics::Circle;
using phantasm::physics::World;
using phantasm::physics::WorldConfig;

constexpr float kStep = 1.0F / 60.0F;
constexpr float kUnitsPerMeter = 32.0F;
constexpr int kSettleSteps = 60;

void add_walls(World& world, float half_width, float half_height) {
    constexpr float kThickness = 32.0F;
    const auto wall = [&](float x, float y, float hx, float hy) {
        const auto body = world.create_body(BodyDef{.type = BodyType::Static, .position = {x, y}});
        world.add_box(body, Box{.half_extents = {hx, hy}});
    };
    wall(0.0F, -half_height - kThickness, half_width + kThickness, kThickness);
    wall(0.0F, half_height + kThickness, half_width + kThickness, kThickness);
    wall(-half_width - kThickness, 0.0F, kThickness, half_height);
    wall(half_width + kThickness, 0.0F, kThickness, half_height);
}

// Top-down crowd: characters as circles, no gravity, random walking velocities.
void BM_PhysicsStep_TopDown(benchmark::State& state) {
    const auto count = static_cast<int>(state.range(0));
    // Field sized for ~25% coverage so bodies keep colliding.
    const float half = std::sqrt(static_cast<float>(count)) * 32.0F;
    World world{WorldConfig{.units_per_meter = kUnitsPerMeter}};
    add_walls(world, half, half);

    std::mt19937 rng{1};
    std::uniform_real_distribution<float> position{-half * 0.95F, half * 0.95F};
    std::uniform_real_distribution<float> speed{-96.0F, 96.0F};
    for (int i = 0; i < count; ++i) {
        const auto body = world.create_body(BodyDef{.position = {position(rng), position(rng)},
                                                    .velocity = {speed(rng), speed(rng)}});
        world.add_circle(body, Circle{.radius = 8.0F, .restitution = 0.5F});
    }
    for (int i = 0; i < kSettleSteps; ++i) {
        world.step(kStep);
    }

    for (auto _ : state) {
        world.step(kStep);
    }
    state.SetItemsProcessed(state.iterations() * count);
    state.counters["bodies"] = static_cast<double>(count);
}

constexpr int kCollapseSteps = 120; // 2 seconds at 60 Hz

void build_pile(World& world, int count) {
    const int columns = static_cast<int>(std::sqrt(static_cast<float>(count)) * 2.0F);
    const float half_width = static_cast<float>(columns) * 20.0F;
    constexpr float kFloor = 10'000.0F;
    add_walls(world, half_width, kFloor);
    for (int i = 0; i < count; ++i) {
        const float x = -half_width + 20.0F + static_cast<float>(i % columns) * 40.0F;
        // Rows start 200 units above the floor, so boxes fall, collide, and stack.
        const float y = kFloor - 200.0F - static_cast<float>(i / columns) * 40.0F;
        const auto body = world.create_body(BodyDef{.position = {x, y}});
        world.add_box(body, Box{.half_extents = {16.0F, 16.0F}});
    }
}

// Pile collapse: boxes drop, collide, and stack; the hardest case for the solver.
// Each iteration times 120 steps of a fresh pile (setup excluded). Once a pile
// rests, Box2D puts it to sleep and further steps cost almost nothing.
void BM_PhysicsPileCollapse(benchmark::State& state) {
    const auto count = static_cast<int>(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        auto world = std::make_unique<World>(WorldConfig{
            .gravity = {0.0F, 9.8F * kUnitsPerMeter}, .units_per_meter = kUnitsPerMeter});
        build_pile(*world, count);
        state.ResumeTiming();
        for (int i = 0; i < kCollapseSteps; ++i) {
            world->step(kStep);
        }
        state.PauseTiming();
        world.reset();
        state.ResumeTiming();
    }
    state.counters["step_time"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * kCollapseSteps,
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
    state.counters["bodies"] = static_cast<double>(count);
}

// Line-of-sight workload: a grid of wall blocks and many rays from random points.
void BM_PhysicsRaycast(benchmark::State& state) {
    constexpr int kGrid = 100; // 10,000 static wall blocks
    constexpr float kSpacing = 64.0F;
    World world{WorldConfig{.units_per_meter = kUnitsPerMeter}};
    for (int y = 0; y < kGrid; ++y) {
        for (int x = 0; x < kGrid; ++x) {
            const auto wall = world.create_body(BodyDef{
                .type = BodyType::Static,
                .position = {static_cast<float>(x) * kSpacing, static_cast<float>(y) * kSpacing}});
            world.add_box(wall, Box{.half_extents = {12.0F, 12.0F}});
        }
    }
    world.step(kStep); // build the broad-phase tree

    const auto ray_count = static_cast<std::size_t>(state.range(0));
    std::mt19937 rng{5};
    std::uniform_real_distribution<float> coord{0.0F, kGrid * kSpacing};
    std::uniform_real_distribution<float> offset{-320.0F, 320.0F}; // ~5 tiles of sight
    std::vector<std::pair<phantasm::Vec2, phantasm::Vec2>> rays(ray_count);
    for (auto& [from, to] : rays) {
        from = {coord(rng), coord(rng)};
        to = {from.x + offset(rng), from.y + offset(rng)};
    }

    for (auto _ : state) {
        int hits = 0;
        for (const auto& [from, to] : rays) {
            hits += world.raycast(from, to).has_value() ? 1 : 0;
        }
        benchmark::DoNotOptimize(hits);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(ray_count));
}

} // namespace

BENCHMARK(BM_PhysicsStep_TopDown)->Arg(1'000)->Arg(5'000)->Arg(10'000)->Arg(20'000)
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PhysicsPileCollapse)->Arg(1'000)->Arg(5'000)->Arg(10'000)
    ->Unit(benchmark::kMillisecond)->Iterations(5);
BENCHMARK(BM_PhysicsRaycast)->Arg(1'000)->Arg(10'000)->Unit(benchmark::kMillisecond);
