#pragma once

#include <cstdint>

struct SDL_GPUDevice;
struct SDL_GPUTexture;

namespace phantasm {

// Nearest: crisp texels (pixel art, UI). Linear: smooth sampling with mipmaps, for
// high-resolution art that is scaled or zoomed.
enum class TextureFilter : std::uint8_t { Nearest, Linear };

// GPU texture handle (RAII, move-only). Created by Renderer::load_texture.
// Must be destroyed before the Renderer that created it.
class Texture {
public:
    Texture(SDL_GPUDevice* device, SDL_GPUTexture* handle, int width, int height,
            TextureFilter filter = TextureFilter::Nearest) noexcept;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] TextureFilter filter() const noexcept { return filter_; }
    [[nodiscard]] SDL_GPUTexture* native_handle() const noexcept { return handle_; }
    // Unique per texture (0 for an empty, moved-from texture); used in sort keys.
    [[nodiscard]] std::uint32_t id() const noexcept { return id_; }

private:
    void release() noexcept;

    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUTexture* handle_ = nullptr;
    std::uint32_t id_ = 0;
    int width_ = 0;
    int height_ = 0;
    TextureFilter filter_ = TextureFilter::Nearest;
};

} // namespace phantasm
