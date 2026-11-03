#include <phantasm/chunk_index.hpp>

#include <algorithm>
#include <cmath>
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
            ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
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
    if (auto old = objects_.find(id); old != objects_.end()) {
        remove_from_chunks(id, old->second);
    }
    objects_[id] = bounds;
    const auto first = first_chunk(bounds);
    const auto last = last_chunk(bounds);
    for (auto y = first.y; y <= last.y; ++y) {
        for (auto x = first.x; x <= last.x; ++x) {
            chunks_[ChunkCoord{x, y}].push_back(id);
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
    std::unordered_set<std::uint32_t> seen;
    const auto first = first_chunk(view);
    const auto last = last_chunk(view);
    for (auto y = first.y; y <= last.y; ++y) {
        for (auto x = first.x; x <= last.x; ++x) {
            const auto bucket = chunks_.find(ChunkCoord{x, y});
            if (bucket == chunks_.end()) {
                continue;
            }
            for (const auto id : bucket->second) {
                if (seen.insert(id).second && intersects(objects_.at(id), view)) {
                    result.push_back(id);
                }
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace phantasm
