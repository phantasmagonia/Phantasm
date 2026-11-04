#include <phantasm/pixel_format.hpp>

#include <stdexcept>
#include <utility>

namespace phantasm {

void bgra_to_rgba(std::span<std::byte> pixels) {
    if (pixels.size() % 4 != 0) {
        throw std::invalid_argument("bgra_to_rgba: size must be a multiple of 4");
    }
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        std::swap(pixels[i], pixels[i + 2]);
    }
}

} // namespace phantasm
