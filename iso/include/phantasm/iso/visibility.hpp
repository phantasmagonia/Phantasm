#pragma once

#include <phantasm/iso/iso_grid.hpp>

#include <array>

namespace phantasm::iso {

enum class TileRole { Floor, Wall, Roof };

// Pure rendering policy. Games classify tile characters and decide when a viewer is indoors.
class FloorVisibility {
public:
    struct Focus {
        GridPos position{};
        bool indoors = false;
    };

    void set_role(unsigned char tile, TileRole role) noexcept { roles_[tile] = role; }
    [[nodiscard]] TileRole role(unsigned char tile) const noexcept { return roles_[tile]; }

    // Alpha for a tile's sprite; zero means it need not be submitted to the renderer.
    // Indoors: floors above the viewer and roofs on the viewer's floor are hidden.
    // Always: walls on the viewer's floor in front of the viewer (toward the camera) within
    // wall_fade_radius grid cells fade to wall_opacity.
    [[nodiscard]] float opacity(TileCoord tile, unsigned char value, Focus focus,
                                float wall_fade_radius = 1.5F,
                                float wall_opacity = 0.35F) const noexcept;

private:
    std::array<TileRole, 256> roles_{};
};

} // namespace phantasm::iso
