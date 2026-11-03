#pragma once

struct SDL_GPUDevice;
struct SDL_GPUTexture;

namespace phantasm {

// GPU texture handle (RAII, move-only). Created by Renderer::load_texture.
// Must be destroyed before the Renderer that created it.
class Texture {
public:
    Texture(SDL_GPUDevice* device, SDL_GPUTexture* handle, int width, int height) noexcept;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] SDL_GPUTexture* native_handle() const noexcept { return handle_; }

private:
    void release() noexcept;

    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUTexture* handle_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace phantasm
