#pragma once

#include <phantasm/iso/tile_map.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace phantasm::iso {

enum class FogState { Unseen, Explored, Visible };

// Per-player fog. Only revealed 32x32 chunks are stored; visibility can be
// accumulated from several viewers before begin_frame() clears the last view.
class TileFog {
public:
    void set_opaque(unsigned char tile, bool opaque) noexcept { opaque_[tile] = opaque; }
    [[nodiscard]] bool is_opaque(unsigned char tile) const noexcept { return opaque_[tile]; }
    void begin_frame() noexcept;
    void add_viewer(const TileMap& map, TileCoord viewer, int radius);
    [[nodiscard]] FogState state(TileCoord tile) const noexcept;
    [[nodiscard]] bool has_line_of_sight(const TileMap& map, TileCoord from,
                                         TileCoord to) const noexcept;

private:
    struct Key {
        int x, y, level;
        bool operator==(const Key&) const noexcept = default;
    };
    struct Hash {
        std::size_t operator()(Key key) const noexcept;
    };
    struct Chunk {
        std::array<std::uint64_t, 16> explored{};
        std::array<std::uint64_t, 16> visible{};
        bool active = false;
    };
    void reveal(TileCoord tile);
    void cast(const TileMap& map, TileCoord viewer, int radius, int row,
              float start, float end, int xx, int xy, int yx, int yy);
    [[nodiscard]] static bool contains(const TileMap& map, TileCoord tile) noexcept;

    std::array<bool, 256> opaque_{};
    std::unordered_map<Key, Chunk, Hash> chunks_;
    std::vector<Key> active_;
};

} // namespace phantasm::iso
