#include <phantasm/task_pool.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

using phantasm::TaskPool;

TEST_CASE("Parallel ranges cover each item exactly once") {
    TaskPool pool{3};
    std::vector<int> visits(1003);
    pool.parallel_for(visits.size(), 17, [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            ++visits[i];
        }
    });

    for (const int count : visits) {
        CHECK(count == 1);
    }
}

TEST_CASE("Task pool propagates errors and can be reused") {
    TaskPool pool{2};
    CHECK_THROWS_AS(pool.parallel_for(100, 10, [](std::size_t begin, std::size_t) {
                        if (begin == 0) {
                            throw std::runtime_error("update failed");
                        }
                    }),
                    std::runtime_error);

    std::vector<int> updated(37);
    pool.parallel_for(37, 5, [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            ++updated[i];
        }
    });
    for (const int count : updated) {
        CHECK(count == 1);
    }
}

TEST_CASE("Zero-worker and small ranges run on the caller") {
    TaskPool pool{0};
    std::size_t total = 0;
    pool.parallel_for(9, 4, [&](std::size_t begin, std::size_t end) { total += end - begin; });
    CHECK(total == 9);
    CHECK_THROWS_AS(pool.parallel_for(1, 0, [](std::size_t, std::size_t) {}),
                    std::invalid_argument);
}

TEST_CASE("Worker exceptions reach the caller") {
    TaskPool pool{2};
    const auto caller = std::this_thread::get_id();
    std::atomic<bool> worker_entered{false};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};

    CHECK_THROWS_AS(pool.parallel_for(100, 1, [&](std::size_t, std::size_t) {
                        if (std::this_thread::get_id() != caller) {
                            worker_entered.store(true);
                            throw std::runtime_error("worker failed");
                        }
                        while (!worker_entered.load()) {
                            if (std::chrono::steady_clock::now() >= deadline) {
                                throw std::runtime_error("worker did not run");
                            }
                            std::this_thread::yield();
                        }
                    }),
                    std::runtime_error);
    CHECK(worker_entered.load());
}
