#include <phantasm/editor/editor.hpp>

#include "asset_cache.hpp"

#include <phantasm/application.hpp>
#include <phantasm/camera.hpp>
#include <phantasm/iso/iso_grid.hpp>
#include <phantasm/renderer.hpp>
#include <phantasm/window.hpp>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

namespace phantasm::editor {

namespace {

constexpr float kSidebarWidth = 280.0F;
constexpr int kMaxViewportSize = 4096;
constexpr Color kBackground{0.06F, 0.08F, 0.11F, 1.0F};
constexpr Color kFloorTint{0.38F, 0.56F, 0.47F, 1.0F};
constexpr Color kWallTint{0.68F, 0.43F, 0.21F, 1.0F};
constexpr Color kEmptyTint{0.18F, 0.24F, 0.30F, 1.0F};
constexpr Color kMarkerTint{0.95F, 0.28F, 0.38F, 1.0F};
constexpr Vec2 kMarkerSize{12.0F, 20.0F};

// Owns the Dear ImGui context and backends; shuts down only what was initialized.
struct ImGuiScope {
    bool platform = false;
    bool gpu = false;

    ImGuiScope() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::StyleColorsDark();
    }
    ~ImGuiScope() {
        if (platform) {
            ImGui_ImplSDL3_Shutdown();
        }
        if (gpu) {
            ImGui_ImplSDLGPU3_Shutdown();
        }
        ImGui::DestroyContext();
    }
    ImGuiScope(const ImGuiScope&) = delete;
    ImGuiScope& operator=(const ImGuiScope&) = delete;
};

std::vector<std::byte> diamond_pixels() {
    constexpr int kWidth = 64;
    constexpr int kHeight = 32;
    std::vector<std::byte> pixels(static_cast<std::size_t>(kWidth * kHeight * 4));
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const float dx = std::abs((static_cast<float>(x) + 0.5F - 32.0F) / 32.0F);
            const float dy = std::abs((static_cast<float>(y) + 0.5F - 16.0F) / 16.0F);
            const auto index = static_cast<std::size_t>((y * kWidth + x) * 4);
            pixels[index] = std::byte{255};
            pixels[index + 1] = std::byte{255};
            pixels[index + 2] = std::byte{255};
            pixels[index + 3] = dx + dy <= 1.0F ? std::byte{255} : std::byte{0};
        }
    }
    return pixels;
}

Color tint_for(char tile) noexcept {
    if (tile == '#') {
        return kWallTint;
    }
    return tile == '.' ? kFloorTint : kEmptyTint;
}

// Fixed-size ImGui text buffer synchronized with a std::string.
template <std::size_t N>
struct TextField {
    std::array<char, N> buffer{};
    void set(const std::string& value) { std::snprintf(buffer.data(), N, "%s", value.c_str()); }
    [[nodiscard]] std::string get() const { return buffer.data(); }
};

} // namespace

struct Editor::Impl {
    explicit Impl(const EditorOptions& options)
        : window{{.title = "Phantasm Editor",
                  .width = options.width,
                  .height = options.height,
                  .hidden = options.hidden}},
          renderer{window},
          diamond{renderer.create_texture(64, 32, diamond_pixels())},
          marker{renderer.create_texture(
              1, 1, std::vector<std::byte>(4, std::byte{255}))} {
        renderer.set_camera({.position = {0.0F, 512.0F}, .zoom = 0.8F});
        if (!ImGui_ImplSDL3_InitForSDLGPU(window.native_handle())) {
            throw std::runtime_error("Could not initialize the ImGui SDL3 backend");
        }
        imgui.platform = true;
        ImGui_ImplSDLGPU3_InitInfo info{};
        info.Device = renderer.native_device();
        info.ColorTargetFormat =
            SDL_GetGPUSwapchainTextureFormat(info.Device, window.native_handle());
        if (!ImGui_ImplSDLGPU3_Init(&info)) {
            throw std::runtime_error("Could not initialize the ImGui GPU backend");
        }
        imgui.gpu = true;
        file.set("scene.phscene");
        if (options.scene) {
            state.load(*options.scene);
            assets.set_scene_file(*options.scene);
            file.set(options.scene->string());
        }
    }

