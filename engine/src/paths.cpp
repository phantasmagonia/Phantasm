#include <phantasm/paths.hpp>

#include <SDL3/SDL.h>

#include <memory>
#include <stdexcept>

namespace phantasm {

std::string asset_path(std::string_view relative) {
    const char* base = SDL_GetBasePath();
    std::string path = base != nullptr ? base : "";
    path += relative;
    return path;
}

std::string read_text_file(const std::string& path) {
    std::size_t size = 0;
    const std::unique_ptr<void, decltype(&SDL_free)> data{SDL_LoadFile(path.c_str(), &size),
                                                          &SDL_free};
    if (!data) {
        throw std::runtime_error("Reading " + path + " failed: " + SDL_GetError());
    }
    return std::string{static_cast<const char*>(data.get()), size};
}

} // namespace phantasm
