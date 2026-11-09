#pragma once

#include <phantasm/camera.hpp>
#include <phantasm/color.hpp>
#include <phantasm/geometry.hpp>
#include <phantasm/sprite.hpp>
#include <phantasm/sprite_batch.hpp>
#include <phantasm/texture.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
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
class TaskPool;
class FontFace;
struct TextLayout;

struct RenderStats {
    std::uint32_t sprites = 0; // drawn, after culling
    std::uint32_t culled = 0;  // skipped because they were outside the camera view
    std::uint32_t draw_calls = 0;
    double cpu_ms = 0.0;    // end_frame only: sort + upload + command recording
    double submit_ms = 0.0; // begin_frame to end of end_frame, incl. draw_sprite calls and culling
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
    [[nodiscard]] Texture load_texture(const std::string& path,
                                       TextureFilter filter = TextureFilter::Nearest);
    // Creates a texture from tightly packed RGBA8 pixels, rows top to bottom.
    // Throws std::invalid_argument on bad sizes, std::runtime_error on GPU failure.
    [[nodiscard]] Texture create_texture(int width, int height, std::span<const std::byte> rgba,
                                         TextureFilter filter = TextureFilter::Nearest);

    // Renders a static layer once, in local pixel coordinates, onto a transparent texture.
    // Call between frames; rebuild after edits. The returned texture must outlive its draws.
    [[nodiscard]] Texture cache_layer(const Texture& atlas, std::span<const Sprite> sprites,
                                      int width, int height);

    void set_camera(const Camera& camera) noexcept;
    [[nodiscard]] const Camera& camera() const noexcept { return camera_; }
    // Current drawable-area bounds in world coordinates, set by begin_frame().
    [[nodiscard]] Rect visible_rect() const noexcept { return visible_; }
    // Screen pixels of the drawable area (origin top-left) to world units, using the
    // current camera and the viewport of the last frame.
    [[nodiscard]] Vec2 screen_to_world(Vec2 screen_pixels) const noexcept;
    [[nodiscard]] const RenderStats& last_stats() const noexcept { return stats_; }
    // Borrowed handles for tools that submit their own SDL_GPU passes; never release them.
    [[nodiscard]] SDL_GPUDevice* native_device() const noexcept { return device_; }
    [[nodiscard]] SDL_GPUTexture* offscreen_texture() const noexcept { return offscreen_; }

    // Vsync off uses immediate (or mailbox) presentation when the backend supports it.
    // Returns false if the requested mode is unavailable.
    bool set_vsync(bool enabled);

    // Renders frames into a width x height texture instead of the window (benchmarks,
    // tests, screenshots). Pass 0, 0 to render to the window again.
    void set_offscreen(int width, int height);
    // Waits for the GPU and returns the offscreen image as RGBA8, rows top to bottom.
    // Throws std::logic_error if no offscreen target is set.
    [[nodiscard]] std::vector<std::byte> read_offscreen_pixels();
    // Blocks until the GPU has finished all submitted work.
    void wait_idle();

    // Returns false if this frame can't be drawn (e.g. window minimized).
    // end_frame() must be called either way.
    bool begin_frame(Color clear_color);
    // Queues a sprite; sorting and drawing happen in end_frame().
    // Sprites outside the camera view are culled here, before any sorting.
    // The texture must stay alive until end_frame().
    void draw_sprite(const Texture& texture, const Sprite& sprite);
    // Single-texture bulk submission. Call on the render thread with immutable input.
    void draw_sprites(const Texture& texture, std::span<const Sprite> sprites);
    // Draws glyphs from a texture created with FontFace::atlas_pixels().
    void draw_text(const Texture& atlas, const FontFace& face, std::string_view text,
                   Vec2 position, Color color = {1.0F, 1.0F, 1.0F, 1.0F}, int layer = 0);
    void draw_text_layout(const Texture& atlas, const TextLayout& layout, Vec2 position,
                          Color color = {1.0F, 1.0F, 1.0F, 1.0F}, int layer = 0);
    // Optional workers for bulk preparation and sorted instance gathering; 0 disables.
    // Call between frames. GPU calls remain on the render thread.
    void set_worker_count(std::size_t count);
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
    SDL_GPUSampler* sampler_ = nullptr;        // TextureFilter::Nearest
    SDL_GPUSampler* linear_sampler_ = nullptr; // TextureFilter::Linear (with mipmaps)
    SDL_GPUTexture* offscreen_ = nullptr;
    int offscreen_width_ = 0;
    int offscreen_height_ = 0;
    std::uint32_t target_format_ = 0; // SDL_GPUTextureFormat of the window's swapchain
    SDL_GPUBuffer* instance_buffer_ = nullptr;
    SDL_GPUTransferBuffer* instance_transfer_ = nullptr;
    std::uint32_t instance_capacity_ = 0;

    SDL_GPUCommandBuffer* commands_ = nullptr;
    SDL_GPUTexture* frame_target_ = nullptr;
    Color clear_color_{};
    Vec2 viewport_{};
    Camera camera_{};
    Rect visible_{};
    std::uint32_t culled_ = 0;
    std::uint64_t frame_start_ns_ = 0;
    SpriteQueue queue_;
    RenderStats stats_{};
    std::unique_ptr<TaskPool> workers_;
};

} // namespace phantasm
