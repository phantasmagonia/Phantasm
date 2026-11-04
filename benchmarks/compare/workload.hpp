#pragma once

// Shared by every engine in the comparison so they run identical work:
// N 16x16 sprites bouncing inside a 1280x720 window, one texture, vsync off.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string_view>
#include <vector>

namespace compare {

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
    std::size_t count = 10'000;
    bool sorted = false;    // y-sorted draw order every frame (isometric case)
    bool offscreen = false; // render into a texture; nothing is presented to the screen
};

inline Options parse(int argc, char** argv) {
    Options options;
    if (argc > 1) {
        options.count = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
    }
    if (argc > 2) {
        options.sorted = std::string_view{argv[2]} == "sorted";
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
    bool tick() {
        const auto now = std::chrono::steady_clock::now();
        if (frame_ == kWarmupFrames) {
            measure_start_ = now;
        }
        if (frame_ > 0 && frame_ > kWarmupFrames) {
            samples_.push_back(std::chrono::duration<double, std::milli>(now - last_).count());
        }
        last_ = now;
        ++frame_;
        return frame_ <= kWarmupFrames + kMeasuredFrames + 1;
    }

    // Call after waiting for the GPU to finish all frames.
    void finish() { measure_end_ = std::chrono::steady_clock::now(); }

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
                    engine.data(), options.sorted ? "sorted" : "plain",
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

} // namespace compare
