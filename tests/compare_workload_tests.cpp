#include "../benchmarks/compare/workload.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>

TEST_CASE("Comparison timer measures exactly the requested frame count") {
    compare::FrameTimer timer;
    auto now = std::chrono::steady_clock::time_point{};
    int frames = 0;
    while (timer.tick(now)) {
        ++frames;
        now += std::chrono::milliseconds{1};
    }
    CHECK(frames == compare::kWarmupFrames + compare::kMeasuredFrames);
    CHECK(timer.measured_frames() == compare::kMeasuredFrames);
    CHECK_FALSE(timer.tick(now));
    CHECK(timer.measured_frames() == compare::kMeasuredFrames);
}

TEST_CASE("Isometric comparison keeps floors fixed while characters move") {
    compare::IsoScene scene{100};
    REQUIRE(scene.floors().size() == compare::kMapTiles * compare::kMapTiles);
    REQUIRE_FALSE(scene.walls().empty());
    const auto floor = scene.floors().front();
    const auto character = scene.characters().front();
    scene.update(1.0F / 60.0F);
    CHECK(scene.floors().front().x == floor.x);
    CHECK(scene.floors().front().y == floor.y);
    CHECK((scene.characters().front().gx != character.gx ||
           scene.characters().front().gy != character.gy));
    const auto& objects = scene.objects(true);
    CHECK(objects.size() == scene.walls().size() + 100);
    CHECK(std::is_sorted(objects.begin(), objects.end(),
                         [](const auto& a, const auto& b) { return a.depth < b.depth; }));
}
