#pragma once

#include <string>
#include <string_view>

namespace phantasm {

// Resolves a path relative to the executable's folder (where shaders/ and assets/ live).
[[nodiscard]] std::string asset_path(std::string_view relative);

// Reads a whole file as text. Throws std::runtime_error on failure.
[[nodiscard]] std::string read_text_file(const std::string& path);

} // namespace phantasm
