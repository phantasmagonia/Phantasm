#include "workload.hpp"

#include <SDL3/SDL.h>

#include <optional>

int main(int argc, char** argv) {
    const auto options = compare::parse(argc, argv);
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_CreateWindowAndRenderer("SDL_Renderer", compare::kWindowWidth, compare::kWindowHeight,
                                options.offscreen ? SDL_WINDOW_HIDDEN : 0, &window, &renderer);
    SDL_SetRenderVSync(renderer, 0);

    const auto pixels = compare::blob_pixels();
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC, compare::kSpriteSize,
                                             compare::kSpriteSize);
    SDL_UpdateTexture(texture, nullptr, pixels.data(), compare::kSpriteSize * 4);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_Texture* offscreen = nullptr;
    if (options.offscreen) {
        offscreen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
                                      compare::kWindowWidth, compare::kWindowHeight);
        SDL_SetRenderTarget(renderer, offscreen);
    }
    auto sprites = options.iso ? std::vector<compare::Sprite>{} : compare::make_sprites(options.count);
    std::optional<compare::IsoScene> iso;
    SDL_Texture* atlas = nullptr;
    if (options.iso) {
        iso.emplace(options.count);
        const auto atlas_pixels = compare::iso_atlas_pixels();
        atlas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                  compare::kAtlasW, compare::kAtlasH);
        SDL_UpdateTexture(atlas, nullptr, atlas_pixels.data(), compare::kAtlasW * 4);
        SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(atlas, SDL_SCALEMODE_NEAREST);
    }
    const auto draw_iso = [&](const compare::IsoSprite& s) {
        const SDL_FRect source{s.region.u, s.region.v, s.region.w, s.region.h};
        const SDL_FRect target{s.x, s.y, s.region.w, s.region.h};
        SDL_RenderTexture(renderer, atlas, &source, &target);
    };
    compare::FrameTimer timer;

    bool running = true;
    while (running && timer.tick()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            running = running && event.type != SDL_EVENT_QUIT;
        }
        if (iso) {
            iso->update(timer.dt());
        } else {
            compare::update(sprites, timer.dt());
        }
        if (options.sorted) {
            compare::sort_by_y(sprites);
        }
        SDL_SetRenderDrawColor(renderer, 20, 22, 28, 255);
        SDL_RenderClear(renderer);
        if (iso) {
            for (const auto& floor : iso->floors()) {
                draw_iso(floor);
            }
            for (const auto& object : iso->objects(true)) {
                draw_iso(object);
            }
        }
        for (const compare::Sprite& s : sprites) {
            const SDL_FRect target{s.x, s.y, compare::kSpriteSize, compare::kSpriteSize};
            SDL_RenderTexture(renderer, texture, nullptr, &target); // batched internally
        }
        if (offscreen != nullptr) {
            SDL_FlushRenderer(renderer);
        } else {
            SDL_RenderPresent(renderer);
        }
    }
    // Reading a pixel back waits for all queued GPU work.
    const SDL_Rect one_pixel{0, 0, 1, 1};
    SDL_DestroySurface(SDL_RenderReadPixels(renderer, &one_pixel));
    timer.finish();
    if (offscreen != nullptr) {
        SDL_DestroyTexture(offscreen);
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    timer.report("sdl_renderer", options);
}
