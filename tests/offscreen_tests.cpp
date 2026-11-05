#include <phantasm/application.hpp>
#include <phantasm/pixel_format.hpp>
#include <phantasm/renderer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

TEST_CASE("BGRA pixels are swizzled to RGBA in place") {
    std::vector<std::byte> pixels{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
                                  std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};

    phantasm::bgra_to_rgba(pixels);

    CHECK(pixels == std::vector<std::byte>{std::byte{3}, std::byte{2}, std::byte{1}, std::byte{4},
                                           std::byte{7}, std::byte{6}, std::byte{5}, std::byte{8}});
}

TEST_CASE("Swizzling rejects data that is not whole pixels") {
    std::vector<std::byte> pixels(5);
    CHECK_THROWS_AS(phantasm::bgra_to_rgba(pixels), std::invalid_argument);
}

namespace {

// Exposes the renderer of a hidden-window application for direct GPU tests.
class GpuHarness final : public phantasm::Application {
public:
    GpuHarness() : Application{{.title = "gpu test", .width = 64, .height = 64, .hidden = true}} {}
    using Application::renderer;
    bool quit_on_update = false;
    int rendered_frames = 0;

protected:
    void on_update(double) override {
        if (quit_on_update) {
            request_quit();
        }
    }
    void on_render() override { ++rendered_frames; }
};

std::unique_ptr<GpuHarness> make_harness() {
    try {
        return std::make_unique<GpuHarness>();
    } catch (const std::exception&) {
        return nullptr; // no display or GPU (e.g. headless CI)
    }
}

std::array<int, 4> pixel_at(const std::vector<std::byte>& rgba, int width, int x, int y) {
    const auto i = static_cast<std::size_t>((y * width + x) * 4);
    return {std::to_integer<int>(rgba[i]), std::to_integer<int>(rgba[i + 1]),
            std::to_integer<int>(rgba[i + 2]), std::to_integer<int>(rgba[i + 3])};
}

} // namespace

TEST_CASE("Offscreen frames render sprites and read back as RGBA") {
    auto harness = make_harness();
    if (!harness) {
        SKIP("No GPU device available");
    }
    phantasm::Renderer& renderer = harness->renderer();
    renderer.set_offscreen(32, 16);
    renderer.set_camera({.position = {16.0F, 8.0F}}); // world units = pixels, origin top-left

    const std::vector<std::byte> red{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
    const phantasm::Texture texture = renderer.create_texture(1, 1, red);

    REQUIRE(renderer.begin_frame({0.0F, 0.0F, 1.0F, 1.0F})); // blue background
    renderer.draw_sprite(texture, {.position = {0.0F, 0.0F}, .size = {8.0F, 8.0F}});
    renderer.end_frame();
    const auto pixels = renderer.read_offscreen_pixels();

    REQUIRE(pixels.size() == 32U * 16U * 4U);
    CHECK(pixel_at(pixels, 32, 2, 2) == std::array{255, 0, 0, 255});  // inside the sprite
    CHECK(pixel_at(pixels, 32, 20, 12) == std::array{0, 0, 255, 255}); // background
}

TEST_CASE("Reading pixels without an offscreen target is an error") {
    auto harness = make_harness();
    if (!harness) {
        SKIP("No GPU device available");
    }
    CHECK_THROWS_AS(harness->renderer().read_offscreen_pixels(), std::logic_error);
}

TEST_CASE("Quitting during update does not render an extra frame") {
    auto harness = make_harness();
    if (!harness) {
        SKIP("No GPU device available");
    }
    harness->renderer().set_offscreen(16, 16);
    harness->quit_on_update = true;
    harness->run();
    CHECK(harness->rendered_frames == 0);
}

TEST_CASE("Cached layers preserve transparent edges and tinted alpha") {
    auto harness = make_harness();
    if (!harness) {
        SKIP("No GPU device available");
    }
    auto& renderer = harness->renderer();
    renderer.set_offscreen(16, 16);
    renderer.set_camera({.position = {8, 8}});
    const std::array<std::byte, 4> rgba{
        std::byte{255}, std::byte{0}, std::byte{0}, std::byte{128}};
    auto source = renderer.create_texture(1, 1, rgba);
    const std::array<phantasm::Sprite, 1> sprites{{
        {.position = {2, 2}, .size = {8, 8}}}};
    auto cached = renderer.cache_layer(source, sprites, 16, 16);

    REQUIRE(renderer.begin_frame({0, 0, 1, 1}));
    renderer.draw_sprite(source, {.position = {2, 2}, .size = {8, 8},
                                  .tint = {1, 1, 1, 0.5F}});
    renderer.end_frame();
    const auto direct = renderer.read_offscreen_pixels();

    REQUIRE(renderer.begin_frame({0, 0, 1, 1}));
    renderer.draw_sprite(cached, {.size = {16, 16}, .tint = {1, 1, 1, 0.5F}});
    renderer.end_frame();
    const auto composed = renderer.read_offscreen_pixels();
    REQUIRE(direct.size() == composed.size());
    for (std::size_t i = 0; i < direct.size(); ++i) {
        CHECK(std::abs(std::to_integer<int>(direct[i]) - std::to_integer<int>(composed[i])) <= 1);
    }
    CHECK(pixel_at(composed, 16, 0, 0) == std::array{0, 0, 255, 255});
    CHECK_THROWS_AS(renderer.cache_layer(source, sprites, 0, 16), std::invalid_argument);
    REQUIRE(renderer.begin_frame({0, 0, 0, 1}));
    CHECK_THROWS_AS(renderer.cache_layer(source, sprites, 16, 16), std::logic_error);
    renderer.end_frame();
}

TEST_CASE("The renderer maps screen pixels to world positions through its camera") {
    auto harness = make_harness();
    if (!harness) {
        SKIP("No GPU device available");
    }
    phantasm::Renderer& renderer = harness->renderer();
    renderer.set_offscreen(64, 32);
    renderer.set_camera({.position = {100.0F, 50.0F}, .zoom = 2.0F});
    REQUIRE(renderer.begin_frame({0.0F, 0.0F, 0.0F, 1.0F}));
    renderer.end_frame();

    const phantasm::Vec2 center = renderer.screen_to_world({32.0F, 16.0F});
    const phantasm::Vec2 corner = renderer.screen_to_world({0.0F, 0.0F});

    CHECK(center.x == 100.0F);
    CHECK(center.y == 50.0F);
    CHECK(corner.x == 84.0F);
    CHECK(corner.y == 42.0F);
}

namespace {

std::vector<std::byte> red_blue_pixels() {
    return {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255},
            std::byte{0},   std::byte{0}, std::byte{255}, std::byte{255}};
}

// 64x64 black/white checkerboard of 1-pixel squares: averages to mid gray.
std::vector<std::byte> fine_checker() {
    std::vector<std::byte> pixels(64U * 64U * 4U);
    for (std::size_t y = 0; y < 64; ++y) {
        for (std::size_t x = 0; x < 64; ++x) {
            const auto value = std::byte{((x + y) % 2 == 0) ? std::uint8_t{255} : std::uint8_t{0}};
            const std::size_t i = (y * 64 + x) * 4;
            pixels[i] = value;
            pixels[i + 1] = value;
            pixels[i + 2] = value;
            pixels[i + 3] = std::byte{255};
        }
    }
    return pixels;
}

std::vector<std::byte> render_scaled(phantasm::Renderer& renderer, const phantasm::Texture& texture,
                                     float width, float height) {
    REQUIRE(renderer.begin_frame({0.0F, 0.0F, 0.0F, 1.0F}));
    renderer.draw_sprite(texture, {.position = {0.0F, 0.0F}, .size = {width, height}});
    renderer.end_frame();
    return renderer.read_offscreen_pixels();
}

} // namespace

