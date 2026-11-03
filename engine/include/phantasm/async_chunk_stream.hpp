#pragma once

#include <phantasm/chunk_stream.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace phantasm {

// Loads game-owned chunk data on one worker. update/poll/find run on the game
// thread; the loader must not call SDL_GPU or touch game-thread-only state.
template <typename T>
class AsyncChunkStream {
public:
    struct Failure {
        ChunkCoord coord;
        std::exception_ptr error;
    };

    AsyncChunkStream(float chunk_size, int prefetch_chunks,
                     std::function<T(ChunkCoord)> loader)
        : chunk_size_{chunk_size}, desired_{chunk_size, prefetch_chunks},
          loader_{std::move(loader)},
          worker_{[this](std::stop_token stop) { work(stop); }} {}

    ~AsyncChunkStream() {
        worker_.request_stop();
        cv_.notify_all();
        worker_.join();
    }

    AsyncChunkStream(const AsyncChunkStream&) = delete;
    AsyncChunkStream& operator=(const AsyncChunkStream&) = delete;
    AsyncChunkStream(AsyncChunkStream&&) = delete;
    AsyncChunkStream& operator=(AsyncChunkStream&&) = delete;

    // Limits all queued, running, and completed-but-unpolled requests together.
    std::size_t update(Rect view, std::size_t max_pending) {
        desired_.update(view, 1'000'000, [](ChunkCoord coord) { return coord; });
        std::vector<ChunkCoord> missing;
        {
            std::lock_guard lock{mutex_};
            desired_coords_.clear();
            for (const auto& [coord, value] : desired_.loaded()) {
                (void)value;
                desired_coords_.insert(coord);
            }
            for (auto it = queue_.begin(); it != queue_.end();) {
                if (!desired_coords_.contains(*it)) {
                    pending_.erase(*it);
                    it = queue_.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = ready_.begin(); it != ready_.end();) {
                if (!desired_coords_.contains(it->coord)) {
                    pending_.erase(it->coord);
                    it = ready_.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = loaded_.begin(); it != loaded_.end();) {
                it = desired_coords_.contains(it->first) ? std::next(it) : loaded_.erase(it);
            }
            for (auto it = failed_.begin(); it != failed_.end();) {
                it = desired_coords_.contains(*it) ? std::next(it) : failed_.erase(it);
            }
            for (const auto& [coord, value] : desired_.loaded()) {
                (void)value;
                if (!loaded_.contains(coord) && !pending_.contains(coord) &&
                    !failed_.contains(coord)) {
                    missing.push_back(coord);
                }
            }
        }
        const double center_x = (static_cast<double>(view.x) + view.w * 0.5) / chunk_size_;
        const double center_y = (static_cast<double>(view.y) + view.h * 0.5) / chunk_size_;
        std::sort(missing.begin(), missing.end(), [=](ChunkCoord a, ChunkCoord b) {
            const auto distance = [=](ChunkCoord c) {
                const double dx = static_cast<double>(c.x) + 0.5 - center_x;
                const double dy = static_cast<double>(c.y) + 0.5 - center_y;
                return dx * dx + dy * dy;
            };
            const double da = distance(a);
            const double db = distance(b);
            return da == db ? a < b : da < db;
        });
        std::size_t queued = 0;
        {
            std::lock_guard lock{mutex_};
            for (const auto coord : missing) {
                if (pending_.size() >= max_pending) {
                    break;
                }
                pending_.insert(coord);
                queue_.push_back(coord);
                ++queued;
            }
        }
        cv_.notify_all();
        return queued;
    }

    // Moves finished data into the resident map. Failures stay paused until retry().
    std::size_t poll() {
        std::deque<Result> ready;
        {
            std::lock_guard lock{mutex_};
            ready.swap(ready_);
            for (const auto& result : ready) {
                pending_.erase(result.coord);
            }
        }
        std::size_t loaded = 0;
        for (auto& result : ready) {
            if (!desired_coords_.contains(result.coord)) {
                continue;
            }
            if (result.error) {
                failed_.insert(result.coord);
                failures_.push_back({result.coord, result.error});
            } else {
                loaded_.emplace(result.coord, std::move(*result.value));
                ++loaded;
            }
        }
        return loaded;
    }

    [[nodiscard]] bool wait_for_ready(std::chrono::milliseconds timeout) {
        std::unique_lock lock{mutex_};
        return cv_.wait_for(lock, timeout, [this] { return !ready_.empty(); });
    }

    [[nodiscard]] bool retry(ChunkCoord coord) { return failed_.erase(coord) != 0; }

    [[nodiscard]] std::vector<Failure> take_failures() {
        auto result = std::move(failures_);
        failures_.clear();
        return result;
    }

    [[nodiscard]] T* find(ChunkCoord coord) noexcept {
        auto it = loaded_.find(coord);
        return it == loaded_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const std::map<ChunkCoord, T>& loaded() const noexcept { return loaded_; }
    [[nodiscard]] std::size_t size() const noexcept { return loaded_.size(); }

private:
    struct Result {
        ChunkCoord coord;
        std::optional<T> value;
        std::exception_ptr error;
    };

    void work(std::stop_token stop) {
        while (true) {
            ChunkCoord coord;
            {
                std::unique_lock lock{mutex_};
                cv_.wait(lock, [this, &stop] { return stop.stop_requested() || !queue_.empty(); });
                if (stop.stop_requested()) {
                    return;
                }
                coord = queue_.front();
                queue_.pop_front();
                if (!desired_coords_.contains(coord)) {
                    pending_.erase(coord);
                    continue;
                }
            }
            Result result{coord, std::nullopt, nullptr};
            try {
                result.value.emplace(loader_(coord));
            } catch (...) {
                result.error = std::current_exception();
            }
            {
                std::lock_guard lock{mutex_};
                if (desired_coords_.contains(coord)) {
                    ready_.push_back(std::move(result));
                } else {
                    pending_.erase(coord);
                }
            }
            cv_.notify_all();
        }
    }

    float chunk_size_ = 0.0F;
    ChunkStream<ChunkCoord> desired_;
    std::function<T(ChunkCoord)> loader_;
    std::map<ChunkCoord, T> loaded_;
    std::set<ChunkCoord> failed_;
    std::vector<Failure> failures_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::set<ChunkCoord> desired_coords_;
    std::set<ChunkCoord> pending_;
    std::deque<ChunkCoord> queue_;
    std::deque<Result> ready_;
    std::jthread worker_;
};

} // namespace phantasm
