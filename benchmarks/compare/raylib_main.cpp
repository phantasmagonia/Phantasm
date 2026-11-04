#include "workload.hpp"

#include <raylib.h>

#include <optional>

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#endif

int main(int argc, char** argv) {
    const auto options = compare::parse(argc, argv);
    SetTraceLogLevel(LOG_WARNING);
    if (options.offscreen) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
    }
    InitWindow(compare::kWindowWidth, compare::kWindowHeight, "raylib"); // vsync off by default
    SetTargetFPS(0);                                                      // unlimited

    auto pixels = compare::blob_pixels();
    const Image image{pixels.data(), compare::kSpriteSize, compare::kSpriteSize, 1,
                      PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    const Texture2D texture = LoadTextureFromImage(image);
    const RenderTexture2D target = LoadRenderTexture(compare::kWindowWidth, compare::kWindowHeight);
    auto sprites = options.iso ? std::vector<compare::Sprite>{} : compare::make_sprites(options.count);
    std::optional<compare::IsoScene> iso;
    Texture2D atlas{};
    if (options.iso) {
        iso.emplace(options.count);
        auto atlas_pixels = compare::iso_atlas_pixels();
        const Image atlas_image{atlas_pixels.data(), compare::kAtlasW, compare::kAtlasH, 1,
                                PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        atlas = LoadTextureFromImage(atlas_image);
    }
    const auto draw_iso = [&](const compare::IsoSprite& s) {
        DrawTextureRec(atlas, Rectangle{s.region.u, s.region.v, s.region.w, s.region.h},
                       Vector2{s.x, s.y}, WHITE);
    };
    compare::FrameTimer timer;

    while (!WindowShouldClose() && timer.tick()) {
        if (iso) {
            iso->update(timer.dt());
        } else {
            compare::update(sprites, timer.dt());
        }
        if (options.sorted) {
            compare::sort_by_y(sprites);
        }
        if (options.offscreen) {
            BeginTextureMode(target);
        } else {
            BeginDrawing();
        }
        ClearBackground(Color{20, 22, 28, 255});
        if (iso) {
            for (const auto& floor : iso->floors()) {
                draw_iso(floor);
            }
            for (const auto& object : iso->objects(true)) {
                draw_iso(object);
            }
        }
        for (const compare::Sprite& s : sprites) {
            DrawTextureV(texture, Vector2{s.x, s.y}, WHITE); // batched internally by rlgl
        }
        if (options.offscreen) {
            EndTextureMode();
            PollInputEvents();
        } else {
            EndDrawing();
        }
    }
    glFinish(); // include all queued GPU work in the measurement
    timer.finish();
    UnloadRenderTexture(target);
    UnloadTexture(texture);
    CloseWindow();
    timer.report("raylib", options);
}
