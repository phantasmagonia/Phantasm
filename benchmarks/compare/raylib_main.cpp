#include "workload.hpp"

#include <raylib.h>

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
    auto sprites = compare::make_sprites(options.count);
    compare::FrameTimer timer;

    while (!WindowShouldClose() && timer.tick()) {
        compare::update(sprites, timer.dt());
        if (options.sorted) {
            compare::sort_by_y(sprites);
        }
        if (options.offscreen) {
            BeginTextureMode(target);
        } else {
            BeginDrawing();
        }
        ClearBackground(Color{20, 22, 28, 255});
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
