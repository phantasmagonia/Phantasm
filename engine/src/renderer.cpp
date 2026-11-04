#include <phantasm/renderer.hpp>

#include "image.hpp"

#include <phantasm/log.hpp>
#include <phantasm/paths.hpp>
#include <phantasm/pixel_format.hpp>
#include <phantasm/window.hpp>
#include <phantasm/task_pool.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
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
constexpr Uint32 kInitialInstanceCapacity = 1024;
constexpr double kMsPerNs = 1e-6;

// Matches CameraUniforms in sprite.vert.
struct CameraUniforms {
    float clip_transform[4];
};

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

struct ShaderResources {
    Uint32 samplers = 0;
    Uint32 storage_buffers = 0;
    Uint32 uniform_buffers = 0;
};

ShaderPtr load_shader(SDL_GPUDevice* device, const std::string& name, SDL_GPUShaderStage stage,
                      ShaderResources resources) {
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
    info.num_samplers = resources.samplers;
    info.num_storage_buffers = resources.storage_buffers;
    info.num_uniform_buffers = resources.uniform_buffers;

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
        ensure_instance_capacity(kInitialInstanceCapacity);
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
    if (offscreen_ != nullptr) {
        SDL_ReleaseGPUTexture(device_, offscreen_);
    }
    if (instance_transfer_ != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device_, instance_transfer_);
    }
    if (instance_buffer_ != nullptr) {
        SDL_ReleaseGPUBuffer(device_, instance_buffer_);
    }
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
    const ShaderPtr vertex =
        load_shader(device_, "sprite.vert", SDL_GPU_SHADERSTAGE_VERTEX,
                    ShaderResources{.uniform_buffers = 1});
    const ShaderPtr fragment = load_shader(device_, "sprite.frag", SDL_GPU_SHADERSTAGE_FRAGMENT,
                                           ShaderResources{.samplers = 1});

    SDL_GPUColorTargetDescription target{};
    // Offscreen targets reuse this format so one pipeline serves both.
    target.format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
    target_format_ = static_cast<std::uint32_t>(target.format);
    target.blend_state.enable_blend = true;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    const SDL_GPUVertexBufferDescription instance_buffer{
        .slot = 0,
        .pitch = sizeof(SpriteInstance),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
        .instance_step_rate = 0,
    };
    const std::array<SDL_GPUVertexAttribute, 3> attributes{{
        // Normalized formats: the GPU expands 16/8-bit integers to floats in [0, 1].
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(SpriteInstance, rect)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM, offsetof(SpriteInstance, uv)},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(SpriteInstance, tint)},
    }};

    SDL_GPUGraphicsPipelineCreateInfo pipeline{};
    pipeline.vertex_input_state.vertex_buffer_descriptions = &instance_buffer;
    pipeline.vertex_input_state.num_vertex_buffers = 1;
    pipeline.vertex_input_state.vertex_attributes = attributes.data();
    pipeline.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
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
    const std::size_t byte_count = static_cast<std::size_t>(image.width) *
                                   static_cast<std::size_t>(image.height) * kBytesPerPixel;
    try {
        return create_texture(
            image.width, image.height,
            std::span{reinterpret_cast<const std::byte*>(image.pixels.get()), byte_count});
    } catch (const std::exception& error) {
        throw std::runtime_error(path + ": " + error.what());
    }
}

