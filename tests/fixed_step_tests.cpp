#include <phantasm/fixed_step.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using phantasm::FixedStep;

namespace {
constexpr double kStep = 1.0 / 60.0;
}

TEST_CASE("One frame of exactly one step runs one step") {
    FixedStep stepper{kStep};

    CHECK(stepper.advance(kStep) == 1);
}

TEST_CASE("Short frames accumulate until a full step is available") {
    FixedStep stepper{kStep};

    CHECK(stepper.advance(kStep * 0.5) == 0);
    CHECK(stepper.advance(kStep * 0.6) == 1);
    CHECK(stepper.alpha() == Catch::Approx(0.1).margin(1e-9));
}

TEST_CASE("Long frames run several steps") {
    FixedStep stepper{kStep};

    CHECK(stepper.advance(kStep * 3.0) == 3);
}

TEST_CASE("A huge frame is capped and the backlog is dropped") {
    FixedStep stepper{kStep, 4};

    CHECK(stepper.advance(10.0) == 4);
    CHECK(stepper.alpha() == 0.0);
    CHECK(stepper.advance(kStep) == 1);
}

TEST_CASE("Negative frame time is ignored") {
    FixedStep stepper{kStep};

    CHECK(stepper.advance(-1.0) == 0);
    CHECK(stepper.alpha() == 0.0);
}

TEST_CASE("Invalid step settings are rejected") {
    CHECK_THROWS_AS(FixedStep(0.0), std::invalid_argument);
    CHECK_THROWS_AS(FixedStep(kStep, 0), std::invalid_argument);
}
