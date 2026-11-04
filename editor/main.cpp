#include <phantasm/application.hpp>
#include <phantasm/iso/scene.hpp>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using phantasm::iso::GridPos;
using phantasm::iso::IsoGrid;
using phantasm::iso::Scene;
using phantasm::iso::SceneObject;
using phantasm::iso::TileCoord;
using phantasm::iso::TileMap;

enum class Tool { floor, wall, erase, place, select };

struct ImGuiScope {
    bool platform = false;
    bool gpu = false;
    ~ImGuiScope() {
        if (platform) ImGui_ImplSDL3_Shutdown();
        if (gpu) ImGui_ImplSDLGPU3_Shutdown();
        ImGui::DestroyContext();
    }
};

std::vector<std::byte> diamond_pixels() {
    constexpr int width = 64;
    constexpr int height = 32;
    std::vector<std::byte> pixels(static_cast<std::size_t>(width * height * 4));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float dx = std::abs((static_cast<float>(x) + 0.5F - 32.0F) / 32.0F);
            const float dy = std::abs((static_cast<float>(y) + 0.5F - 16.0F) / 16.0F);
            const auto index = static_cast<std::size_t>((y * width + x) * 4);
            pixels[index] = std::byte{255};
            pixels[index + 1] = std::byte{255};
            pixels[index + 2] = std::byte{255};
            pixels[index + 3] = dx + dy <= 1.0F ? std::byte{255} : std::byte{0};
        }
    }
    return pixels;
}

std::vector<std::byte> white_pixel() {
    return {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
}

void draw_scene(phantasm::Renderer& renderer, const Scene& scene, const IsoGrid& grid,
                const phantasm::Texture& diamond, const phantasm::Texture& marker, int level) {
    if (!renderer.begin_frame({0.06F, 0.08F, 0.11F, 1.0F})) {
        renderer.end_frame();
        return;
    }
    const auto bounds = phantasm::iso::visible_tile_bounds(
        grid, renderer.visible_rect(), level, scene.map().width(), scene.map().height());
    for (int y = bounds.first_y; y < bounds.last_y; ++y) {
        for (int x = bounds.first_x; x < bounds.last_x; ++x) {
            const char tile = scene.map().at(TileCoord{x, y, level});
            const phantasm::Color tint = tile == '#'
                ? phantasm::Color{0.68F, 0.43F, 0.21F, 1.0F}
                : tile == '.' ? phantasm::Color{0.38F, 0.56F, 0.47F, 1.0F}
                              : phantasm::Color{0.18F, 0.24F, 0.30F, 1.0F};
            renderer.draw_sprite(diamond, {
                .position = grid.grid_to_world(GridPos{static_cast<float>(x),
                                                        static_cast<float>(y), level}),
                .size = {grid.tile_width, grid.tile_height},
                .pivot = {0.5F, 0.0F},
                .tint = tint,
                .layer = 0,
                .depth = static_cast<float>(x + y),
            });
        }
    }
    scene.entities().each<SceneObject>([&](const SceneObject& object) {
        if (object.position.level != level) return;
        const auto world = grid.grid_to_world(object.position);
        renderer.draw_sprite(marker, {
            .position = {world.x, world.y - 12.0F},
            .size = {12.0F, 20.0F},
            .pivot = {0.5F, 1.0F},
            .tint = {0.95F, 0.28F, 0.38F, 1.0F},
            .layer = 1,
            .depth = phantasm::iso::draw_depth(object.position),
        });
    });
    renderer.end_frame();
}

void present(phantasm::Renderer& renderer, SDL_Window* window) {
    ImGui::Render();
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(renderer.native_device());
    if (commands == nullptr) throw std::runtime_error(SDL_GetError());
    SDL_GPUTexture* swapchain = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window, &swapchain, nullptr, nullptr)) {
        SDL_CancelGPUCommandBuffer(commands);
        throw std::runtime_error(SDL_GetError());
    }
    if (swapchain != nullptr) {
        ImDrawData* data = ImGui::GetDrawData();
        ImGui_ImplSDLGPU3_PrepareDrawData(data, commands);
        SDL_GPUColorTargetInfo target{};
        target.texture = swapchain;
        target.clear_color = {0.06F, 0.08F, 0.11F, 1.0F};
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
        ImGui_ImplSDLGPU3_RenderDrawData(data, commands, pass);
        SDL_EndGPURenderPass(pass);
    }
    if (!SDL_SubmitGPUCommandBuffer(commands)) throw std::runtime_error(SDL_GetError());
}