    ~Impl() { renderer.wait_idle(); }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    bool frame();
    void handle_shortcuts();
    void sidebar();
    void inspector(iso::SceneObject& object);
    void viewport();
    void custom_panels();
    void draw_scene();
    void present();

    // Track an inspector widget: snapshot before it changes, record when released.
    void track(const iso::SceneObject& before) {
        if (ImGui::IsItemActivated() || ImGui::IsItemEdited()) {
            state.begin_object_edit(before);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            state.finish_object_edit();
        }
    }

    // Declaration order is construction order: SDL, then window, then renderer, and the
    // ImGui context before anything that uses it; destruction runs in reverse.
    phantasm::detail::SdlContext sdl;
    Window window;
    Renderer renderer;
    ImGuiScope imgui;
    Texture diamond;
    Texture marker;
    detail::AssetCache assets{renderer};
    EditorState state;
    iso::IsoGrid grid;
    std::vector<std::pair<std::string, PanelFn>> panels;
    std::vector<std::pair<std::string, InspectorFn>> inspectors;
    TextField<512> file;
    TextField<512> placement;
    TextField<128> name;
    TextField<512> image;
    std::string message;
    int view_width = 0;
    int view_height = 0;
};

bool Editor::Impl::frame() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        const bool closed = event.type == SDL_EVENT_QUIT ||
                            (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                             event.window.windowID == SDL_GetWindowID(window.native_handle()));
        if (closed) {
            return false;
        }
    }
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        state.pointer_up();
    }
    handle_shortcuts();
    sidebar();
    viewport();
    custom_panels();
    draw_scene();
    present();
    return true;
}

void Editor::Impl::handle_shortcuts() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || !(io.KeyCtrl || io.KeySuper)) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        (void)(io.KeyShift ? state.redo() : state.undo());
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        (void)state.redo();
    }
}

