#pragma once

// Shared by every engine in the comparison so they run identical work:
// Moving sprites or an isometric tilemap, walls, and moving characters.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace compare {

inline void save_screenshot(std::span<const std::uint8_t> rgba, int width, int height) {
    const char* path = std::getenv("COMPARE_SCREENSHOT");
    if (path == nullptr) {
        return;
    }
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        throw std::runtime_error("Cannot open comparison screenshot");
    }
    std::fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        std::fwrite(rgba.data() + i, 1, 3, file);
    }
    std::fclose(file);
}

inline constexpr int kWindowWidth = 1280;
inline constexpr int kWindowHeight = 720;
inline constexpr int kSpriteSize = 16;
inline constexpr int kWarmupFrames = 30;
inline constexpr int kMeasuredFrames = 200;

struct Sprite {
    float x;
    float y;
    float vx;
    float vy;
};

struct Options {
    std::size_t count = 10'000; // sprites, or characters in the iso scene
    bool sorted = false;    // y-sorted draw order every frame (isometric case)
    bool offscreen = false; // render into a texture; nothing is presented to the screen
    bool iso = false;       // isometric scene instead of free sprites
    bool cached = false;    // floor tiles rendered once into a texture
};

inline Options parse(int argc, char** argv) {
    Options options;
    if (argc > 1) {
        options.count = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
    }
    if (argc > 2) {
        options.sorted = std::string_view{argv[2]} == "sorted";
        options.cached = std::string_view{argv[2]} == "iso-cached";
        options.iso = std::string_view{argv[2]} == "iso" || options.cached;
    }
    if (argc > 3) {
        options.offscreen = std::string_view{argv[3]} == "offscreen";
    }
    return options;
}

inline std::vector<Sprite> make_sprites(std::size_t count) {
    std::mt19937 rng{42};
    std::uniform_real_distribution<float> x{0.0F, kWindowWidth - kSpriteSize};
    std::uniform_real_distribution<float> y{0.0F, kWindowHeight - kSpriteSize};
    std::uniform_real_distribution<float> v{-120.0F, 120.0F};
    std::vector<Sprite> sprites(count);
    for (Sprite& s : sprites) {
        s = {x(rng), y(rng), v(rng), v(rng)};
    }
    return sprites;
}

inline void update(std::vector<Sprite>& sprites, float dt) {
    constexpr float kMaxX = kWindowWidth - kSpriteSize;
    constexpr float kMaxY = kWindowHeight - kSpriteSize;
    for (Sprite& s : sprites) {
        s.x += s.vx * dt;
        s.y += s.vy * dt;
        if (s.x < 0.0F || s.x > kMaxX) {
            s.vx = -s.vx;
            s.x = std::clamp(s.x, 0.0F, kMaxX);
        }
        if (s.y < 0.0F || s.y > kMaxY) {
            s.vy = -s.vy;
            s.y = std::clamp(s.y, 0.0F, kMaxY);
        }
    }
}

// What a game on an engine without built-in depth sorting must do each frame.
inline void sort_by_y(std::vector<Sprite>& sprites) {
    std::sort(sprites.begin(), sprites.end(),
              [](const Sprite& a, const Sprite& b) { return a.y < b.y; });
}

// 16x16 RGBA8 round blob, generated in code.
inline std::vector<std::uint8_t> blob_pixels() {
    std::vector<std::uint8_t> pixels(kSpriteSize * kSpriteSize * 4, 0);
    for (int y = 0; y < kSpriteSize; ++y) {
        for (int x = 0; x < kSpriteSize; ++x) {
            const float dx = static_cast<float>(x) + 0.5F - 8.0F;
            const float dy = static_cast<float>(y) + 0.5F - 8.0F;
            if (dx * dx + dy * dy <= 49.0F) {
                const std::size_t i = static_cast<std::size_t>((y * kSpriteSize + x) * 4);
                pixels[i] = 120;
                pixels[i + 1] = 220;
                pixels[i + 2] = 140;
                pixels[i + 3] = 255;
            }
        }
    }
    return pixels;
}

