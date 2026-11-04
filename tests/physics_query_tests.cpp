#include <phantasm/physics/world.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using phantasm::Rect;
using phantasm::Vec2;
using phantasm::physics::BodyDef;
using phantasm::physics::BodyHandle;
using phantasm::physics::BodyType;
using phantasm::physics::Box;
using phantasm::physics::Circle;
using phantasm::physics::CollisionFilter;
using phantasm::physics::World;
using phantasm::physics::WorldConfig;

namespace {

constexpr float kStep = 1.0F / 60.0F;
constexpr std::uint64_t kWalls = 1U << 0;
constexpr std::uint64_t kPlayer = 1U << 1;
constexpr std::uint64_t kItems = 1U << 2;

void run(World& world, int steps) {
    for (int i = 0; i < steps; ++i) {
        world.step(kStep);
    }
}

BodyHandle add_wall(World& world, Vec2 center, Vec2 half_extents) {
    const auto wall = world.create_body(BodyDef{.type = BodyType::Static, .position = center});
    world.add_box(wall, Box{.half_extents = half_extents,
                            .filter = CollisionFilter{.category = kWalls}});
    return wall;
}

} // namespace

TEST_CASE("Bodies whose filters exclude each other pass through") {
    World world{WorldConfig{}};
    const auto a = world.create_body(BodyDef{.position = {0, 0}});
    const auto b = world.create_body(BodyDef{.position = {4, 0}});
    world.add_circle(a, Circle{.radius = 16, .filter = {.category = kPlayer, .mask = kWalls}});
    world.add_circle(b, Circle{.radius = 16, .filter = {.category = kItems, .mask = kWalls}});

    run(world, 60);

    CHECK(world.position(b).x - world.position(a).x == Catch::Approx(4.0F).margin(0.01F));
}

TEST_CASE("Walking through a sensor reports enter and leave once each") {
    World world{WorldConfig{}};
    const auto pickup = world.create_body(BodyDef{.type = BodyType::Static, .position = {100, 0}});
    world.add_box(pickup, Box{.half_extents = {8, 8}, .sensor = true});
    const auto player = world.create_body(BodyDef{.position = {0, 0}, .velocity = {200, 0}});
    world.add_circle(player, Circle{.radius = 8});

    run(world, 30); // reaches x = 100
    auto events = world.take_sensor_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].entered);
    CHECK(events[0].sensor == pickup);
    CHECK(events[0].visitor == player);
    CHECK(world.take_sensor_events().empty()); // taking clears

    run(world, 60); // leaves the sensor
    events = world.take_sensor_events();
    REQUIRE(events.size() == 1);
    CHECK_FALSE(events[0].entered);
}

TEST_CASE("Sensors do not block movement") {
    World world{WorldConfig{}};
    const auto pickup = world.create_body(BodyDef{.type = BodyType::Static, .position = {100, 0}});
    world.add_box(pickup, Box{.half_extents = {8, 8}, .sensor = true});
    const auto player = world.create_body(BodyDef{.velocity = {200, 0}});
    world.add_circle(player, Circle{.radius = 8});

    run(world, 60);

    CHECK(world.position(player).x == Catch::Approx(200.0F).margin(3.0F));
}

TEST_CASE("Events from several steps between takes are all kept") {
    World world{WorldConfig{}};
    const auto pickup = world.create_body(BodyDef{.type = BodyType::Static, .position = {20, 0}});
    world.add_box(pickup, Box{.half_extents = {4, 4}, .sensor = true});
    const auto player = world.create_body(BodyDef{.velocity = {600, 0}});
    world.add_circle(player, Circle{.radius = 4});

    run(world, 10); // enters and leaves without a take in between

    const auto events = world.take_sensor_events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].entered);
    CHECK_FALSE(events[1].entered);
}

TEST_CASE("Shapes can opt out of triggering sensors") {
    World world{WorldConfig{}};
    const auto pickup = world.create_body(BodyDef{.type = BodyType::Static, .position = {100, 0}});
    world.add_box(pickup, Box{.half_extents = {8, 8}, .sensor = true});
    const auto ghost = world.create_body(BodyDef{.velocity = {200, 0}});
    world.add_circle(ghost, Circle{.radius = 8, .triggers_sensors = false});

    run(world, 60);

    CHECK(world.take_sensor_events().empty());
}

TEST_CASE("A raycast returns the nearest hit in world units") {
    World world{WorldConfig{}};
    const auto near_wall = add_wall(world, {100, 0}, {10, 50});
    add_wall(world, {300, 0}, {10, 50});

    const auto hit = world.raycast({0, 0}, {500, 0});

    REQUIRE(hit.has_value());
    CHECK(hit->body == near_wall);
    CHECK(hit->point.x == Catch::Approx(90.0F).margin(0.1F));
    CHECK(hit->normal.x == Catch::Approx(-1.0F));
    CHECK(hit->fraction == Catch::Approx(90.0F / 500.0F).margin(0.001F));
}

TEST_CASE("A raycast that hits nothing returns no hit") {
    World world{WorldConfig{}};
    add_wall(world, {100, 200}, {10, 10});

    CHECK_FALSE(world.raycast({0, 0}, {500, 0}).has_value());
    CHECK_FALSE(world.raycast({0, 0}, {0, 0}).has_value());
}

TEST_CASE("A raycast skips categories outside its mask") {
    World world{WorldConfig{}};
    const auto item = world.create_body(BodyDef{.type = BodyType::Static, .position = {50, 0}});
    world.add_circle(item, Circle{.radius = 5, .filter = {.category = kItems}});
    const auto wall = add_wall(world, {100, 0}, {10, 50});

    const auto hit = world.raycast({0, 0}, {500, 0}, kWalls);

    REQUIRE(hit.has_value());
    CHECK(hit->body == wall);
}

TEST_CASE("An area query returns each overlapping body once") {
    World world{WorldConfig{}};
    const auto two_shapes = world.create_body(BodyDef{.type = BodyType::Static, .position = {0, 0}});
    world.add_circle(two_shapes, Circle{.radius = 4});
    world.add_box(two_shapes, Box{.half_extents = {6, 6}});
    const auto nearby = add_wall(world, {20, 0}, {4, 4});
    const auto far_away = add_wall(world, {500, 500}, {4, 4});

    const auto found = world.query_area(Rect{-10, -10, 40, 20});

    CHECK(found.size() == 2);
    CHECK(std::ranges::count(found, two_shapes) == 1);
    CHECK(std::ranges::count(found, nearby) == 1);
    CHECK(std::ranges::count(found, far_away) == 0);
}
