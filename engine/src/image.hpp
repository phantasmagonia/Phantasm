#pragma once

#include <cstddef>
#include <memory>
#include <span>

namespace phantasm::detail {

struct ImagePixelsDeleter {
    void operator()(unsigned char* pixels) const noexcept;
};

// Decoded image, always 4 bytes per pixel (RGBA8), rows top to bottom.
struct Image {
    int width = 0;
    int height = 0;
    std::unique_ptr<unsigned char, ImagePixelsDeleter> pixels;
};

// Decodes PNG or JPEG bytes. Throws std::runtime_error on failure.
[[nodiscard]] Image decode_image(std::span<const std::byte> encoded);

} // namespace phantasm::detail