// Wall-clock time between frames, including presentation.
class FrameTimer {
public:
    // Returns true while more frames should run.
    bool tick(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) {
        if (frame_ > kWarmupFrames + kMeasuredFrames) {
            return false;
        }
        if (frame_ == kWarmupFrames) {
            measure_start_ = now;
        }
        if (frame_ > 0 && frame_ > kWarmupFrames) {
            samples_.push_back(std::chrono::duration<double, std::milli>(now - last_).count());
        }
        last_ = now;
        return frame_++ < kWarmupFrames + kMeasuredFrames;
    }

    // Call after waiting for the GPU to finish all frames.
    void finish() { measure_end_ = std::chrono::steady_clock::now(); }

    [[nodiscard]] std::size_t measured_frames() const { return samples_.size(); }

    [[nodiscard]] float dt() const { return 1.0F / 60.0F; } // fixed so all engines move alike

    void report(std::string_view engine, const Options& options) const {
        std::vector<double> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        double sum = 0.0;
        for (const double s : sorted) {
            sum += s;
        }
        const auto rank = [&](double p) {
            const auto r = static_cast<std::size_t>(std::ceil(p / 100.0 * sorted.size()));
            return sorted[std::clamp<std::size_t>(r, 1, sorted.size()) - 1];
        };
        // Throughput: measured frames plus the final GPU wait, divided by frame count.
        const double throughput =
            std::chrono::duration<double, std::milli>(measure_end_ - measure_start_).count() /
            static_cast<double>(sorted.size());
        std::printf("%.*s,%s,%s,%zu,%zu,%.3f,%.3f,%.3f,%.3f\n", static_cast<int>(engine.size()),
                    engine.data(), options.cached ? "iso-cached" :
                        (options.iso ? "iso" : (options.sorted ? "sorted" : "plain")),
                    options.offscreen ? "offscreen" : "window", options.count, sorted.size(),
                    sum / static_cast<double>(sorted.size()), rank(50.0), rank(99.0), throughput);
        std::fflush(stdout);
    }

private:
    std::chrono::steady_clock::time_point last_{};
    std::chrono::steady_clock::time_point measure_start_{};
    std::chrono::steady_clock::time_point measure_end_{};
    int frame_ = 0;
    std::vector<double> samples_;
};

// ---------------------------------------------------------------------------
// Isometric scene: a 160x160 tile map (8x4 px diamonds) with walls on ~15% of tiles
// and `count` characters walking on the grid. Floors are drawn first, in a fixed
// back-to-front order; walls and characters must be drawn in depth order every
// frame because characters pass in front of and behind walls.

inline constexpr int kMapTiles = 160;
inline constexpr float kTileW = 8.0F;
inline constexpr float kTileH = 4.0F;
inline constexpr float kOriginX = kMapTiles * kTileW / 2.0F; // map spans the window width
inline constexpr float kOriginY = 40.0F;

// One atlas texture (32x16) with three regions, shared by every engine.
inline constexpr int kAtlasW = 32;
inline constexpr int kAtlasH = 16;
struct Region {
    float u, v, w, h;
};
inline constexpr Region kFloorRegion{0, 0, 8, 4};
inline constexpr Region kWallRegion{8, 0, 8, 12};
inline constexpr Region kCharacterRegion{16, 0, 6, 10};

struct IsoSprite {
    float x, y;    // top-left on screen, pixels
    Region region; // source rect in the atlas, pixels
    float depth;   // larger = nearer the viewer
};

struct Character {
    float gx, gy, vx, vy; // grid position and velocity (tiles, tiles per second)
};

inline float screen_x(float gx, float gy) {
    return kOriginX + (gx - gy) * kTileW * 0.5F;
}
inline float screen_y(float gx, float gy) {
    return kOriginY + (gx + gy) * kTileH * 0.5F;
}

