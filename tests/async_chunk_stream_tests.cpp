#include <phantasm/async_chunk_stream.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

using phantasm::AsyncChunkStream;
using phantasm::ChunkCoord;
using phantasm::Rect;

using namespace std::chrono_literals;

TEST_CASE("Background loader publishes move-only chunks on the main thread") {
    const auto main_thread = std::this_thread::get_id();
    std::thread::id loader_thread;
    AsyncChunkStream<std::unique_ptr<int>> stream{32.0F, 0, [&](ChunkCoord coord) {
        loader_thread = std::this_thread::get_id();
        return std::make_unique<int>(static_cast<int>(coord.x) + 7);
    }};

    CHECK(stream.update(Rect{0, 0, 32, 32}, 1) == 1);
    REQUIRE(stream.wait_for_ready(2s));
    CHECK(stream.poll() == 1);
    CHECK(loader_thread != main_thread);
    REQUIRE(stream.find({0, 0}) != nullptr);
    CHECK(**stream.find({0, 0}) == 7);
    CHECK(stream.update(Rect{0, 0, 32, 32}, 1) == 0);
    CHECK(stream.update(Rect{32, 0, 32, 32}, 1) == 1);
    CHECK(stream.find({0, 0}) == nullptr);
    REQUIRE(stream.wait_for_ready(2s));
    CHECK(stream.poll() == 1);
    CHECK(stream.find({1, 0}) != nullptr);
}

TEST_CASE("Background loader discards stale results after camera movement") {
    std::promise<void> started;
    std::promise<void> release;
    auto gate = release.get_future().share();
    AsyncChunkStream<int> stream{32.0F, 0, [&](ChunkCoord coord) {
        if (coord.x == 0) {
            started.set_value();
            gate.wait();
        }
        return static_cast<int>(coord.x);
    }};

    stream.update(Rect{0, 0, 32, 32}, 1);
    const auto start_status = started.get_future().wait_for(2s);
    if (start_status != std::future_status::ready) {
        release.set_value();
        FAIL("background loader did not start");
    }
    stream.update(Rect{64, 0, 32, 32}, 2);
    release.set_value();
    REQUIRE(stream.wait_for_ready(2s));
    CHECK(stream.poll() == 1);
    CHECK(stream.find({0, 0}) == nullptr);
    REQUIRE(stream.find({2, 0}) != nullptr);
    CHECK(*stream.find({2, 0}) == 2);
}

TEST_CASE("Completed stale chunks free request capacity") {
    AsyncChunkStream<int> stream{32.0F, 0,
                                 [](ChunkCoord coord) { return static_cast<int>(coord.x); }};
    stream.update(Rect{0, 0, 32, 32}, 1);
    REQUIRE(stream.wait_for_ready(2s));

    CHECK(stream.update(Rect{32, 0, 32, 32}, 1) == 1);
    CHECK(stream.poll() == 0);
    REQUIRE(stream.wait_for_ready(2s));
    CHECK(stream.poll() == 1);
    REQUIRE(stream.find({1, 0}) != nullptr);
    CHECK(*stream.find({1, 0}) == 1);
}

TEST_CASE("Background load errors are reported and retried explicitly") {
    int attempts = 0;
    AsyncChunkStream<int> stream{32.0F, 0, [&](ChunkCoord) -> int {
        if (++attempts == 1) {
            throw std::runtime_error("load failed");
        }
        return 42;
    }};

    stream.update(Rect{0, 0, 32, 32}, 1);
    REQUIRE(stream.wait_for_ready(2s));
    CHECK(stream.poll() == 0);
    auto failures = stream.take_failures();
    REQUIRE(failures.size() == 1);
    CHECK(failures[0].coord == (ChunkCoord{0, 0}));
    bool got_load_error = false;
    try {
        std::rethrow_exception(failures[0].error);
    } catch (const std::runtime_error&) {
        got_load_error = true;
    }
    CHECK(got_load_error);
    CHECK(stream.update(Rect{0, 0, 32, 32}, 1) == 0);
    CHECK(stream.retry({0, 0}));
    CHECK(stream.update(Rect{0, 0, 32, 32}, 1) == 1);
    REQUIRE(stream.wait_for_ready(2s));
    CHECK(stream.poll() == 1);
    CHECK(*stream.find({0, 0}) == 42);
}