Texture Renderer::create_texture(int width_px, int height_px, std::span<const std::byte> rgba) {
    if (width_px <= 0 || height_px <= 0 || width_px > kMaxTextureSize ||
        height_px > kMaxTextureSize) {
        throw std::invalid_argument("Texture size must be 1.." +
                                    std::to_string(kMaxTextureSize) + " pixels per side");
    }
    const auto width = static_cast<Uint32>(width_px);
    const auto height = static_cast<Uint32>(height_px);
    const Uint32 byte_count = width * height * kBytesPerPixel;
    if (rgba.size() != byte_count) {
        throw std::invalid_argument("Pixel data must be width * height * 4 bytes (RGBA8)");
    }

    SDL_GPUTextureCreateInfo texture_info{};
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture_info.width = width;
    texture_info.height = height;
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    // Wrap immediately so the texture is released if anything below throws.
    Texture texture{device_, SDL_CreateGPUTexture(device_, &texture_info), width_px, height_px};
    if (texture.native_handle() == nullptr) {
        throw_sdl_error("Creating texture");
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
    // Rendered layers already contain premultiplied color; uploaded textures use
    // the same representation so nested layers blend without dark alpha edges.
    auto* pixels = static_cast<Uint8*>(mapped);
    for (std::size_t i = 0; i < byte_count; i += 4) {
        const auto alpha = std::to_integer<Uint32>(rgba[i + 3]);
        for (std::size_t channel = 0; channel < 3; ++channel) {
            pixels[i + channel] = static_cast<Uint8>(
                (std::to_integer<Uint32>(rgba[i + channel]) * alpha + 127) / 255);
        }
        pixels[i + 3] = static_cast<Uint8>(alpha);
    }
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

Texture Renderer::cache_layer(const Texture& atlas, std::span<const Sprite> sprites,
                              int width, int height) {
    if (commands_ != nullptr) {
        throw std::logic_error("cache_layer must be called between frames");
    }
    if (width <= 0 || height <= 0 || width > kMaxTextureSize || height > kMaxTextureSize) {
        throw std::invalid_argument("Cached layer dimensions are outside supported texture sizes");
    }
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = static_cast<SDL_GPUTextureFormat>(target_format_);
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = static_cast<Uint32>(width);
    info.height = static_cast<Uint32>(height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    Texture layer{device_, SDL_CreateGPUTexture(device_, &info), width, height};
    if (layer.native_handle() == nullptr) {
        throw_sdl_error("Creating cached layer");
    }
    const auto saved_camera = camera_;
    const auto saved_viewport = viewport_;
    const auto saved_visible = visible_;
    const auto saved_stats = stats_;
    const auto restore = [&] {
        camera_ = saved_camera;
        viewport_ = saved_viewport;
        visible_ = saved_visible;
        stats_ = saved_stats;
        frame_target_ = nullptr;
        queue_.clear();
    };
    commands_ = SDL_AcquireGPUCommandBuffer(device_);
    if (commands_ == nullptr) {
        throw_sdl_error("Acquiring cached layer command buffer");
    }
    try {
        frame_target_ = layer.native_handle();
        viewport_ = {static_cast<float>(width), static_cast<float>(height)};
        camera_ = {.position = {viewport_.x * 0.5F, viewport_.y * 0.5F}};
        visible_ = visible_world_rect(camera_, viewport_);
        clear_color_ = {0, 0, 0, 0};
        queue_.clear();
        culled_ = 0;
        frame_start_ns_ = SDL_GetTicksNS();
        for (const auto& sprite : sprites) {
            draw_sprite(atlas, sprite);
        }
        end_frame();
    } catch (...) {
        if (commands_ != nullptr) {
            SDL_CancelGPUCommandBuffer(commands_);
            commands_ = nullptr;
        }
        restore();
        throw;
    }
    restore();
    return layer;
}

void Renderer::ensure_instance_capacity(std::uint32_t count) {
    if (count <= instance_capacity_) {
        return;
    }
    const std::uint32_t capacity = std::max(count, instance_capacity_ * 2);
    const auto byte_size = static_cast<Uint32>(capacity * sizeof(SpriteInstance));

    // SDL defers the actual release until the GPU has finished using them.
    if (instance_transfer_ != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device_, instance_transfer_);
        instance_transfer_ = nullptr;
    }
    if (instance_buffer_ != nullptr) {
        SDL_ReleaseGPUBuffer(device_, instance_buffer_);
        instance_buffer_ = nullptr;
    }
    instance_capacity_ = 0;

    SDL_GPUBufferCreateInfo buffer_info{};
    buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    buffer_info.size = byte_size;
    instance_buffer_ = SDL_CreateGPUBuffer(device_, &buffer_info);
    if (instance_buffer_ == nullptr) {
        throw_sdl_error("Creating sprite instance buffer");
    }

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = byte_size;
    instance_transfer_ = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
    if (instance_transfer_ == nullptr) {
        throw_sdl_error("Creating sprite transfer buffer");
    }
    instance_capacity_ = capacity;
}

bool Renderer::set_vsync(bool enabled) {
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!enabled) {
        if (SDL_WindowSupportsGPUPresentMode(device_, window_, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        } else if (SDL_WindowSupportsGPUPresentMode(device_, window_,
                                                    SDL_GPU_PRESENTMODE_MAILBOX)) {
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
        } else {
            return false;
        }
    }
    if (!SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
        log_error("SDL_SetGPUSwapchainParameters failed: {}", SDL_GetError());
        return false;
    }
    return true;
}

void Renderer::set_camera(const Camera& camera) noexcept {
    camera_ = camera;
    visible_ = visible_world_rect(camera_, viewport_);
}

bool Renderer::begin_frame(Color clear_color) {
    queue_.clear();
    culled_ = 0;
    frame_target_ = nullptr;
    clear_color_ = clear_color;

    commands_ = SDL_AcquireGPUCommandBuffer(device_);
    if (commands_ == nullptr) {
        log_error("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
        return false;
    }

    if (offscreen_ != nullptr) {
        frame_target_ = offscreen_;
        viewport_ = Vec2{static_cast<float>(offscreen_width_), static_cast<float>(offscreen_height_)};
        visible_ = visible_world_rect(camera_, viewport_);
        frame_start_ns_ = SDL_GetTicksNS();
        return true;
    }

    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands_, window_, &frame_target_, &width,
                                               &height)) {
        log_error("SDL_WaitAndAcquireGPUSwapchainTexture failed: {}", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commands_);
        commands_ = nullptr;
        return false;
    }
    viewport_ = Vec2{static_cast<float>(width), static_cast<float>(height)};
    visible_ = visible_world_rect(camera_, viewport_);
    frame_start_ns_ = SDL_GetTicksNS();
    return frame_target_ != nullptr;
}

void Renderer::draw_sprite(const Texture& texture, const Sprite& sprite) {
    if (frame_target_ == nullptr || texture.native_handle() == nullptr) {
        return;
    }
    const Rect world_rect = sprite_world_rect(sprite);
    if (!intersects(world_rect, visible_)) {
        ++culled_;
        return;
    }
    queue_.push(texture, sprite.layer, sprite.depth,
                pack_instance(world_rect, sprite.uv, sprite.tint));
}

void Renderer::set_worker_count(std::size_t count) {
    if (commands_ != nullptr) {
        throw std::logic_error("set_worker_count must be called between frames");
    }
    if (count == 0) {
        workers_.reset();
    } else {
        workers_ = std::make_unique<TaskPool>(count);
    }
}

void Renderer::draw_sprites(const Texture& texture, std::span<const Sprite> sprites) {
    if (frame_target_ == nullptr || texture.native_handle() == nullptr) {
        return;
    }
    const std::size_t added = queue_.append(texture, sprites, visible_, workers_.get());
    culled_ += static_cast<std::uint32_t>(sprites.size() - added);
}

void Renderer::upload_instances() {
    const auto count = static_cast<std::uint32_t>(queue_.size());
    ensure_instance_capacity(count);

    // cycle = true: if the GPU is still reading last frame's data, SDL hands us
    // a fresh region instead of stalling or overwriting it.
    auto* mapped = static_cast<SpriteInstance*>(
        SDL_MapGPUTransferBuffer(device_, instance_transfer_, true));
    if (mapped == nullptr) {
        throw_sdl_error("Mapping sprite transfer buffer");
    }
    // Sorted instances go straight into GPU-visible memory: no intermediate copy.
    queue_.write_sorted(std::span{mapped, count}, workers_.get());
    SDL_UnmapGPUTransferBuffer(device_, instance_transfer_);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands_);
    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = instance_transfer_;
    SDL_GPUBufferRegion destination{};
    destination.buffer = instance_buffer_;
    destination.size = static_cast<Uint32>(count * sizeof(SpriteInstance));
    SDL_UploadToGPUBuffer(copy, &source, &destination, true);
    SDL_EndGPUCopyPass(copy);
}

