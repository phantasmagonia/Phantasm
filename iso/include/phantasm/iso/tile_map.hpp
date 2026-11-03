#pragma once

#include <phantasm/iso/iso_grid.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace phantasm::iso {

// Character grid per floor level. The engine stores characters; games decide
// what they mean. Text format:
//
//   level 0
//   #####
//   #...#
//   level 1
//   ...
class TileMap {
public:
    static constexpr char kEmpty = ' ';

    // Throws std::runtime_error (with the line number) on malformed input.
    [[nodiscard]] static TileMap parse(std::string_view text);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] int level_count() const noexcept { return static_cast<int>(levels_.size()); }

    // Returns kEmpty outside the map.
    [[nodiscard]] char at(TileCoord tile) const noexcept;
    [[nodiscard]] std::vector<TileCoord> find(char tile) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::vector<std::string>> levels_; // [level][row]
};

} // namespace phantasm::iso
