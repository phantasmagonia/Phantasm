#include <phantasm/task_pool.hpp>

#include <stdexcept>

namespace phantasm {

TaskPool::TaskPool(std::size_t worker_count) {
    workers_.reserve(worker_count);
    try {
        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    } catch (...) {
        {
            std::lock_guard lock{mutex_};
            stopping_ = true;
        }
        work_cv_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
        throw;
    }
}

TaskPool::~TaskPool() {
    {
        std::lock_guard lock{mutex_};
        stopping_ = true;
    }
    work_cv_.notify_all();
    for (auto& worker : workers_) {
        worker.join();
    }
}

void TaskPool::parallel_for(std::size_t count, std::size_t grain,
                            std::function<void(std::size_t, std::size_t)> task) {
    if (grain == 0 || !task) {
        throw std::invalid_argument("parallel_for requires a task and a positive grain");
    }
    std::lock_guard call_lock{call_mutex_};
    if (count == 0) {
        return;
    }
    if (workers_.empty() || count <= grain) {
        for (std::size_t begin = 0; begin < count;) {
            const std::size_t end = grain >= count - begin ? count : begin + grain;
            task(begin, end);
            begin = end;
        }
        return;
    }

    {
        std::lock_guard lock{mutex_};
        task_ = std::move(task);
        count_ = count;
        grain_ = grain;
        next_.store(0, std::memory_order_relaxed);
        cancelled_.store(false, std::memory_order_relaxed);
        error_ = nullptr;
        finished_workers_ = 0;
        ++generation_;
    }
    work_cv_.notify_all();
    run_chunks();

    std::unique_lock lock{mutex_};
    done_cv_.wait(lock, [this] { return finished_workers_ == workers_.size(); });
    const auto error = error_;
    task_ = {};
    lock.unlock();
    if (error) {
        std::rethrow_exception(error);
    }
}

void TaskPool::run_chunks() {
    while (!cancelled_.load(std::memory_order_relaxed)) {
        const std::size_t begin = next_.fetch_add(grain_, std::memory_order_relaxed);
        if (begin >= count_) {
            return;
        }
        try {
            const std::size_t end = grain_ >= count_ - begin ? count_ : begin + grain_;
            task_(begin, end);
        } catch (...) {
            if (!cancelled_.exchange(true, std::memory_order_relaxed)) {
                std::lock_guard lock{mutex_};
                error_ = std::current_exception();
            }
            return;
        }
    }
}

void TaskPool::worker_loop() {
    std::size_t observed = 0;
    while (true) {
        std::unique_lock lock{mutex_};
        work_cv_.wait(lock, [this, &observed] {
            return stopping_ || generation_ != observed;
        });
        if (stopping_) {
            return;
        }
        observed = generation_;
        lock.unlock();
        run_chunks();
        lock.lock();
        if (++finished_workers_ == workers_.size()) {
            done_cv_.notify_one();
        }
    }
}

} // namespace phantasm