class IsoScene {
public:
    explicit IsoScene(std::size_t characters) {
        std::mt19937 rng{7};
        std::uniform_real_distribution<float> wall_roll{0.0F, 1.0F};
        // Back-to-front: rows of constant gx + gy.
        for (int sum = 0; sum <= 2 * (kMapTiles - 1); ++sum) {
            for (int gx = 0; gx < kMapTiles; ++gx) {
                const int gy = sum - gx;
                if (gy < 0 || gy >= kMapTiles) {
                    continue;
                }
                const float cx = screen_x(gx + 0.5F, gy + 0.5F);
                const float cy = screen_y(gx + 0.5F, gy + 0.5F);
                floors_.push_back({cx - 4.0F, cy - 2.0F, kFloorRegion, 0.0F});
                if (wall_roll(rng) < 0.15F) {
                    walls_.push_back({cx - 4.0F, cy + 2.0F - 12.0F, kWallRegion,
                                      static_cast<float>(gx + gy + 1)});
                }
            }
        }
        std::uniform_real_distribution<float> position{0.0F, static_cast<float>(kMapTiles)};
        std::uniform_real_distribution<float> speed{-2.0F, 2.0F};
        characters_.resize(characters);
        for (Character& c : characters_) {
            c = {position(rng), position(rng), speed(rng), speed(rng)};
        }
    }

    void update(float dt) {
        constexpr auto kMax = static_cast<float>(kMapTiles);
        for (Character& c : characters_) {
            c.gx += c.vx * dt;
            c.gy += c.vy * dt;
            if (c.gx < 0.0F || c.gx > kMax) {
                c.vx = -c.vx;
                c.gx = std::clamp(c.gx, 0.0F, kMax);
            }
            if (c.gy < 0.0F || c.gy > kMax) {
                c.vy = -c.vy;
                c.gy = std::clamp(c.gy, 0.0F, kMax);
            }
        }
    }

    // Walls plus characters for this frame; sorted by depth if the engine can't sort.
    const std::vector<IsoSprite>& objects(bool sort) {
        objects_ = walls_;
        for (const Character& c : characters_) {
            const float sx = screen_x(c.gx, c.gy);
            const float sy = screen_y(c.gx, c.gy);
            objects_.push_back({sx - 3.0F, sy - 9.0F, kCharacterRegion, c.gx + c.gy});
        }
        if (sort) {
            std::stable_sort(objects_.begin(), objects_.end(),
                             [](const IsoSprite& a, const IsoSprite& b) { return a.depth < b.depth; });
        }
        return objects_;
    }

    [[nodiscard]] const std::vector<IsoSprite>& floors() const { return floors_; }
    [[nodiscard]] const std::vector<IsoSprite>& walls() const { return walls_; }
    [[nodiscard]] const std::vector<Character>& characters() const { return characters_; }

private:
    std::vector<IsoSprite> floors_;
    std::vector<IsoSprite> walls_;
    std::vector<Character> characters_;
    std::vector<IsoSprite> objects_;
};

// RGBA8 atlas: floor diamond, wall block, character capsule.
inline std::vector<std::uint8_t> iso_atlas_pixels() {
    std::vector<std::uint8_t> pixels(kAtlasW * kAtlasH * 4, 0);
    const auto put = [&](int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
        const auto i = static_cast<std::size_t>((y * kAtlasW + x) * 4);
        pixels[i] = r;
        pixels[i + 1] = g;
        pixels[i + 2] = b;
        pixels[i + 3] = 255;
    };
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (std::abs(x + 0.5F - 4.0F) / 4.0F + std::abs(y + 0.5F - 2.0F) / 2.0F <= 1.0F) {
                put(x, y, 150, 110, 74);
            }
        }
    }
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 8; ++x) {
            put(8 + x, y, y < 4 ? 200 : (x < 4 ? 150 : 112), y < 4 ? 196 : (x < 4 ? 146 : 108),
                y < 4 ? 186 : (x < 4 ? 138 : 102));
        }
    }
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 6; ++x) {
            put(16 + x, y, 70, 120, 200);
        }
    }
    return pixels;
}

} // namespace compare
