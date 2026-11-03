#pragma once

// Meaning of the characters in Scavenger's .map files.
namespace scavenger::tiles {

inline constexpr char kGrass = ',';
inline constexpr char kFloor = '.';
inline constexpr char kWall = '#';
inline constexpr char kDoor = 'D';
inline constexpr char kSupply = 'S';
inline constexpr char kPlayerStart = 'P';

[[nodiscard]] constexpr bool is_indoor_floor(char tile) noexcept {
    return tile == kFloor || tile == kDoor || tile == kSupply;
}

} // namespace scavenger::tiles
