#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace phantasm {

// Reuses worker threads across updates. Each callback owns its [begin, end)
// range; callbacks must synchronize shared state. Calls cannot be nested on the
// same pool, and captured data must stay alive until parallel_for returns.
class TaskPool {
public:
    explicit TaskPool(std::size_t worker_count);
    ~TaskPool();

    TaskPool(const TaskPool&) = delete;
    TaskPool& operator=(const TaskPool&) = delete;
    TaskPool(TaskPool&&) = delete;
    TaskPool& operator=(TaskPool&&) = delete;

    // Blocks until all ranges finish. Calls on the same pool are serialized.
    void parallel_for(std::size_t count, std::size_t grain,
                      std::function<void(std::size_t, std::size_t)> task);
    [[nodiscard]] std::size_t worker_count() const noexcept { return workers_.size(); }

private:
    void worker_loop();
    void run_chunks();

    std::mutex call_mutex_;
    std::mutex mutex_;
    std::condition_variable work_cv_;
    std::condition_variable done_cv_;
    std::function<void(std::size_t, std::size_t)> task_;
    std::size_t count_ = 0;
    std::size_t grain_ = 1;
    std::size_t generation_ = 0;
    std::size_t finished_workers_ = 0;
    bool stopping_ = false;
    std::atomic<std::size_t> next_{0};
    std::atomic<bool> cancelled_{false};
    std::exception_ptr error_;
    std::vector<std::thread> workers_;
};

} // namespace phantasm
