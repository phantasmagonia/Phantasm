#include <phantasm/window.hpp>

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace phantasm {

Window::Window(const WindowConfig& config)
    : handle_{SDL_CreateWindow(config.title.c_str(), config.width, config.height,
                               SDL_WINDOW_RESIZABLE |
                                   (config.hidden ? SDL_WINDOW_HIDDEN : SDL_WindowFlags{0}))} {
    if (handle_ == nullptr) {
        throw std::runtime_error(std::string{"SDL_CreateWindow failed: "} + SDL_GetError());
    }
}

float Window::pixel_density() const noexcept {
    const float density = SDL_GetWindowPixelDensity(handle_);
    return density > 0.0F ? density : 1.0F;
}

Window::~Window() {
    SDL_DestroyWindow(handle_);
}

} // namespace phantasm
