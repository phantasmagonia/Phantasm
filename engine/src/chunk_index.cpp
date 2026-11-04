#include <phantasm/chunk_index.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace phantasm {

namespace {

bool valid(Rect rect) noexcept {
    return std::isfinite(rect.x) && std::isfinite(rect.y) && std::isfinite(rect.w) &&
           std::isfinite(rect.h) && rect.w > 0.0F && rect.h > 0.0F &&
           std::isfinite(static_cast<double>(rect.x) + rect.w) &&
           std::isfinite(static_cast<double>(rect.y) + rect.h);
}

void validate_chunk_range(Rect rect, float chunk_size) {
    const double left = static_cast<double>(rect.x) / chunk_size;
    const double top = static_cast<double>(rect.y) / chunk_size;
    const double right = (static_cast<double>(rect.x) + rect.w) / chunk_size;
    const double bottom = (static_cast<double>(rect.y) + rect.h) / chunk_size;
    const double limit = static_cast<double>(std::numeric_limits<std::int64_t>::max()) / 4.0;
    if (std::abs(left) >= limit || std::abs(top) >= limit ||
        std::abs(right) >= limit || std::abs(bottom) >= limit) {
        throw std::invalid_argument("bounds are outside supported chunk coordinates");
    }
    const double columns = std::ceil(right) - std::floor(left);
    const double rows = std::ceil(bottom) - std::floor(top);
    if (columns * rows > 1'000'000.0) {
        throw std::length_error("bounds span too many chunks");
    }
}

} // namespace

ChunkIndex::ChunkIndex(float chunk_size) : chunk_size_(chunk_size) {
    if (!std::isfinite(chunk_size) || chunk_size <= 0.0F) {
        throw std::invalid_argument("chunk size must be positive and finite");
    }
}

std::size_t ChunkIndex::ChunkHash::operator()(ChunkCoord coord) const noexcept {
    const auto x = static_cast<std::uint64_t>(coord.x);
    const auto y = static_cast<std::uint64_t>(coord.y);
    return static_cast<std::size_t>(x * 0x9E3779B97F4A7C15ULL ^
                                    (y + 0x9E3779B97F4A7C15ULL + (x << 6) + (x >> 2)));
}

ChunkIndex::ChunkCoord ChunkIndex::first_chunk(Rect rect) const noexcept {
    return {static_cast<std::int64_t>(std::floor(static_cast<double>(rect.x) / chunk_size_)),
            static_cast<std::int64_t>(std::floor(static_cast<double>(rect.y) / chunk_size_))};
}

ChunkIndex::ChunkCoord ChunkIndex::last_chunk(Rect rect) const noexcept {
    return {static_cast<std::int64_t>(std::ceil((static_cast<double>(rect.x) + rect.w) /
                                                 chunk_size_) - 1.0),
            static_cast<std::int64_t>(std::ceil((static_cast<double>(rect.y) + rect.h) /
                                                 chunk_size_) - 1.0)};
}

void ChunkIndex::remove_from_chunks(std::uint32_t id, Rect bounds) {
    const auto first = first_chunk(bounds);
    const auto last = last_chunk(bounds);
    for (auto y = first.y; y <= last.y; ++y) {
        for (auto x = first.x; x <= last.x; ++x) {
            const ChunkCoord coord{x, y};
            auto bucket = chunks_.find(coord);
            if (bucket == chunks_.end()) {
                continue;
            }
            auto& ids = bucket->second;
            ids.erase(std::remove_if(ids.begin(), ids.end(),
                                     [id](const BucketEntry& entry) { return entry.id == id; }),
                      ids.end());
            if (ids.empty()) {
                chunks_.erase(bucket);
            }
        }
    }
}

void ChunkIndex::upsert(std::uint32_t id, Rect bounds) {
    if (!valid(bounds)) {
        throw std::invalid_argument("object bounds must have positive finite area");
    }
    validate_chunk_range(bounds, chunk_size_);
    if (auto old = objects_.find(id); old != objects_.end()) {
        remove_from_chunks(id, old->second);
    }
    objects_[id] = bounds;
    const auto first = first_chunk(bounds);
    const auto last = last_chunk(bounds);
    for (auto y = first.y; y <= last.y; ++y) {
        for (auto x = first.x; x <= last.x; ++x) {
            chunks_[ChunkCoord{x, y}].push_back(BucketEntry{id, bounds});
        }
    }
}

bool ChunkIndex::erase(std::uint32_t id) {
    auto object = objects_.find(id);
    if (object == objects_.end()) {
        return false;
    }
    remove_from_chunks(id, object->second);
    objects_.erase(object);
    return true;
}

std::vector<std::uint32_t> ChunkIndex::query(Rect view) const {
    std::vector<std::uint32_t> result;
    if (!valid(view)) {
        return result;
    }
    validate_chunk_range(view, chunk_size_);
    std::unordered_set<std::uint32_t> seen;
    const auto first = first_chunk(view);
    const auto last = last_chunk(view);
    for (auto y = first.y; y <= last.y; ++y) {
        for (auto x = first.x; x <= last.x; ++x) {
            const auto bucket = chunks_.find(ChunkCoord{x, y});
            if (bucket == chunks_.end()) {
                continue;
            }
            for (const auto& entry : bucket->second) {
                if (intersects(entry.bounds, view) && seen.insert(entry.id).second) {
                    result.push_back(entry.id);
                }
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace phantasm
