#include <phantasm/texture.hpp>

#include <SDL3/SDL.h>

#include <atomic>
#include <utility>

namespace phantasm {

namespace {
std::atomic<std::uint32_t> next_texture_id{1};
}

Texture::Texture(SDL_GPUDevice* device, SDL_GPUTexture* handle, int width, int height) noexcept
    : device_{device}, handle_{handle},
      id_{next_texture_id.fetch_add(1, std::memory_order_relaxed)}, width_{width},
      height_{height} {}

Texture::~Texture() {
    release();
}

// The moved-from object is left empty so its destructor releases nothing.
Texture::Texture(Texture&& other) noexcept
    : device_{std::exchange(other.device_, nullptr)},
      handle_{std::exchange(other.handle_, nullptr)},
      id_{std::exchange(other.id_, 0)},
      width_{std::exchange(other.width_, 0)},
      height_{std::exchange(other.height_, 0)} {}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        release();
        device_ = std::exchange(other.device_, nullptr);
        handle_ = std::exchange(other.handle_, nullptr);
        id_ = std::exchange(other.id_, 0);
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