class Editor {
public:
    Editor() : window_{{.title = "Phantasm Editor", .width = 1280, .height = 800}},
               renderer_{window_}, scene_{TileMap::blank(32, 32, 2)},
               diamond_{renderer_.create_texture(64, 32, diamond_pixels())},
               marker_{renderer_.create_texture(1, 1, white_pixel())} {
        renderer_.set_camera({.position = {0.0F, 512.0F}, .zoom = 0.8F});
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        imgui_ = std::make_unique<ImGuiScope>();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::StyleColorsDark();
        if (!ImGui_ImplSDL3_InitForSDLGPU(window_.native_handle())) {
            throw std::runtime_error("Could not initialize ImGui SDL3 backend");
        }
        imgui_->platform = true;
        ImGui_ImplSDLGPU3_InitInfo info{};
        info.Device = renderer_.native_device();
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(info.Device,
                                                                  window_.native_handle());
        if (!ImGui_ImplSDLGPU3_Init(&info)) {
            throw std::runtime_error("Could not initialize ImGui GPU backend");
        }
        imgui_->gpu = true;
    }

    ~Editor() { renderer_.wait_idle(); }

    void run() {
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                     event.window.windowID == SDL_GetWindowID(window_.native_handle()))) {
                    running = false;
                }
            }
            if (!running) break;
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            sidebar();
            viewport();
            draw_scene(renderer_, scene_, grid_, diamond_, marker_, level_);
            present(renderer_, window_.native_handle());
        }
        renderer_.wait_idle();
    }

