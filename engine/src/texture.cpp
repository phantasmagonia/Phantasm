#include <phantasm/texture.hpp>

#include <SDL3/SDL.h>

#include <utility>

namespace phantasm {

Texture::Texture(SDL_GPUDevice* device, SDL_GPUTexture* handle, int width, int height) noexcept
    : device_{device}, handle_{handle}, width_{width}, height_{height} {}

Texture::~Texture() {
    release();
}

// The moved-from object is left empty so its destructor releases nothing.
Texture::Texture(Texture&& other) noexcept
    : device_{std::exchange(other.device_, nullptr)},
      handle_{std::exchange(other.handle_, nullptr)},
      width_{std::exchange(other.width_, 0)},
      height_{std::exchange(other.height_, 0)} {}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        release();
        device_ = std::exchange(other.device_, nullptr);
        handle_ = std::exchange(other.handle_, nullptr);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
    }
    return *this;
}

void Texture::release() noexcept {
    if (handle_ != nullptr) {
        SDL_ReleaseGPUTexture(device_, handle_);
        handle_ = nullptr;
    }
}

} // namespace phantasm
