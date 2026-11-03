#pragma once

#include <string>
#include <string_view>

namespace phantasm {

// Resolves a path relative to the executable's folder (where shaders/ and assets/ live).
[[nodiscard]] std::string asset_path(std::string_view relative);

} // namespace phantasm
