#pragma once

#include <phantasm/camera.hpp>
#include <phantasm/color.hpp>
#include <phantasm/geometry.hpp>
#include <phantasm/sprite.hpp>
#include <phantasm/sprite_batch.hpp>
#include <phantasm/texture.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct SDL_GPUBuffer;
struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
struct SDL_GPUTexture;
struct SDL_GPUTransferBuffer;
struct SDL_Window;

namespace phantasm {

class Window;

struct RenderStats {
    std::uint32_t sprites = 0;
    std::uint32_t draw_calls = 0;
    double cpu_ms = 0.0; // sort + upload + command recording
};

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
    // Creates a texture from tightly packed RGBA8 pixels, rows top to bottom.
    // Throws std::invalid_argument on bad sizes, std::runtime_error on GPU failure.
    [[nodiscard]] Texture create_texture(int width, int height, std::span<const std::byte> rgba);

    void set_camera(const Camera& camera) noexcept { camera_ = camera; }
    [[nodiscard]] const Camera& camera() const noexcept { return camera_; }
    [[nodiscard]] const RenderStats& last_stats() const noexcept { return stats_; }

    // Vsync off uses immediate (or mailbox) presentation when the backend supports it.
    // Returns false if the requested mode is unavailable.
    bool set_vsync(bool enabled);

    // Returns false if this frame can't be drawn (e.g. window minimized).
    // end_frame() must be called either way.
    bool begin_frame(Color clear_color);
    // Queues a sprite; sorting and drawing happen in end_frame().
    // The texture must stay alive until end_frame().
    void draw_sprite(const Texture& texture, const Sprite& sprite);
    void end_frame();

private:
    void create_sprite_pipeline();
    void ensure_instance_capacity(std::uint32_t count);
    void upload_instances();
    void record_draws(const std::vector<DrawRun>& runs);
    void destroy() noexcept;

    SDL_GPUDevice* device_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GPUGraphicsPipeline* sprite_pipeline_ = nullptr;
    SDL_GPUSampler* sampler_ = nullptr;
    SDL_GPUBuffer* instance_buffer_ = nullptr;
    SDL_GPUTransferBuffer* instance_transfer_ = nullptr;
    std::uint32_t instance_capacity_ = 0;

    SDL_GPUCommandBuffer* commands_ = nullptr;
    SDL_GPUTexture* frame_target_ = nullptr;
    Color clear_color_{};
    Vec2 viewport_{};
    Camera camera_{};
    std::vector<SpriteDraw> sprites_;
    RenderStats stats_{};
};

} // namespace phantasm
