#pragma once

#include <phantasm/geometry.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace phantasm {

// Indexes object bounds in world-space chunks. Games keep ownership of objects
// and use returned IDs to submit visible sprites to Renderer::draw_sprite.
class ChunkIndex {
public:
    explicit ChunkIndex(float chunk_size);

    void upsert(std::uint32_t id, Rect bounds);
    [[nodiscard]] bool erase(std::uint32_t id);
    [[nodiscard]] std::vector<std::uint32_t> query(Rect view) const;
    [[nodiscard]] std::size_t size() const noexcept { return objects_.size(); }

private:
    struct ChunkCoord {
        std::int64_t x;
        std::int64_t y;
        bool operator==(const ChunkCoord&) const = default;
    };
    struct ChunkHash {
        [[nodiscard]] std::size_t operator()(ChunkCoord coord) const noexcept;
    };

    [[nodiscard]] ChunkCoord first_chunk(Rect rect) const noexcept;
    [[nodiscard]] ChunkCoord last_chunk(Rect rect) const noexcept;
    void remove_from_chunks(std::uint32_t id, Rect bounds);

    float chunk_size_;
    std::unordered_map<std::uint32_t, Rect> objects_;
    std::unordered_map<ChunkCoord, std::vector<std::uint32_t>, ChunkHash> chunks_;
};

} // namespace phantasm