void Renderer::record_draws(const std::vector<DrawRun>& runs) {
    SDL_GPUColorTargetInfo color_target{};
    color_target.texture = frame_target_;
    color_target.clear_color = {clear_color_.r * clear_color_.a, clear_color_.g * clear_color_.a,
                                clear_color_.b * clear_color_.a, clear_color_.a};
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands_, &color_target, 1, nullptr);

    if (!runs.empty()) {
        SDL_BindGPUGraphicsPipeline(pass, sprite_pipeline_);

        const ClipTransform clip = camera_to_clip(camera_, viewport_);
        const CameraUniforms uniforms{
            {clip.scale.x, clip.scale.y, clip.offset.x, clip.offset.y}};
        SDL_PushGPUVertexUniformData(commands_, 0, &uniforms, sizeof(uniforms));

        for (const DrawRun& run : runs) {
            const SDL_GPUTextureSamplerBinding sampler_binding{run.texture->native_handle(),
                                                               sampler_};
            SDL_BindGPUFragmentSamplers(pass, 0, &sampler_binding, 1);
            // Offsetting the binding (not first_instance) works on every backend.
            const SDL_GPUBufferBinding instances{
                instance_buffer_, static_cast<Uint32>(run.first * sizeof(SpriteInstance))};
            SDL_BindGPUVertexBuffers(pass, 0, &instances, 1);
            SDL_DrawGPUPrimitives(pass, kQuadVertexCount, run.count, 0, 0);
        }
    }
    SDL_EndGPURenderPass(pass);
}