private:
    void sidebar() {
        ImGui::SetNextWindowPos({0.0F, 0.0F}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({280.0F, ImGui::GetIO().DisplaySize.y}, ImGuiCond_Always);
        ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoCollapse);
        ImGui::InputText("File", path_.data(), path_.size());
        if (ImGui::Button("Save")) {
            try { scene_.save_file(path_.data()); message_ = "Saved"; }
            catch (const std::exception& error) { message_ = error.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            try { scene_ = Scene::load_file(path_.data()); selected_ = 0; level_ = 0; message_ = "Loaded"; }
            catch (const std::exception& error) { message_ = error.what(); }
        }
        ImGui::TextWrapped("%s", message_.c_str());
        ImGui::Separator();
        ImGui::SliderInt("Floor level", &level_, 0, scene_.map().level_count() - 1);
        int tool = static_cast<int>(tool_);
        const char* names[] = {"Floor", "Wall", "Erase", "Place entity", "Select"};
        if (ImGui::Combo("Tool", &tool, names, 5)) tool_ = static_cast<Tool>(tool);
        ImGui::TextUnformatted("Click or drag in the viewport to paint.");
        ImGui::Separator();
        ImGui::Text("Entities (%zu)", scene_.entities().size());
        scene_.entities().each<SceneObject>([&](const SceneObject& object) {
            const std::string label = object.name + "##" + std::to_string(object.id);
            if (ImGui::Selectable(label.c_str(), selected_ == object.id)) selected_ = object.id;
        });
        if (SceneObject* object = scene_.object(selected_)) {
            ImGui::Separator();
            ImGui::Text("ID: %llu", static_cast<unsigned long long>(selected_));
            std::snprintf(name_.data(), name_.size(), "%s", object->name.c_str());
            if (ImGui::InputText("Name", name_.data(), name_.size())) object->name = name_.data();
            ImGui::DragFloat2("Grid position", &object->position.x, 0.05F);
            ImGui::SliderInt("Object level", &object->position.level, 0,
                             scene_.map().level_count() - 1);
            if (ImGui::Button("Delete entity")) {
                (void)scene_.erase(selected_);
                selected_ = 0;
            }
        }
        ImGui::End();
    }

    void viewport() {
        ImGui::SetNextWindowPos({280.0F, 0.0F}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({std::max(1.0F, ImGui::GetIO().DisplaySize.x - 280.0F),
                                  ImGui::GetIO().DisplaySize.y}, ImGuiCond_Always);
        ImGui::Begin("Isometric viewport", nullptr, ImGuiWindowFlags_NoMove |
                                                      ImGuiWindowFlags_NoResize |
                                                      ImGuiWindowFlags_NoCollapse);
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const int width = std::clamp(static_cast<int>(available.x), 1, 4096);
        const int height = std::clamp(static_cast<int>(available.y), 1, 4096);
        if (width != view_width_ || height != view_height_) {
            renderer_.set_offscreen(width, height);
            view_width_ = width;
            view_height_ = height;
        }
        const ImVec2 top_left = ImGui::GetCursorScreenPos();
        ImGui::Image(ImTextureRef(renderer_.offscreen_texture()),
                     {static_cast<float>(width), static_cast<float>(height)});
        if (ImGui::IsItemHovered()) {
            auto camera = renderer_.camera();
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                camera.position.x -= delta.x / camera.zoom;
                camera.position.y -= delta.y / camera.zoom;
            }
            if (ImGui::GetIO().MouseWheel != 0.0F) {
                camera.zoom = std::clamp(camera.zoom *
                                         std::pow(1.1F, ImGui::GetIO().MouseWheel), 0.2F, 4.0F);
            }
            renderer_.set_camera(camera);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const auto& camera = renderer_.camera();
            const phantasm::Vec2 world{
                (mouse.x - top_left.x - static_cast<float>(width) * 0.5F) / camera.zoom + camera.position.x,
                (mouse.y - top_left.y - static_cast<float>(height) * 0.5F) / camera.zoom + camera.position.y};
            const TileCoord tile = grid_.pick_tile(world, level_);
            if (tile.x >= 0 && tile.x < scene_.map().width() && tile.y >= 0 &&
                tile.y < scene_.map().height()) {
                if (tool_ == Tool::floor || tool_ == Tool::wall || tool_ == Tool::erase) {
                    scene_.map().set(tile, tool_ == Tool::floor ? '.' : tool_ == Tool::wall ? '#' : ' ');
                } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && tool_ == Tool::place) {
                    selected_ = scene_.create(GridPos{static_cast<float>(tile.x) + 0.5F,
                                                       static_cast<float>(tile.y) + 0.5F, level_},
                                              "Entity");
                } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && tool_ == Tool::select) {
                    selected_ = 0;
                    scene_.entities().each<SceneObject>([&](const SceneObject& object) {
                        if (object.position.level == level_ &&
                            std::abs(object.position.x - static_cast<float>(tile.x) - 0.5F) < 0.5F &&
                            std::abs(object.position.y - static_cast<float>(tile.y) - 0.5F) < 0.5F) {
                            selected_ = object.id;
                        }
                    });
                }
            }
        }
        ImGui::End();
    }

    phantasm::detail::SdlContext sdl_;
    phantasm::Window window_;
    phantasm::Renderer renderer_;
    Scene scene_;
    phantasm::Texture diamond_;
    phantasm::Texture marker_;
    std::unique_ptr<ImGuiScope> imgui_;
    IsoGrid grid_;
    std::array<char, 512> path_{'s', 'c', 'e', 'n', 'e', '.', 'p', 'h', 's', 'c', 'e', 'n', 'e', '\0'};
    std::array<char, 128> name_{};
    std::string message_;
    std::uint64_t selected_ = 0;
    int level_ = 0;
    int view_width_ = 0;
    int view_height_ = 0;
    Tool tool_ = Tool::floor;
};

} // namespace

int main() {
    try {
        Editor editor;
        editor.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Phantasm Editor: %s\n", error.what());
        return 1;
    }
}
