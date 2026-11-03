#pragma once

#include <phantasm/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace phantasm {

struct ChunkCoord {
    std::int64_t x = 0;
    std::int64_t y = 0;
    auto operator<=>(const ChunkCoord&) const = default;
};

// Owns game-provided chunk data near a view. The loader is synchronous; use a
// small load budget or provide already prepared data to avoid frame stalls.
template <typename T>
class ChunkStream {
public:
    explicit ChunkStream(float chunk_size, int prefetch_chunks = 0)
        : chunk_size_(chunk_size), prefetch_chunks_(prefetch_chunks) {
        if (!std::isfinite(chunk_size) || chunk_size <= 0.0F || prefetch_chunks < 0) {
            throw std::invalid_argument("invalid chunk stream settings");
        }
    }

    // Loads at most max_loads missing chunks, nearest the view center first.
    // Chunks outside the view plus prefetch margin are unloaded immediately.
    template <typename Loader>
    std::size_t update(Rect view, std::size_t max_loads, Loader&& loader) {
        if (view.w <= 0.0F || view.h <= 0.0F) {
            return 0;
        }
        if (!std::isfinite(view.x) || !std::isfinite(view.y) ||
            !std::isfinite(view.w) || !std::isfinite(view.h)) {
            throw std::invalid_argument("view must have finite bounds");
        }
        const double limit = static_cast<double>(std::numeric_limits<std::int64_t>::max()) / 4.0;
        const double right = (static_cast<double>(view.x) + view.w) / chunk_size_;
        const double bottom = (static_cast<double>(view.y) + view.h) / chunk_size_;
        if (std::abs(static_cast<double>(view.x) / chunk_size_) >= limit ||
            std::abs(static_cast<double>(view.y) / chunk_size_) >= limit ||
            std::abs(right) >= limit || std::abs(bottom) >= limit) {
            throw std::invalid_argument("view is outside supported chunk coordinates");
        }

        const auto min_x = chunk_floor(view.x) - prefetch_chunks_;
        const auto min_y = chunk_floor(view.y) - prefetch_chunks_;
        const auto max_x = chunk_last(view.x, view.w) + prefetch_chunks_;
        const auto max_y = chunk_last(view.y, view.h) + prefetch_chunks_;
        if (static_cast<double>(max_x - min_x + 1) *
                static_cast<double>(max_y - min_y + 1) > 1'000'000.0) {
            throw std::length_error("view spans too many chunks");
        }
        const double center_x = (static_cast<double>(view.x) + view.w * 0.5) / chunk_size_;
        const double center_y = (static_cast<double>(view.y) + view.h * 0.5) / chunk_size_;

        for (auto it = loaded_.begin(); it != loaded_.end();) {
            const auto coord = it->first;
            if (coord.x < min_x || coord.x > max_x || coord.y < min_y || coord.y > max_y) {
                it = loaded_.erase(it);
            } else {
                ++it;
            }
        }
        if (max_loads == 0) {
            return 0;
        }

        std::vector<ChunkCoord> missing;
        for (auto y = min_y; y <= max_y; ++y) {
            for (auto x = min_x; x <= max_x; ++x) {
                const ChunkCoord coord{x, y};
                if (!loaded_.contains(coord)) {
                    missing.push_back(coord);
                }
            }
        }
        std::sort(missing.begin(), missing.end(), [=](ChunkCoord a, ChunkCoord b) {
            const auto distance = [=](ChunkCoord coord) {
                const double dx = static_cast<double>(coord.x) + 0.5 - center_x;
                const double dy = static_cast<double>(coord.y) + 0.5 - center_y;
                return dx * dx + dy * dy;
            };
            const double da = distance(a);
            const double db = distance(b);
            return da == db ? a < b : da < db;
        });

        const std::size_t count = std::min(max_loads, missing.size());
        for (std::size_t i = 0; i < count; ++i) {
            loaded_.emplace(missing[i], loader(missing[i]));
        }
        return count;
    }

    [[nodiscard]] T* find(ChunkCoord coord) noexcept {
        auto it = loaded_.find(coord);
        return it == loaded_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const T* find(ChunkCoord coord) const noexcept {
        auto it = loaded_.find(coord);
        return it == loaded_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const std::map<ChunkCoord, T>& loaded() const noexcept { return loaded_; }
    [[nodiscard]] std::size_t size() const noexcept { return loaded_.size(); }
    void clear() noexcept { loaded_.clear(); }

private:
    [[nodiscard]] std::int64_t chunk_floor(float coord) const noexcept {
        return static_cast<std::int64_t>(std::floor(static_cast<double>(coord) / chunk_size_));
    }

    [[nodiscard]] std::int64_t chunk_last(float start, float length) const noexcept {
        return static_cast<std::int64_t>(
            std::ceil((static_cast<double>(start) + length) / chunk_size_) - 1.0);
    }

    float chunk_size_;
    int prefetch_chunks_;
    std::map<ChunkCoord, T> loaded_;
};

} // namespace phantasm
