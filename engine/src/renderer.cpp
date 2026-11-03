#include <phantasm/renderer.hpp>

#include <phantasm/log.hpp>
#include <phantasm/window.hpp>

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace phantasm {

namespace {

#ifdef NDEBUG
constexpr bool kGpuDebugMode = false;
#else
constexpr bool kGpuDebugMode = true;
#endif

// One shader format per backend: Vulkan, D3D12, Metal.
constexpr SDL_GPUShaderFormat kShaderFormats =
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;

[[noreturn]] void throw_sdl_error(const char* what) {
    throw std::runtime_error(std::string{what} + " failed: " + SDL_GetError());
}

} // namespace

Renderer::Renderer(Window& window)
    : device_{SDL_CreateGPUDevice(kShaderFormats, kGpuDebugMode, nullptr)},
      window_{window.native_handle()} {
    if (device_ == nullptr) {
        throw_sdl_error("SDL_CreateGPUDevice");
    }
    if (!SDL_ClaimWindowForGPUDevice(device_, window_)) {
        // The destructor won't run if the constructor throws.
        const std::string error = SDL_GetError();
        SDL_DestroyGPUDevice(device_);
        throw std::runtime_error("SDL_ClaimWindowForGPUDevice failed: " + error);
    }
    log_info("GPU device created (backend: {})", SDL_GetGPUDeviceDriver(device_));
}

Renderer::~Renderer() {
    SDL_WaitForGPUIdle(device_);
    SDL_ReleaseWindowFromGPUDevice(device_, window_);
    SDL_DestroyGPUDevice(device_);
}

bool Renderer::draw_frame(Color clear_color) {
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    if (commands == nullptr) {
        log_error("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
        return false;
    }

    SDL_GPUTexture* target = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window_, &target, nullptr, nullptr)) {
        log_error("SDL_WaitAndAcquireGPUSwapchainTexture failed: {}", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commands);
        return false;
    }
    if (target == nullptr) {
        // A command buffer must always be submitted or cancelled.
        SDL_SubmitGPUCommandBuffer(commands);
        return false;
    }

    SDL_GPUColorTargetInfo color_target{};
    color_target.texture = target;
    color_target.clear_color = {clear_color.r, clear_color.g, clear_color.b, clear_color.a};
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color_target, 1, nullptr);
    SDL_EndGPURenderPass(pass);

    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        log_error("SDL_SubmitGPUCommandBuffer failed: {}", SDL_GetError());
        return false;
    }
    return true;
}

} // namespace phantasm
