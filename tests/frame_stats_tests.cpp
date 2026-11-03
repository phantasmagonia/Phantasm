#include <phantasm/frame_stats.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using phantasm::summarize;

TEST_CASE("Summary of no samples is all zeros") {
    const auto summary = summarize({});

    CHECK(summary.count == 0);
    CHECK(summary.mean == 0.0);
    CHECK(summary.p99 == 0.0);
}

TEST_CASE("Mean, median, and max of a small sample") {
    const std::vector<double> samples{4.0, 1.0, 3.0, 2.0, 5.0};

    const auto summary = summarize(samples);

    CHECK(summary.count == 5);
    CHECK(summary.mean == Catch::Approx(3.0));
    CHECK(summary.p50 == Catch::Approx(3.0));
    CHECK(summary.max == Catch::Approx(5.0));
}

TEST_CASE("p99 picks the slow tail, not the average") {
    std::vector<double> samples(100, 1.0);
    samples[10] = 50.0; // 2% of frames hitch
    samples[42] = 50.0;

    const auto summary = summarize(samples);

    CHECK(summary.p50 == Catch::Approx(1.0));
    CHECK(summary.p99 == Catch::Approx(50.0));
}

TEST_CASE("A single hitch in 100 frames shows in max but not in p99") {
    std::vector<double> samples(100, 1.0);
    samples[42] = 50.0;

    const auto summary = summarize(samples);

    CHECK(summary.p99 == Catch::Approx(1.0));
    CHECK(summary.max == Catch::Approx(50.0));
}

TEST_CASE("Summarizing does not reorder the caller's samples") {
    const std::vector<double> samples{3.0, 1.0, 2.0};

    (void)summarize(samples);

    CHECK(samples[0] == 3.0);
}