void Editor::Impl::sidebar() {
    ImGui::SetNextWindowPos({0.0F, 0.0F}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({kSidebarWidth, ImGui::GetIO().DisplaySize.y}, ImGuiCond_Always);
    ImGui::Begin("Scene", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::InputText("File", file.buffer.data(), file.buffer.size());
    if (ImGui::Button("Save")) {
        try {
            state.save(file.get());
            assets.set_scene_file(file.get());
            message = "Saved";
        } catch (const std::exception& error) {
            message = error.what();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        try {
            state.load(file.get());
            assets.set_scene_file(file.get());
            message = "Loaded";
        } catch (const std::exception& error) {
            message = error.what();
        }
    }
    ImGui::TextWrapped("%s", message.c_str());
    if (!assets.error().empty()) {
        ImGui::TextWrapped("%s", assets.error().c_str());
    }
    if (ImGui::Button("Reload images")) {
        assets.clear();
    }

    ImGui::Separator();
    ImGui::BeginDisabled(!state.can_undo());
    if (ImGui::Button("Undo")) {
        (void)state.undo();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.can_redo());
    if (ImGui::Button("Redo")) {
        (void)state.redo();
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    int level = state.level();
    if (ImGui::SliderInt("Floor level", &level, 0, state.scene().map().level_count() - 1)) {
        state.set_level(level);
    }
    int tool = static_cast<int>(state.tool());
    const std::array names{"Floor", "Wall", "Erase", "Place entity", "Select"};
    if (ImGui::Combo("Tool", &tool, names.data(), static_cast<int>(names.size()))) {
        state.set_tool(static_cast<Tool>(tool));
    }
    ImGui::TextUnformatted("Click or drag in the viewport to paint.");
    placement.set(state.placement_asset());
    if (ImGui::InputText("Placement image", placement.buffer.data(), placement.buffer.size())) {
        state.set_placement_asset(placement.get());
    }

    ImGui::Separator();
    ImGui::Text("Entities (%zu)", state.scene().entities().size());
    std::uint64_t clicked = 0;
    state.scene().entities().each<iso::SceneObject>([&](const iso::SceneObject& object) {
        const std::string label = object.name + "##" + std::to_string(object.id);
        if (ImGui::Selectable(label.c_str(), state.selected() == object.id)) {
            clicked = object.id;
        }
    });
    if (clicked != 0) {
        state.select(clicked); // outside iteration: selecting may record an undo step
    }
    if (iso::SceneObject* object = state.selected_object()) {
        inspector(*object);
    }
    ImGui::End();
}

void Editor::Impl::inspector(iso::SceneObject& object) {
    ImGui::Separator();
    ImGui::Text("ID: %llu", static_cast<unsigned long long>(object.id));

    name.set(object.name);
    const iso::SceneObject name_before = object;
    if (ImGui::InputText("Name", name.buffer.data(), name.buffer.size())) {
        object.name = name.get();
    }
    track(name_before);

    image.set(object.asset);
    const iso::SceneObject image_before = object;
    if (ImGui::InputText("Image", image.buffer.data(), image.buffer.size())) {
        object.asset = image.get();
    }
    track(image_before);

    const iso::SceneObject position_before = object;
    ImGui::DragFloat2("Grid position", &object.position.x, 0.05F);
    track(position_before);

    const iso::SceneObject level_before = object;
    ImGui::SliderInt("Object level", &object.position.level, 0,
                     state.scene().map().level_count() - 1);
    track(level_before);

    const iso::SceneObject scale_before = object;
    ImGui::DragFloat("Image scale", &object.scale, 0.01F, 0.01F, 20.0F);
    track(scale_before);

    const Entity entity = state.scene().runtime_entity(object.id);
    for (auto& [title, draw] : inspectors) {
        ImGui::SeparatorText(title.c_str());
        draw(state.scene().entities(), entity);
    }

    if (ImGui::Button("Delete entity")) {
        (void)state.delete_selected();
    }
}

void Editor::Impl::viewport() {
    ImGui::SetNextWindowPos({kSidebarWidth, 0.0F}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({std::max(1.0F, ImGui::GetIO().DisplaySize.x - kSidebarWidth),
                              ImGui::GetIO().DisplaySize.y},
                             ImGuiCond_Always);
    ImGui::Begin("Isometric viewport", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int width = std::clamp(static_cast<int>(available.x), 1, kMaxViewportSize);
    const int height = std::clamp(static_cast<int>(available.y), 1, kMaxViewportSize);
    if (width != view_width || height != view_height) {
        renderer.set_offscreen(width, height);
        view_width = width;
        view_height = height;
    }
    const ImVec2 top_left = ImGui::GetCursorScreenPos();
    ImGui::Image(ImTextureRef(renderer.offscreen_texture()),
                 {static_cast<float>(width), static_cast<float>(height)});

    if (ImGui::IsItemHovered()) {
        Camera camera = renderer.camera();
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            camera.position.x -= io.MouseDelta.x / camera.zoom;
            camera.position.y -= io.MouseDelta.y / camera.zoom;
        }
        if (io.MouseWheel != 0.0F) {
            camera.zoom = std::clamp(camera.zoom * std::pow(1.1F, io.MouseWheel), 0.2F, 4.0F);
        }
        renderer.set_camera(camera);

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const Vec2 world = screen_to_world(
                renderer.camera(), {static_cast<float>(width), static_cast<float>(height)},
                {mouse.x - top_left.x, mouse.y - top_left.y});
            state.pointer_down(grid.pick_tile(world, state.level()),
                               ImGui::IsMouseClicked(ImGuiMouseButton_Left));
        }
    }
    ImGui::End();
}

void Editor::Impl::custom_panels() {
    for (auto& [title, draw] : panels) {
        ImGui::Begin(title.c_str());
        draw(state);
        ImGui::End();
    }
}

void Editor::Impl::draw_scene() {
    const iso::Scene& scene = state.scene();
    const int level = state.level();
    const Rect view = visible_world_rect(
        renderer.camera(), {static_cast<float>(view_width), static_cast<float>(view_height)});
    assets.prepare(scene, grid, level, view);
    if (!renderer.begin_frame(kBackground)) {
        renderer.end_frame();
        return;
    }
    const auto bounds = iso::visible_tile_bounds(grid, renderer.visible_rect(), level,
                                                 scene.map().width(), scene.map().height());
    for (int y = bounds.first_y; y < bounds.last_y; ++y) {
        for (int x = bounds.first_x; x < bounds.last_x; ++x) {
            renderer.draw_sprite(
                diamond, {.position = grid.grid_to_world(iso::GridPos{static_cast<float>(x),
                                                                      static_cast<float>(y), level}),
                          .size = {grid.tile_width, grid.tile_height},
                          .pivot = {0.5F, 0.0F},
                          .tint = tint_for(scene.map().at(iso::TileCoord{x, y, level})),
                          .layer = 0,
                          .depth = static_cast<float>(x + y)});
        }
    }
    scene.entities().each<iso::SceneObject>([&](const iso::SceneObject& object) {
        if (object.position.level != level) {
            return;
        }
        const Texture* texture = assets.get(object.asset);
        const bool has_image = texture != nullptr;
        const Vec2 size = has_image ? Vec2{static_cast<float>(texture->width()) * object.scale,
                                           static_cast<float>(texture->height()) * object.scale}
                                    : kMarkerSize;
        renderer.draw_sprite(has_image ? *texture : marker,
                             {.position = grid.grid_to_world(object.position),
                              .size = size,
                              .pivot = {0.5F, 1.0F},
                              .tint = has_image ? Color{1.0F, 1.0F, 1.0F, 1.0F} : kMarkerTint,
                              .layer = 1,
                              .depth = iso::draw_depth(object.position)});
    });
    renderer.end_frame();
}

void Editor::Impl::present() {
    ImGui::Render();
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(renderer.native_device());
    if (commands == nullptr) {
        throw std::runtime_error(SDL_GetError());
    }
    SDL_GPUTexture* swapchain = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window.native_handle(), &swapchain,
                                               nullptr, nullptr)) {
        SDL_CancelGPUCommandBuffer(commands);
        throw std::runtime_error(SDL_GetError());
    }
    if (swapchain != nullptr) {
        ImDrawData* data = ImGui::GetDrawData();
        ImGui_ImplSDLGPU3_PrepareDrawData(data, commands);
        SDL_GPUColorTargetInfo target{};
        target.texture = swapchain;
        target.clear_color = {kBackground.r, kBackground.g, kBackground.b, kBackground.a};
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
        ImGui_ImplSDLGPU3_RenderDrawData(data, commands, pass);
        SDL_EndGPURenderPass(pass);
    }
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        throw std::runtime_error(SDL_GetError());
    }
}

Editor::Editor(const EditorOptions& options) : impl_{std::make_unique<Impl>(options)} {}

Editor::~Editor() = default;

void Editor::add_panel(std::string title, PanelFn draw) {
    impl_->panels.emplace_back(std::move(title), std::move(draw));
}

void Editor::add_inspector(std::string title, InspectorFn draw) {
    impl_->inspectors.emplace_back(std::move(title), std::move(draw));
}

bool Editor::frame() {
    return impl_->frame();
}

void Editor::run() {
    while (frame()) {
    }
    impl_->renderer.wait_idle();
}

EditorState& Editor::state() noexcept {
    return impl_->state;
}

} // namespace phantasm::editor
