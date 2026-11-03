#pragma once

#include <phantasm/color.hpp>
#include <phantasm/geometry.hpp>
#include <phantasm/texture.hpp>

#include <string>

struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPURenderPass;
struct SDL_GPUSampler;
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

    // Loads a PNG or JPEG file. Throws std::runtime_error on failure.
    [[nodiscard]] Texture load_texture(const std::string& path);

    // Returns false if this frame can't be drawn (e.g. window minimized).
    // end_frame() must be called either way.
    bool begin_frame(Color clear_color);
    // Destination is in pixels, origin top-left. No-op outside an active frame.
    void draw_sprite(const Texture& texture, Rect destination);
    void end_frame();

private:
    void create_sprite_pipeline();
    void destroy() noexcept;

    SDL_GPUDevice* device_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GPUGraphicsPipeline* sprite_pipeline_ = nullptr;
    SDL_GPUSampler* sampler_ = nullptr;

    SDL_GPUCommandBuffer* commands_ = nullptr;
    SDL_GPURenderPass* pass_ = nullptr;
    Vec2 viewport_{};
};

} // namespace phantasm
