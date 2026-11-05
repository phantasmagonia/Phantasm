#pragma once

#include <string>

struct SDL_Window;

namespace phantasm {

struct WindowConfig {
    std::string title = "Phantasm";
    int width = 1280;
    int height = 720;
    bool hidden = false; // for offscreen rendering, tests, and benchmarks
};

class Window {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] SDL_Window* native_handle() const noexcept { return handle_; }
    // Drawable pixels per window coordinate (2 on most Retina displays when high density is on).
    [[nodiscard]] float pixel_density() const noexcept;

private:
    SDL_Window* handle_;
};

} // namespace phantasm
