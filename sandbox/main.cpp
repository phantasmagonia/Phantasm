#include <phantasm/application.hpp>
#include <phantasm/atlas.hpp>
#include <phantasm/frame_stats.hpp>
#include <phantasm/log.hpp>
#include <phantasm/sprite.hpp>
#include <phantasm/texture.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
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
constexpr double kMsPerSecond = 1000.0;

constexpr int kBlobSize = 16;
constexpr std::size_t kVariantCount = 16;

// How sprites get their pixels; the benchmark compares these.
enum class TextureMode { Single, Separate, Atlas };

constexpr std::string_view mode_name(TextureMode mode) {
    switch (mode) {
    case TextureMode::Single: return "1 texture";
    case TextureMode::Separate: return "16 textures";
    case TextureMode::Atlas: return "16-image atlas";
    }
    return "?";
}

constexpr float kOverviewZoom = 0.5F; // whole world visible
constexpr float kCloseZoom = 4.0F;     // ~1% of the world visible, like a large map

struct BenchScenario {
    std::size_t sprites;
    TextureMode mode;
    float zoom = kOverviewZoom;
};

constexpr std::array kBenchScenarios{
    BenchScenario{1'000, TextureMode::Single},
    BenchScenario{10'000, TextureMode::Single},
    BenchScenario{100'000, TextureMode::Single},
    BenchScenario{1'000'000, TextureMode::Single},
    BenchScenario{10'000, TextureMode::Separate},
    BenchScenario{10'000, TextureMode::Atlas},
    BenchScenario{100'000, TextureMode::Separate},
    BenchScenario{100'000, TextureMode::Atlas},
    BenchScenario{1'000'000, TextureMode::Atlas, kCloseZoom},
};
constexpr int kBenchWarmupFrames = 30;
constexpr int kBenchMeasuredFrames = 240;

// Placeholder sprite generated in code (no art files): a shaded round blob.
// `variant` changes its radius slightly so the variants are distinct images.
std::vector<std::byte> make_blob_pixels(std::size_t variant) {
    std::vector<std::byte> pixels(static_cast<std::size_t>(kBlobSize * kBlobSize * 4));
    const float center = static_cast<float>(kBlobSize) / 2.0F;
    const float radius = center - 1.0F - static_cast<float>(variant % 4) * 0.5F;
    for (int y = 0; y < kBlobSize; ++y) {
        for (int x = 0; x < kBlobSize; ++x) {
            const float dx = static_cast<float>(x) + 0.5F - center;
            const float dy = static_cast<float>(y) + 0.5F - center;
            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }
            // Lighter toward the top-left, darker toward the bottom-right.
            const float light = std::clamp(1.0F - (dx + dy) / (radius * 4.0F), 0.5F, 1.0F);
            const auto shade = static_cast<std::uint8_t>(255.0F * light);
            const auto index = static_cast<std::size_t>((y * kBlobSize + x) * 4);
            pixels[index + 0] = std::byte{shade};
            pixels[index + 1] = std::byte{shade};
            pixels[index + 2] = std::byte{shade};
            pixels[index + 3] = std::byte{255};
        }
    }
    return pixels;
}

std::vector<std::vector<std::byte>> make_variants() {
    std::vector<std::vector<std::byte>> variants;
    for (std::size_t i = 0; i < kVariantCount; ++i) {
        variants.push_back(make_blob_pixels(i));
    }
    return variants;
}

struct Options {
    std::size_t slime_count = kDefaultSlimeCount;
    bool bench = false;
};

// Runs each scenario for a fixed number of frames with vsync off.
struct BenchRun {
    std::size_t scenario = 0;
    int frame = 0;
    std::vector<double> frame_ms;
    std::vector<double> render_cpu_ms;
    std::vector<double> submit_ms;
    std::vector<double> draw_calls;
    std::vector<double> visible;
};

struct Slime {
    phantasm::Vec2 position;
    phantasm::Vec2 velocity;
    phantasm::Color tint;
    std::size_t variant = 0;
};

class Sandbox final : public phantasm::Application {
public:
    explicit Sandbox(const Options& options)
        : Application{{.title = "Phantasm Sandbox", .width = 1280, .height = 720}},
          variant_pixels_{make_variants()},
          single_texture_{renderer().create_texture(kBlobSize, kBlobSize, variant_pixels_[0])},
          separate_textures_{make_separate_textures()},
          atlas_{phantasm::build_atlas(renderer(), variant_views())} {
        if (options.bench) {
            start_bench();
        } else {
            spawn_slimes(options.slime_count);
        }
    }

protected:
    void on_update(double delta_seconds) override {
        const auto dt = static_cast<float>(delta_seconds);
        if (input().was_pressed(phantasm::Key::Escape)) {
            request_quit();
        }
        if (!bench_) {
            handle_interactive_keys();
            update_camera(dt);
        }
        for (Slime& slime : slimes_) {
            move_and_bounce(slime, dt);
        }
        if (bench_) {
            advance_bench(delta_seconds * kMsPerSecond);
        } else {
            report_stats(delta_seconds);
        }
    }

    void on_render() override {
        const phantasm::Vec2 size{static_cast<float>(kBlobSize) * kSpriteScale,
                                  static_cast<float>(kBlobSize) * kSpriteScale};
        for (const Slime& slime : slimes_) {
            // Feet pivot + depth from y: lower slimes draw in front.
            phantasm::Sprite sprite{.position = slime.position,
                                    .size = size,
                                    .pivot = {0.5F, 1.0F},
                                    .tint = slime.tint,
                                    .depth = slime.position.y};
            const phantasm::Texture* texture = &single_texture_;
            if (mode_ == TextureMode::Separate) {
                texture = &separate_textures_[slime.variant];
            } else if (mode_ == TextureMode::Atlas) {
                texture = &atlas_.texture;
                sprite.uv = atlas_.uvs[slime.variant];
            }
            renderer().draw_sprite(*texture, sprite);
        }
    }

private:
    std::vector<phantasm::Texture> make_separate_textures() {
        std::vector<phantasm::Texture> textures;
        for (const auto& pixels : variant_pixels_) {
            textures.push_back(renderer().create_texture(kBlobSize, kBlobSize, pixels));
        }
        return textures;
    }

    std::vector<phantasm::ImageView> variant_views() const {
        std::vector<phantasm::ImageView> views;
        for (const auto& pixels : variant_pixels_) {
            views.push_back({.width = kBlobSize, .height = kBlobSize, .rgba = pixels});
        }
        return views;
    }

    void handle_interactive_keys() {
        using phantasm::Key;
        if (input().was_pressed(Key::Space)) {
            next_color();
        }
        if (input().was_pressed(Key::T)) {
            mode_ = static_cast<TextureMode>((static_cast<int>(mode_) + 1) % 3);
            phantasm::log_info("Texture mode: {}", mode_name(mode_));
        }
        const std::array keys{Key::Num1, Key::Num2, Key::Num3};
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (input().was_pressed(keys[i])) {
                spawn_slimes(kSlimeCounts[i]);
            }
        }
    }

    void spawn_slimes(std::size_t count) {
        std::uniform_real_distribution<float> x{-kWorldHalfWidth, kWorldHalfWidth};
        std::uniform_real_distribution<float> y{-kWorldHalfHeight, kWorldHalfHeight};
        std::uniform_real_distribution<float> speed{-kMaxSlimeSpeed, kMaxSlimeSpeed};
        std::uniform_real_distribution<float> shade{0.7F, 1.0F};
        std::uniform_int_distribution<std::size_t> variant{0, kVariantCount - 1};

        slimes_.clear();
        slimes_.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            slimes_.push_back(Slime{
                .position = {x(rng_), y(rng_)},
                .velocity = {speed(rng_), speed(rng_)},
                .tint = {shade(rng_) * 0.4F, shade(rng_), shade(rng_) * 0.5F, 1.0F},
                .variant = variant(rng_),
            });
        }
        if (!bench_) {
            phantasm::log_info("{} slimes ({})", count, mode_name(mode_));
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

    void start_bench() {
        if (!renderer().set_vsync(false)) {
            phantasm::log_warn("Vsync could not be disabled; results are capped");
        }
        bench_.emplace();
        begin_scenario(0);
        phantasm::log_info(
            "{:>9} | {:>14} | {:>4} | {:>9} | {:>5} | {:>9} | {:>9} | {:>9} | {:>9} | {:>6}",
            "sprites", "textures", "zoom", "visible", "fps", "frame p50", "frame p99", "submit",
            "end_frame", "draws");
    }

    void begin_scenario(std::size_t index) {
        mode_ = kBenchScenarios[index].mode;
        renderer().set_camera({.zoom = kBenchScenarios[index].zoom});
        spawn_slimes(kBenchScenarios[index].sprites);
    }

    void advance_bench(double frame_ms) {
        BenchRun& run = *bench_;
        ++run.frame;
        if (run.frame > kBenchWarmupFrames) {
            const auto& stats = renderer().last_stats();
            run.frame_ms.push_back(frame_ms);
            run.render_cpu_ms.push_back(stats.cpu_ms);
            run.submit_ms.push_back(stats.submit_ms);
            run.draw_calls.push_back(static_cast<double>(stats.draw_calls));
            run.visible.push_back(static_cast<double>(stats.sprites));
        }
        if (run.frame < kBenchWarmupFrames + kBenchMeasuredFrames) {
            return;
        }

        const auto frame = phantasm::summarize(run.frame_ms);
        const auto render = phantasm::summarize(run.render_cpu_ms);
        const auto submit = phantasm::summarize(run.submit_ms);
        const auto draws = phantasm::summarize(run.draw_calls);
        const auto visible = phantasm::summarize(run.visible);
        phantasm::log_info(
            "{:>9} | {:>14} | {:>4.1f} | {:>9.0f} | {:>5.0f} | {:>6.2f} ms | {:>6.2f} ms | "
            "{:>6.2f} ms | {:>6.2f} ms | {:>6.0f}",
            slimes_.size(), mode_name(mode_), renderer().camera().zoom, visible.mean,
            kMsPerSecond / frame.mean, frame.p50, frame.p99, submit.mean, render.mean,
            draws.mean);

        if (++run.scenario >= kBenchScenarios.size()) {
            request_quit();
            return;
        }
        run = BenchRun{.scenario = run.scenario};
        begin_scenario(run.scenario);
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

    // Declared before the textures: they are created from these pixels.
    std::vector<std::vector<std::byte>> variant_pixels_;
    phantasm::Texture single_texture_;
    std::vector<phantasm::Texture> separate_textures_;
    phantasm::Atlas atlas_;
    TextureMode mode_ = TextureMode::Single;
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
// WASD/arrows pan, Q/E zoom, 1/2/3 = 1k/10k/100k slimes, T cycles texture mode,
// Space cycles background. --bench runs fixed scenarios with vsync off.
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
