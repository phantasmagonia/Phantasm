#pragma once

#include <phantasm/color.hpp>

struct SDL_GPUDevice;
struct SDL_Window;

namespace phantasm {

class Window;

class Renderer {
public:
    explicit Renderer(Window& window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    // Returns false if nothing was presented (e.g. window minimized).
    bool draw_frame(Color clear_color);

private:
    SDL_GPUDevice* device_;
    SDL_Window* window_;
};

} // namespace phantasm