TEST_CASE("Nearest filtering keeps hard texel edges when magnified") {
    auto harness = make_harness();
    if (!harness) {
        SKIP("No GPU device available");
    }
    phantasm::Renderer& renderer = harness->renderer();
    renderer.set_offscreen(64, 8);
    renderer.set_camera({.position = {32.0F, 4.0F}});
    const auto texture = renderer.create_texture(2, 1, red_blue_pixels());

    const auto pixels = render_scaled(renderer, texture, 64.0F, 8.0F);

    CHECK(pixel_at(pixels, 64, 30, 4) == std::array{255, 0, 0, 255});
    CHECK(pixel_at(pixels, 64, 33, 4) == std::array{0, 0, 255, 255});
}

TEST_CASE("Linear filtering blends neighboring texels when magnified") {
    auto harness = make_harness();
    if (!harness) {
        SKIP("No GPU device available");
    }
    phantasm::Renderer& renderer = harness->renderer();
    renderer.set_offscreen(64, 8);
    renderer.set_camera({.position = {32.0F, 4.0F}});
    const auto texture =
        renderer.create_texture(2, 1, red_blue_pixels(), phantasm::TextureFilter::Linear);
    CHECK(texture.filter() == phantasm::TextureFilter::Linear);

    const auto pixels = render_scaled(renderer, texture, 64.0F, 8.0F);
    const auto middle = pixel_at(pixels, 64, 32, 4);

    CHECK(middle[0] > 80);
    CHECK(middle[0] < 175);
    CHECK(middle[2] > 80);
    CHECK(middle[2] < 175);
}

TEST_CASE("Linear textures use mipmaps when minified, averaging fine detail") {
    auto harness = make_harness();
    if (!harness) {
        SKIP("No GPU device available");
    }
    phantasm::Renderer& renderer = harness->renderer();
    renderer.set_offscreen(4, 4);
    renderer.set_camera({.position = {2.0F, 2.0F}});
    const auto smooth =
        renderer.create_texture(64, 64, fine_checker(), phantasm::TextureFilter::Linear);

    const auto pixels = render_scaled(renderer, smooth, 4.0F, 4.0F);

    // 16x16 checker texels per screen pixel average to ~128; without mipmaps a single
    // texel (0 or 255) would be sampled.
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const int gray = pixel_at(pixels, 4, x, y)[0];
            CHECK(gray > 100);
            CHECK(gray < 156);
        }
    }
}
