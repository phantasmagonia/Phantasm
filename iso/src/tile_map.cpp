#include <phantasm/iso/tile_map.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace phantasm::iso {

namespace {

constexpr std::string_view kLevelHeader = "level ";

[[noreturn]] void fail(int line, const std::string& message) {
    throw std::runtime_error("Map line " + std::to_string(line) + ": " + message);
}

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        lines.push_back(line);
        if (end == std::string_view::npos) {
            break;
        }
        text.remove_prefix(end + 1);
    }
    return lines;
}

int parse_level_number(std::string_view line, int line_number) {
    const std::string_view digits = line.substr(kLevelHeader.size());
    int level = -1;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), level);
    if (error != std::errc{} || end != digits.data() + digits.size()) {
        fail(line_number, "invalid level header '" + std::string{line} + "'");
    }
    return level;
}

} // namespace

TileMap TileMap::parse(std::string_view text) {
    TileMap map;
    const std::vector<std::string_view> lines = split_lines(text);

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string_view line = lines[i];
        const int line_number = static_cast<int>(i) + 1;

        if (line.starts_with(kLevelHeader)) {
            const int level = parse_level_number(line, line_number);
            if (level != map.level_count()) {
                fail(line_number, "expected level " + std::to_string(map.level_count()) +
                                      ", got level " + std::to_string(level));
            }
            map.levels_.emplace_back();
            continue;
        }
        if (map.levels_.empty()) {
            fail(line_number, "tile row before the first 'level N' header");
        }
        map.levels_.back().emplace_back(line);
    }

    for (const auto& rows : map.levels_) {
        map.height_ = std::max(map.height_, static_cast<int>(rows.size()));
        for (const std::string& row : rows) {
            map.width_ = std::max(map.width_, static_cast<int>(row.size()));
        }
    }
    if (map.width_ == 0 || map.height_ == 0) {
        throw std::runtime_error("Map has no tiles");
    }

    for (auto& rows : map.levels_) {
        rows.resize(static_cast<std::size_t>(map.height_));
        for (std::string& row : rows) {
            row.resize(static_cast<std::size_t>(map.width_), kEmpty);
        }
    }
    return map;
}

TileMap TileMap::blank(int width, int height, int levels) {
    if (width <= 0 || height <= 0 || levels <= 0) {
        throw std::invalid_argument("Map dimensions and level count must be positive");
    }
    TileMap map;
    map.width_ = width;
    map.height_ = height;
    map.levels_.resize(static_cast<std::size_t>(levels),
                       std::vector<std::string>(static_cast<std::size_t>(height),
                                                std::string(static_cast<std::size_t>(width), kEmpty)));
    return map;
}

std::string TileMap::serialize() const {
    if (levels_.empty()) {
        throw std::runtime_error("Cannot serialize an empty map");
    }
    std::string text;
    for (int level = 0; level < level_count(); ++level) {
        text += "level " + std::to_string(level) + '\n';
        for (const std::string& row : levels_[static_cast<std::size_t>(level)]) {
            if (row.starts_with(kLevelHeader)) {
                throw std::runtime_error("Tile row begins with reserved 'level ' header");
            }
            text += row;
            text += '\n';
        }
    }
    return text;
}

void TileMap::set(TileCoord tile, char value) {
    if (tile.level < 0 || tile.level >= level_count() || tile.x < 0 || tile.x >= width_ ||
        tile.y < 0 || tile.y >= height_) {
        throw std::out_of_range("Tile coordinate outside map");
    }
    if (value == '\n' || value == '\r') {
        throw std::invalid_argument("Tile character cannot be a line break");
    }
    levels_[static_cast<std::size_t>(tile.level)][static_cast<std::size_t>(tile.y)]
           [static_cast<std::size_t>(tile.x)] = value;
}

char TileMap::at(TileCoord tile) const noexcept {
    if (tile.level < 0 || tile.level >= level_count() || tile.x < 0 || tile.x >= width_ ||
        tile.y < 0 || tile.y >= height_) {
        return kEmpty;
    }
    const auto& rows = levels_[static_cast<std::size_t>(tile.level)];
    return rows[static_cast<std::size_t>(tile.y)][static_cast<std::size_t>(tile.x)];
}

std::vector<TileCoord> TileMap::find(char tile) const {
    std::vector<TileCoord> found;
    for (int level = 0; level < level_count(); ++level) {
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                if (at(TileCoord{x, y, level}) == tile) {
                    found.push_back(TileCoord{x, y, level});
                }
            }
        }
    }
    return found;
}

} // namespace phantasm::iso
