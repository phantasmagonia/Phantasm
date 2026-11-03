#pragma once

#include <phantasm/geometry.hpp>
#include <phantasm/texture.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace phantasm {

class Renderer;

struct Size {
    int width = 0;
    int height = 0;
};

// Top-left corner of a packed image, in atlas pixels.
struct PackedRect {
    int x = 0;
    int y = 0;
};

struct PackResult {
    int atlas_size = 0; // square, power of two
    std::vector<PackedRect> positions; // same order as the input sizes
};

// Tightly packed RGBA8 pixels, rows top to bottom.
struct ImageView {
    int width = 0;
    int height = 0;
    std::span<const std::byte> rgba;
};

// Packs into the smallest power-of-two square (64..max_size) that fits, keeping
// `padding` pixels between images. Returns nullopt if nothing fits.
// Throws std::invalid_argument on non-positive sizes.
[[nodiscard]] std::optional<PackResult> pack_rects(std::span<const Size> sizes, int max_size,
                                                   int padding);

[[nodiscard]] Rect region_uv(PackedRect position, Size size, int atlas_size) noexcept;

// Copies images into a transparent atlas_size x atlas_size RGBA8 buffer.
[[nodiscard]] std::vector<std::byte> compose_atlas(int atlas_size, std::span<const ImageView> images,
                                                   std::span<const PackedRect> positions);

// Many images in one GPU texture, so sprites using them batch into one draw call.
struct Atlas {
    Texture texture;
    std::vector<Rect> uvs;   // per input image, same order
    std::vector<Size> sizes; // per input image, in pixels
};

// Throws std::runtime_error if the images don't fit max_size.
[[nodiscard]] Atlas build_atlas(Renderer& renderer, std::span<const ImageView> images,
                                int max_size = 4096, int padding = 2);

} // namespace phantasm