void Renderer::end_frame() {
    if (commands_ == nullptr) {
        return;
    }
    if (frame_target_ != nullptr) {
        const Uint64 start_ns = SDL_GetTicksNS();
        const std::vector<DrawRun> runs = queue_.sort();
        if (queue_.size() != 0) {
            upload_instances();
        }
        record_draws(runs);
        stats_ = RenderStats{
            .sprites = static_cast<std::uint32_t>(queue_.size()),
            .culled = culled_,
            .draw_calls = static_cast<std::uint32_t>(runs.size()),
            .cpu_ms = static_cast<double>(SDL_GetTicksNS() - start_ns) * kMsPerNs,
            .submit_ms = static_cast<double>(SDL_GetTicksNS() - frame_start_ns_) * kMsPerNs,
        };
    }
    // A command buffer must always be submitted or cancelled.
    if (!SDL_SubmitGPUCommandBuffer(commands_)) {
        log_error("SDL_SubmitGPUCommandBuffer failed: {}", SDL_GetError());
    }
    commands_ = nullptr;
    frame_target_ = nullptr;
}

void Renderer::set_offscreen(int width, int height) {
    if (offscreen_ != nullptr) {
        SDL_WaitForGPUIdle(device_);
        SDL_ReleaseGPUTexture(device_, offscreen_);
        offscreen_ = nullptr;
        offscreen_width_ = 0;
        offscreen_height_ = 0;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width > kMaxTextureSize || height > kMaxTextureSize) {
        throw std::invalid_argument("Offscreen target is larger than the maximum texture size");
    }
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = static_cast<SDL_GPUTextureFormat>(target_format_);
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = static_cast<Uint32>(width);
    info.height = static_cast<Uint32>(height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    offscreen_ = SDL_CreateGPUTexture(device_, &info);
    if (offscreen_ == nullptr) {
        throw_sdl_error("Creating offscreen target");
    }
    offscreen_width_ = width;
    offscreen_height_ = height;
}

std::vector<std::byte> Renderer::read_offscreen_pixels() {
    if (offscreen_ == nullptr) {
        throw std::logic_error("read_offscreen_pixels() needs set_offscreen() first");
    }
    const auto width = static_cast<Uint32>(offscreen_width_);
    const auto height = static_cast<Uint32>(offscreen_height_);
    const Uint32 byte_count = width * height * kBytesPerPixel;

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer_info.size = byte_count;
    const TransferBufferPtr transfer{SDL_CreateGPUTransferBuffer(device_, &transfer_info),
                                     TransferBufferRelease{device_}};
    if (!transfer) {
        throw_sdl_error("Creating download buffer");
    }

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    if (commands == nullptr) {
        throw_sdl_error("Acquiring download command buffer");
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureRegion source{};
    source.texture = offscreen_;
    source.w = width;
    source.h = height;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = transfer.get();
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (fence == nullptr) {
        throw_sdl_error("Submitting download");
    }
    const bool finished = SDL_WaitForGPUFences(device_, true, &fence, 1);
    SDL_ReleaseGPUFence(device_, fence);
    if (!finished) {
        throw_sdl_error("Waiting for download");
    }

    const void* mapped = SDL_MapGPUTransferBuffer(device_, transfer.get(), false);
    if (mapped == nullptr) {
        throw_sdl_error("Mapping download buffer");
    }
    std::vector<std::byte> pixels(byte_count);
    std::memcpy(pixels.data(), mapped, byte_count);
    SDL_UnmapGPUTransferBuffer(device_, transfer.get());

    const auto format = static_cast<SDL_GPUTextureFormat>(target_format_);
    if (format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ||
        format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB) {
        bgra_to_rgba(pixels);
    }
    return pixels;
}

void Renderer::wait_idle() {
    SDL_WaitForGPUIdle(device_);
}

} // namespace phantasm
