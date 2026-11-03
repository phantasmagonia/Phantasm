#include <phantasm/renderer.hpp>

#include "image.hpp"

#include <phantasm/log.hpp>
#include <phantasm/paths.hpp>
#include <phantasm/window.hpp>

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace phantasm {

namespace {

#ifdef NDEBUG
constexpr bool kGpuDebugMode = false;
#else
constexpr bool kGpuDebugMode = true;
#endif

// No DXIL yet, so on Windows SDL picks Vulkan instead of Direct3D 12.
constexpr SDL_GPUShaderFormat kShaderFormats =
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL;

constexpr int kMaxTextureSize = 16384;
constexpr Uint32 kBytesPerPixel = 4;
constexpr Uint32 kQuadVertexCount = 6;

static_assert(sizeof(Rect) == 4 * sizeof(float), "Rect is pushed to the shader as a vec4");

[[noreturn]] void throw_sdl_error(const std::string& what) {
    throw std::runtime_error(what + " failed: " + SDL_GetError());
}

struct SdlFree {
    void operator()(void* memory) const noexcept { SDL_free(memory); }
};

struct ShaderRelease {
    SDL_GPUDevice* device;
    void operator()(SDL_GPUShader* shader) const noexcept { SDL_ReleaseGPUShader(device, shader); }
};
using ShaderPtr = std::unique_ptr<SDL_GPUShader, ShaderRelease>;

struct TransferBufferRelease {
    SDL_GPUDevice* device;
    void operator()(SDL_GPUTransferBuffer* buffer) const noexcept {
        SDL_ReleaseGPUTransferBuffer(device, buffer);
    }
};
using TransferBufferPtr = std::unique_ptr<SDL_GPUTransferBuffer, TransferBufferRelease>;

std::unique_ptr<void, SdlFree> load_file(const std::string& path, std::size_t& size) {
    std::unique_ptr<void, SdlFree> data{SDL_LoadFile(path.c_str(), &size)};
    if (!data) {
        throw_sdl_error("Reading " + path);
    }
    return data;
}

ShaderPtr load_shader(SDL_GPUDevice* device, const std::string& name, SDL_GPUShaderStage stage,
                      Uint32 sampler_count, Uint32 uniform_buffer_count) {
    const SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    const char* extension = nullptr;
    const char* entry_point = nullptr;
    if ((supported & SDL_GPU_SHADERFORMAT_MSL) != 0) {
        format = SDL_GPU_SHADERFORMAT_MSL;
        extension = ".msl";
        entry_point = "main0"; // spirv-cross renames main
    } else if ((supported & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        extension = ".spv";
        entry_point = "main";
    } else {
        throw std::runtime_error("GPU backend supports none of our shader formats");
    }

    std::size_t size = 0;
    const auto code = load_file(asset_path("shaders/" + name + extension), size);

    SDL_GPUShaderCreateInfo info{};
    info.code = static_cast<const Uint8*>(code.get());
    info.code_size = size;
    info.entrypoint = entry_point;
    info.format = format;
    info.stage = stage;
    info.num_samplers = sampler_count;
    info.num_uniform_buffers = uniform_buffer_count;

    ShaderPtr shader{SDL_CreateGPUShader(device, &info), ShaderRelease{device}};
    if (!shader) {
        throw_sdl_error("Creating shader " + name);
    }
    return shader;
}

} // namespace

Renderer::Renderer(Window& window) : window_{window.native_handle()} {
    // The destructor won't run if the constructor throws, so clean up here.
    try {
        device_ = SDL_CreateGPUDevice(kShaderFormats, kGpuDebugMode, nullptr);
        if (device_ == nullptr) {
            throw_sdl_error("SDL_CreateGPUDevice");
        }
        if (!SDL_ClaimWindowForGPUDevice(device_, window_)) {
            throw_sdl_error("SDL_ClaimWindowForGPUDevice");
        }
        create_sprite_pipeline();
    } catch (...) {
        destroy();
        throw;
    }
    log_info("GPU device created (backend: {})", SDL_GetGPUDeviceDriver(device_));
}

Renderer::~Renderer() {
    destroy();
}

void Renderer::destroy() noexcept {
    if (device_ == nullptr) {
        return;
    }
    SDL_WaitForGPUIdle(device_);
    if (sampler_ != nullptr) {
        SDL_ReleaseGPUSampler(device_, sampler_);
    }
    if (sprite_pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device_, sprite_pipeline_);
    }
    SDL_ReleaseWindowFromGPUDevice(device_, window_);
    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
}

void Renderer::create_sprite_pipeline() {
    const ShaderPtr vertex = load_shader(device_, "sprite.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    const ShaderPtr fragment =
        load_shader(device_, "sprite.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);

    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
    target.blend_state.enable_blend = true;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    SDL_GPUGraphicsPipelineCreateInfo pipeline{};
    pipeline.vertex_shader = vertex.get();
    pipeline.fragment_shader = fragment.get();
    pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline.target_info.color_target_descriptions = &target;
    pipeline.target_info.num_color_targets = 1;

    sprite_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &pipeline);
    if (sprite_pipeline_ == nullptr) {
        throw_sdl_error("Creating sprite pipeline");
    }

    // Nearest filtering keeps pixel art crisp when scaled.
    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_ = SDL_CreateGPUSampler(device_, &sampler);
    if (sampler_ == nullptr) {
        throw_sdl_error("Creating sampler");
    }
}

Texture Renderer::load_texture(const std::string& path) {
    std::size_t file_size = 0;
    const auto file = load_file(path, file_size);
    const detail::Image image =
        detail::decode_image(std::span{static_cast<const std::byte*>(file.get()), file_size});
    if (image.width > kMaxTextureSize || image.height > kMaxTextureSize) {
        throw std::runtime_error("Texture too large: " + path);
    }
    const auto width = static_cast<Uint32>(image.width);
    const auto height = static_cast<Uint32>(image.height);
    const Uint32 byte_count = width * height * kBytesPerPixel;

    SDL_GPUTextureCreateInfo texture_info{};
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture_info.width = width;
    texture_info.height = height;
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    // Wrap immediately so the texture is released if anything below throws.
    Texture texture{device_, SDL_CreateGPUTexture(device_, &texture_info), image.width,
                    image.height};
    if (texture.native_handle() == nullptr) {
        throw_sdl_error("Creating texture for " + path);
    }

    // CPU memory -> transfer buffer -> GPU texture (via a copy pass).
    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = byte_count;
    const TransferBufferPtr transfer{SDL_CreateGPUTransferBuffer(device_, &transfer_info),
                                     TransferBufferRelease{device_}};
    if (!transfer) {
        throw_sdl_error("Creating transfer buffer");
    }
    void* mapped = SDL_MapGPUTransferBuffer(device_, transfer.get(), false);
    if (mapped == nullptr) {
        throw_sdl_error("Mapping transfer buffer");
    }
    std::memcpy(mapped, image.pixels.get(), byte_count);
    SDL_UnmapGPUTransferBuffer(device_, transfer.get());

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    if (commands == nullptr) {
        throw_sdl_error("Acquiring upload command buffer");
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = transfer.get();
    SDL_GPUTextureRegion destination{};
    destination.texture = texture.native_handle();
    destination.w = width;
    destination.h = height;
    destination.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        throw_sdl_error("Submitting texture upload");
    }
    return texture;
}

bool Renderer::begin_frame(Color clear_color) {
    commands_ = SDL_AcquireGPUCommandBuffer(device_);
    if (commands_ == nullptr) {
        log_error("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
        return false;
    }

    SDL_GPUTexture* target = nullptr;
    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands_, window_, &target, &width, &height)) {
        log_error("SDL_WaitAndAcquireGPUSwapchainTexture failed: {}", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commands_);
        commands_ = nullptr;
        return false;
    }
    if (target == nullptr) {
        return false;
    }
    viewport_ = Vec2{static_cast<float>(width), static_cast<float>(height)};

    SDL_GPUColorTargetInfo color_target{};
    color_target.texture = target;
    color_target.clear_color = {clear_color.r, clear_color.g, clear_color.b, clear_color.a};
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    pass_ = SDL_BeginGPURenderPass(commands_, &color_target, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass_, sprite_pipeline_);
    return true;
}

void Renderer::draw_sprite(const Texture& texture, Rect destination) {
    if (pass_ == nullptr || texture.native_handle() == nullptr) {
        return;
    }
    const SDL_GPUTextureSamplerBinding binding{texture.native_handle(), sampler_};
    SDL_BindGPUFragmentSamplers(pass_, 0, &binding, 1);
    const Rect clip = to_clip_rect(destination, viewport_);
    SDL_PushGPUVertexUniformData(commands_, 0, &clip, sizeof(clip));
    SDL_DrawGPUPrimitives(pass_, kQuadVertexCount, 1, 0, 0);
}

void Renderer::end_frame() {
    if (pass_ != nullptr) {
        SDL_EndGPURenderPass(pass_);
        pass_ = nullptr;
    }
    if (commands_ == nullptr) {
        return;
    }
    // A command buffer must always be submitted or cancelled.
    if (!SDL_SubmitGPUCommandBuffer(commands_)) {
        log_error("SDL_SubmitGPUCommandBuffer failed: {}", SDL_GetError());
    }
    commands_ = nullptr;
}

} // namespace phantasm
