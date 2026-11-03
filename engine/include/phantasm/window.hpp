#pragma once

#include <string>

// Forward declaration: we only store a pointer, so users of this header
// don't need to include (or even know about) SDL.
struct SDL_Window;

namespace phantasm {

struct WindowConfig {
    std::string title = "Phantasm";
    int width = 1280;
    int height = 720;
};

// Owns one OS window (RAII): created in the constructor, destroyed in the
// destructor. Not copyable: two owners would destroy the same window twice.
class Window {
public:
    // Throws std::runtime_error if the window cannot be created.
    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

private:
    SDL_Window* handle_;
};

} // namespace phantasm
