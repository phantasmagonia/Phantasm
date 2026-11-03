#include <phantasm/window.hpp>

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace phantasm {

Window::Window(const WindowConfig& config)
    : handle_{SDL_CreateWindow(config.title.c_str(), config.width, config.height,
                               SDL_WINDOW_RESIZABLE)} {
    if (handle_ == nullptr) {
        throw std::runtime_error(std::string{"SDL_CreateWindow failed: "} + SDL_GetError());
    }
}

Window::~Window() {
    SDL_DestroyWindow(handle_);
}

} // namespace phantasm
