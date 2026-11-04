#include <phantasm/physics/world.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using phantasm::Vec2;
using phantasm::physics::BodyDef;
using phantasm::physics::BodyType;
using phantasm::physics::Box;
using phantasm::physics::Circle;
using phantasm::physics::World;
using phantasm::physics::WorldConfig;

namespace {

constexpr float kStep = 1.0F / 60.0F;
constexpr float kUnitsPerMeter = 32.0F;

void run(World& world, float seconds) {
    for (float t = 0.0F; t < seconds; t += kStep) {
        world.step(kStep);
    }
}

} // namespace

TEST_CASE("Bodies report the position they were created at, in world units") {
    World world{WorldConfig{.units_per_meter = kUnitsPerMeter}};
    const auto body = world.create_body(BodyDef{.position = {320.0F, -64.0F}});

    CHECK(world.position(body).x == Catch::Approx(320.0F));
    CHECK(world.position(body).y == Catch::Approx(-64.0F));
}

TEST_CASE("Gravity accelerates dynamic bodies, with y pointing down") {
    World world{WorldConfig{.gravity = {0.0F, 9.8F * kUnitsPerMeter},
                            .units_per_meter = kUnitsPerMeter}};
    const auto body = world.create_body(BodyDef{.position = {0.0F, 0.0F}});
    world.add_circle(body, Circle{.radius = 8.0F});

    run(world, 1.0F);

    // Free fall for 1 s: about 4.9 m, i.e. ~157 units. Loose bounds: integrator differences.
    CHECK(world.position(body).y > 120.0F);
    CHECK(world.position(body).y < 180.0F);
}

TEST_CASE("Static ground stops a falling box") {
    World world{WorldConfig{.gravity = {0.0F, 9.8F * kUnitsPerMeter},
                            .units_per_meter = kUnitsPerMeter}};
    const auto ground = world.create_body(BodyDef{.type = BodyType::Static, .position = {0, 100}});
    world.add_box(ground, Box{.half_extents = {500.0F, 10.0F}});
    const auto crate = world.create_body(BodyDef{.position = {0.0F, 0.0F}});
    world.add_box(crate, Box{.half_extents = {16.0F, 16.0F}});

    run(world, 3.0F);

    // Ground top is at y = 90; the crate's center rests 16 units above it.
    CHECK(world.position(crate).y == Catch::Approx(74.0F).margin(1.5F));
    CHECK(world.velocity(crate).y == Catch::Approx(0.0F).margin(1.0F));
}

TEST_CASE("Top-down worlds without gravity keep constant velocity") {
    World world{WorldConfig{.units_per_meter = kUnitsPerMeter}};
    const auto body = world.create_body(BodyDef{.velocity = {64.0F, 0.0F}});
    world.add_circle(body, Circle{.radius = 4.0F});

    run(world, 1.0F);

    CHECK(world.position(body).x == Catch::Approx(64.0F).margin(2.0F));
    CHECK(world.position(body).y == Catch::Approx(0.0F).margin(0.01F));
}

TEST_CASE("Overlapping dynamic bodies are pushed apart") {
    World world{WorldConfig{.units_per_meter = kUnitsPerMeter}};
    const auto a = world.create_body(BodyDef{.position = {0.0F, 0.0F}});
    const auto b = world.create_body(BodyDef{.position = {4.0F, 0.0F}});
    world.add_circle(a, Circle{.radius = 16.0F});
    world.add_circle(b, Circle{.radius = 16.0F});

    run(world, 1.0F);

    const float gap = world.position(b).x - world.position(a).x;
    CHECK(gap >= 31.0F);
}

TEST_CASE("Destroyed bodies become invalid and are ignored safely") {
    World world{WorldConfig{}};
    const auto body = world.create_body(BodyDef{});
    CHECK(world.is_valid(body));
    CHECK(world.body_count() == 1);

    world.destroy_body(body);

    CHECK_FALSE(world.is_valid(body));
    CHECK(world.body_count() == 0);
    world.set_velocity(body, {1.0F, 1.0F}); // no crash
    world.destroy_body(body);               // double destroy is a no-op
}

TEST_CASE("Invalid world settings are rejected") {
    CHECK_THROWS_AS(World(WorldConfig{.units_per_meter = 0.0F}), std::invalid_argument);
    CHECK_THROWS_AS(World(WorldConfig{.sub_steps = 0}), std::invalid_argument);
}
