#include <phantasm/application.hpp>
#include <phantasm/frame_stats.hpp>
#include <phantasm/log.hpp>
#include <phantasm/paths.hpp>
#include <phantasm/sprite.hpp>
#include <phantasm/texture.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <exception>
#include <optional>
#include <random>
#include <span>
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

constexpr std::array kBenchCounts{std::size_t{1'000}, std::size_t{10'000},
                                  std::size_t{100'000}, std::size_t{1'000'000}};
constexpr int kBenchWarmupFrames = 30;
constexpr int kBenchMeasuredFrames = 240;
constexpr double kMsPerSecond = 1000.0;

struct Options {
    std::size_t slime_count = kDefaultSlimeCount;
    bool bench = false;
};

// Runs each sprite count for a fixed number of frames with vsync off.
struct BenchRun {
    std::size_t scenario = 0;
    int frame = 0;
    std::vector<double> frame_ms;
    std::vector<double> render_cpu_ms;
    std::vector<double> update_ms;
};

struct Slime {
    phantasm::Vec2 position;
    phantasm::Vec2 velocity;
    phantasm::Color tint;
};

class Sandbox final : public phantasm::Application {
public:
    explicit Sandbox(const Options& options)
        : Application{{.title = "Phantasm Sandbox", .width = 1280, .height = 720}},
          slime_texture_{renderer().load_texture(phantasm::asset_path("assets/slime.png"))} {
        if (options.bench) {
            if (!renderer().set_vsync(false)) {
                phantasm::log_warn("Vsync could not be disabled; results are capped");
            }
            bench_.emplace();
            renderer().set_camera({.zoom = 0.5F});
            spawn_slimes(kBenchCounts[0]);
            phantasm::log_info("{:>10} | {:>9} | {:>9} | {:>9} | {:>9} | {:>11} | {:>9}",
                               "sprites", "fps", "frame avg", "frame p50", "frame p99",
                               "render CPU", "update");
        } else {
            spawn_slimes(options.slime_count);
        }
    }

protected:
    void on_update(double delta_seconds) override {
        using phantasm::Key;
        const auto dt = static_cast<float>(delta_seconds);
        const auto update_start = std::chrono::steady_clock::now();

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

        if (!bench_) {
            update_camera(dt);
        }
        for (Slime& slime : slimes_) {
            move_and_bounce(slime, dt);
        }

        const std::chrono::duration<double, std::milli> update_time =
            std::chrono::steady_clock::now() - update_start;
        if (bench_) {
            advance_bench(delta_seconds * kMsPerSecond, update_time.count());
        } else {
            report_stats(delta_seconds);
        }
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
        if (!bench_) {
            phantasm::log_info("{} slimes", count);
        }
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

    void advance_bench(double frame_ms, double update_ms) {
        BenchRun& run = *bench_;
        ++run.frame;
        if (run.frame > kBenchWarmupFrames) {
            run.frame_ms.push_back(frame_ms);
            run.update_ms.push_back(update_ms);
            run.render_cpu_ms.push_back(renderer().last_stats().cpu_ms);
        }
        if (run.frame < kBenchWarmupFrames + kBenchMeasuredFrames) {
            return;
        }

        const auto frame = phantasm::summarize(run.frame_ms);
        const auto render = phantasm::summarize(run.render_cpu_ms);
        const auto update = phantasm::summarize(run.update_ms);
        phantasm::log_info("{:>10} | {:>9.0f} | {:>6.2f} ms | {:>6.2f} ms | {:>6.2f} ms | {:>8.2f} ms | "
                           "{:>6.2f} ms",
                           slimes_.size(), kMsPerSecond / frame.mean, frame.mean, frame.p50,
                           frame.p99, render.mean, update.mean);

        if (++run.scenario >= kBenchCounts.size()) {
            request_quit();
            return;
        }
        run = BenchRun{.scenario = run.scenario};
        spawn_slimes(kBenchCounts[run.scenario]);
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
    std::optional<BenchRun> bench_;
    std::vector<Slime> slimes_;
    std::mt19937 rng_{12345};
    std::size_t color_index_ = 0;
    double elapsed_ = 0.0;
    int frames_ = 0;
};

Options parse_options(std::span<char*> args) {
    Options options;
    for (const std::string_view arg : args) {
        if (arg == "--bench") {
            options.bench = true;
            continue;
        }
        std::size_t count = 0;
        const auto [end, error] = std::from_chars(arg.data(), arg.data() + arg.size(), count);
        if (error != std::errc{} || end != arg.data() + arg.size()) {
            phantasm::log_warn("Ignoring unknown argument '{}'", arg);
            continue;
        }
        options.slime_count = count;
    }
    return options;
}

} // namespace

// Usage: sandbox [slime_count] [--bench]
// WASD/arrows pan, Q/E zoom, 1/2/3 = 1k/10k/100k slimes, Space cycles background.
// --bench runs 1k..1M sprites with vsync off and prints frame-time statistics.
int main(int argc, char** argv) {
    try {
        const std::span<char*> args{argv, static_cast<std::size_t>(argc)};
        Sandbox app{parse_options(args.subspan(1))};
        app.run();
    } catch (const std::exception& error) {
        phantasm::log_error("Fatal: {}", error.what());
        return 1;
    }
    return 0;
}
