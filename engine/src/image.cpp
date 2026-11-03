#include "image.hpp"

#include <stb_image.h>

#include <climits>
#include <stdexcept>
#include <string>

namespace phantasm::detail {

namespace {
constexpr int kRgbaChannels = 4;
}

void ImagePixelsDeleter::operator()(unsigned char* pixels) const noexcept {
    stbi_image_free(pixels);
}

Image decode_image(std::span<const std::byte> encoded) {
    if (encoded.size() > static_cast<std::size_t>(INT_MAX)) {
        throw std::runtime_error("Image file too large");
    }
    Image image;
    int channels_in_file = 0;
    image.pixels.reset(stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(encoded.data()),
                                             static_cast<int>(encoded.size()), &image.width,
                                             &image.height, &channels_in_file, kRgbaChannels));
    if (!image.pixels) {
        throw std::runtime_error(std::string{"Image decode failed: "} + stbi_failure_reason());
    }
    return image;
}

} // namespace phantasm::detail
