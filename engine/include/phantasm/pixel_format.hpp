#pragma once

#include <cstddef>
#include <span>

namespace phantasm {

// Swaps the red and blue channels of tightly packed 4-byte pixels, in place.
// Throws std::invalid_argument if the size is not a multiple of 4.
void bgra_to_rgba(std::span<std::byte> pixels);

} // namespace phantasm
