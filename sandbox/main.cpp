#include <phantasm/application.hpp>
#include <phantasm/log.hpp>
#include <phantasm/paths.hpp>
#include <phantasm/sprite.hpp>
#include <phantasm/texture.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <exception>
#include <random>
#include <string_view>
#include <vector>

namespace {

constexpr float kSpriteScale = 4.0F;
constexpr float kWorldHalfWidth = 1200.0F;
constexpr float kWorldHalfHeight = 700.0F;
constexpr float kCameraSpeed = 600.0F; // world units per second
constexpr float kZoomRate = 1.5F;      // zoom multiplier per second
constexpr float kMinZoom = 0.1F;
constexpr float kMaxZoom = 8.0F;
constexpr float kMaxSlimeSpeed = 120.0F;
constexpr std::size_t kDefaultSlimeCount = 1000;
constexpr std::array kSlimeCounts{std::size_t{1'000}, std::size_t{10'000},
                                  std::size_t{100'000}};

struct Slime {
    phantasm::Vec2 position;
    phantasm::Vec2 velocity;
    phantasm::Color tint;
};

class Sandbox final : public phantasm::Application {
public:
    explicit Sandbox(std::size_t slime_count)
        : Application{{.title = "Phantasm Sandbox", .width = 1280, .height = 720}},
          slime_texture_{renderer().load_texture(phantasm::asset_path("assets/slime.png"))} {
        spawn_slimes(slime_count);
    }

protected:
    void on_update(double delta_seconds) override {
        using phantasm::Key;
        const auto dt = static_cast<float>(delta_seconds);

        if (input().was_pressed(Key::Escape)) {
            request_quit();
        }
        if (input().was_pressed(Key::Space)) {
            next_color();
        }
        if (input().was_pressed(Key::Num1)) {
            spawn_slimes(kSlimeCounts[0]);
        }
        if (input().was_pressed(Key::Num2)) {
            spawn_slimes(kSlimeCounts[1]);
        }
        if (input().was_pressed(Key::Num3)) {
            spawn_slimes(kSlimeCounts[2]);
        }

        update_camera(dt);
        for (Slime& slime : slimes_) {
            move_and_bounce(slime, dt);
        }
        report_stats(delta_seconds);
    }

    void on_render() override {
        const auto size = phantasm::Vec2{
            static_cast<float>(slime_texture_.width()) * kSpriteScale,
            static_cast<float>(slime_texture_.height()) * kSpriteScale,
        };
        for (const Slime& slime : slimes_) {
            // Feet pivot + depth from y: slimes lower on screen draw in front.
            renderer().draw_sprite(slime_texture_, {.position = slime.position,
                                                    .size = size,
                                                    .pivot = {0.5F, 1.0F},
                                                    .tint = slime.tint,
                                                    .depth = slime.position.y});
        }
    }

private:
    void spawn_slimes(std::size_t count) {
        std::uniform_real_distribution<float> x{-kWorldHalfWidth, kWorldHalfWidth};
        std::uniform_real_distribution<float> y{-kWorldHalfHeight, kWorldHalfHeight};
        std::uniform_real_distribution<float> speed{-kMaxSlimeSpeed, kMaxSlimeSpeed};
        std::uniform_real_distribution<float> shade{0.7F, 1.0F};

        slimes_.clear();
        slimes_.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            slimes_.push_back(Slime{
                .position = {x(rng_), y(rng_)},
                .velocity = {speed(rng_), speed(rng_)},
                .tint = {shade(rng_), shade(rng_), shade(rng_), 1.0F},
            });
        }
        phantasm::log_info("{} slimes", count);
    }

    static void move_and_bounce(Slime& slime, float dt) {
        slime.position.x += slime.velocity.x * dt;
        slime.position.y += slime.velocity.y * dt;
        if (slime.position.x < -kWorldHalfWidth || slime.position.x > kWorldHalfWidth) {
            slime.velocity.x = -slime.velocity.x;
            slime.position.x = std::clamp(slime.position.x, -kWorldHalfWidth, kWorldHalfWidth);
        }
        if (slime.position.y < -kWorldHalfHeight || slime.position.y > kWorldHalfHeight) {
            slime.velocity.y = -slime.velocity.y;
            slime.position.y = std::clamp(slime.position.y, -kWorldHalfHeight, kWorldHalfHeight);
        }
    }

    void update_camera(float dt) {
        using phantasm::Key;
        phantasm::Camera camera = renderer().camera();
        const float step = kCameraSpeed * dt / camera.zoom;
        if (input().is_down(Key::Left) || input().is_down(Key::A)) {
            camera.position.x -= step;
        }
        if (input().is_down(Key::Right) || input().is_down(Key::D)) {
            camera.position.x += step;
        }
        if (input().is_down(Key::Up) || input().is_down(Key::W)) {
            camera.position.y -= step;
        }
        if (input().is_down(Key::Down) || input().is_down(Key::S)) {
            camera.position.y += step;
        }
        if (input().is_down(Key::E)) {
            camera.zoom *= 1.0F + (kZoomRate - 1.0F) * dt * 2.0F;
        }
        if (input().is_down(Key::Q)) {
            camera.zoom /= 1.0F + (kZoomRate - 1.0F) * dt * 2.0F;
        }
        camera.zoom = std::clamp(camera.zoom, kMinZoom, kMaxZoom);
        renderer().set_camera(camera);
    }

    void report_stats(double delta_seconds) {
        elapsed_ += delta_seconds;
        ++frames_;
        if (elapsed_ >= 1.0) {
            const auto& stats = renderer().last_stats();
            phantasm::log_info("{} fps | {} sprites | {} draw calls | {:.2f} ms render CPU",
                               frames_, stats.sprites, stats.draw_calls, stats.cpu_ms);
            elapsed_ = 0.0;
            frames_ = 0;
        }
    }

    void next_color() {
        static constexpr std::array kColors{
            phantasm::Color{0.10F, 0.10F, 0.12F, 1.0F},
            phantasm::Color{0.39F, 0.58F, 0.93F, 1.0F},
            phantasm::Color{0.85F, 0.35F, 0.30F, 1.0F},
            phantasm::Color{0.95F, 0.85F, 0.60F, 1.0F},
        };
        color_index_ = (color_index_ + 1) % kColors.size();
        set_clear_color(kColors[color_index_]);
    }

    phantasm::Texture slime_texture_;
    std::vector<Slime> slimes_;
    std::mt19937 rng_{12345};
    std::size_t color_index_ = 0;
    double elapsed_ = 0.0;
    int frames_ = 0;
};

std::size_t parse_count(int argc, char** argv) {
    if (argc < 2) {
        return kDefaultSlimeCount;
    }
    const std::string_view text{argv[1]};
    std::size_t count = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), count);
    if (error != std::errc{} || end != text.data() + text.size()) {
        phantasm::log_warn("Invalid slime count '{}', using {}", text, kDefaultSlimeCount);
        return kDefaultSlimeCount;
    }
    return count;
}

} // namespace

// Usage: sandbox [slime_count]
// WASD/arrows pan, Q/E zoom, 1/2/3 = 1k/10k/100k slimes, Space cycles background.
int main(int argc, char** argv) {
    try {
        Sandbox app{parse_count(argc, argv)};
        app.run();
    } catch (const std::exception& error) {
        phantasm::log_error("Fatal: {}", error.what());
        return 1;
    }
    return 0;
}
