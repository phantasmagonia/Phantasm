#include <phantasm/atlas.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <stdexcept>
#include <vector>

using phantasm::ImageView;
using phantasm::PackedRect;
using phantasm::Size;

namespace {

bool overlaps(PackedRect a, Size sa, PackedRect b, Size sb, int padding) {
    return a.x < b.x + sb.width + padding && b.x < a.x + sa.width + padding &&
           a.y < b.y + sb.height + padding && b.y < a.y + sa.height + padding;
}

} // namespace

TEST_CASE("Packing picks the smallest power-of-two square that fits") {
    const std::vector<Size> sizes{{32, 32}};

    const auto result = phantasm::pack_rects(sizes, 4096, 2);

    REQUIRE(result.has_value());
    CHECK(result->atlas_size == 64);
}

TEST_CASE("Packed rects stay inside the atlas and never overlap, padding included") {
    std::vector<Size> sizes;
    for (int i = 0; i < 40; ++i) {
        sizes.push_back(Size{8 + (i * 7) % 50, 8 + (i * 13) % 40});
    }
    constexpr int kPadding = 2;

    const auto result = phantasm::pack_rects(sizes, 4096, kPadding);

    REQUIRE(result.has_value());
    REQUIRE(result->positions.size() == sizes.size());
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        const PackedRect p = result->positions[i];
        CHECK(p.x >= 0);
        CHECK(p.y >= 0);
        CHECK(p.x + sizes[i].width <= result->atlas_size);
        CHECK(p.y + sizes[i].height <= result->atlas_size);
        for (std::size_t j = i + 1; j < sizes.size(); ++j) {
            CHECK_FALSE(overlaps(p, sizes[i], result->positions[j], sizes[j], kPadding));
        }
    }
}

TEST_CASE("Packing fails when the images cannot fit the maximum size") {
    const std::vector<Size> sizes{{100, 100}, {100, 100}};

    CHECK_FALSE(phantasm::pack_rects(sizes, 128, 2).has_value());
}

TEST_CASE("Non-positive image sizes are rejected") {
    const std::vector<Size> sizes{{0, 10}};

    CHECK_THROWS_AS(phantasm::pack_rects(sizes, 4096, 2), std::invalid_argument);
}

TEST_CASE("A region's UV rect is its pixel rect divided by the atlas size") {
    const auto uv = phantasm::region_uv(PackedRect{64, 32}, Size{16, 8}, 128);

    CHECK(uv.x == Catch::Approx(0.5F));
    CHECK(uv.y == Catch::Approx(0.25F));
    CHECK(uv.w == Catch::Approx(0.125F));
    CHECK(uv.h == Catch::Approx(0.0625F));
}

TEST_CASE("Composing copies each image to its position and leaves the rest transparent") {
    const std::vector<std::byte> red(2 * 1 * 4, std::byte{0xFF});
    const std::vector<ImageView> images{{.width = 2, .height = 1, .rgba = red}};
    const std::vector<PackedRect> positions{{1, 2}};

    const auto atlas = phantasm::compose_atlas(4, images, positions);

    REQUIRE(atlas.size() == 4 * 4 * 4);
    auto pixel_alpha = [&](int x, int y) {
        return atlas[static_cast<std::size_t>((y * 4 + x) * 4 + 3)];
    };
    CHECK(pixel_alpha(1, 2) == std::byte{0xFF});
    CHECK(pixel_alpha(2, 2) == std::byte{0xFF});
    CHECK(pixel_alpha(0, 2) == std::byte{0});
    CHECK(pixel_alpha(1, 1) == std::byte{0});
}

TEST_CASE("Composing rejects pixel data that does not match the image size") {
    const std::vector<std::byte> too_small(3);
    const std::vector<ImageView> images{{.width = 2, .height = 2, .rgba = too_small}};
    const std::vector<PackedRect> positions{{0, 0}};

    CHECK_THROWS_AS(phantasm::compose_atlas(4, images, positions), std::invalid_argument);
}
