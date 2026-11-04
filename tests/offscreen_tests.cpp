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

protected:
    void on_update(double) override {}
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
