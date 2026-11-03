#include <phantasm/paths.hpp>

#include <SDL3/SDL.h>

namespace phantasm {

std::string asset_path(std::string_view relative) {
    const char* base = SDL_GetBasePath();
    std::string path = base != nullptr ? base : "";
    path += relative;
    return path;
}

} // namespace phantasm
