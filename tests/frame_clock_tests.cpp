#include <phantasm/frame_clock.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using phantasm::FrameClock;

namespace {
constexpr std::uint64_t kNsPerMs = 1'000'000;
}

TEST_CASE("FrameClock returns seconds elapsed since the previous tick") {
    FrameClock clock{0};

    CHECK(clock.tick(16 * kNsPerMs) == Catch::Approx(0.016));
    CHECK(clock.tick(32 * kNsPerMs) == Catch::Approx(0.016));
}

TEST_CASE("FrameClock clamps huge gaps so a stall does not explode the simulation") {
    FrameClock clock{0};

    CHECK(clock.tick(5'000 * kNsPerMs) == Catch::Approx(FrameClock::kMaxDeltaSeconds));
}

TEST_CASE("FrameClock returns zero if time does not advance or goes backwards") {
    FrameClock clock{100 * kNsPerMs};

    CHECK(clock.tick(100 * kNsPerMs) == 0.0);
    CHECK(clock.tick(50 * kNsPerMs) == 0.0);
}
