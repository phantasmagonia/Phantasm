#include <phantasm/atlas.hpp>

#include <phantasm/renderer.hpp>

#include <stb_rect_pack.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace phantasm {

namespace {

constexpr int kMinAtlasSize = 64;
constexpr std::size_t kBytesPerPixel = 4;

std::optional<std::vector<PackedRect>> try_pack(std::span<const Size> sizes, int atlas_size,
                                                int padding) {
    std::vector<stbrp_rect> rects(sizes.size());
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        rects[i].id = static_cast<int>(i);
        // Padding is added to the right and bottom of every image.
        rects[i].w = sizes[i].width + padding;
        rects[i].h = sizes[i].height + padding;
    }
    std::vector<stbrp_node> nodes(static_cast<std::size_t>(atlas_size));
    stbrp_context context{};
    stbrp_init_target(&context, atlas_size, atlas_size, nodes.data(),
                      static_cast<int>(nodes.size()));
    if (stbrp_pack_rects(&context, rects.data(), static_cast<int>(rects.size())) == 0) {
        return std::nullopt;
    }
    std::vector<PackedRect> positions(sizes.size());
    for (const stbrp_rect& rect : rects) {
        positions[static_cast<std::size_t>(rect.id)] = PackedRect{rect.x, rect.y};
    }
    return positions;
}

std::size_t byte_size(int width, int height) {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kBytesPerPixel;
}

} // namespace

std::optional<PackResult> pack_rects(std::span<const Size> sizes, int max_size, int padding) {
    for (const Size& size : sizes) {
        if (size.width <= 0 || size.height <= 0) {
            throw std::invalid_argument("Atlas images must have positive width and height");
        }
    }
    for (int atlas_size = kMinAtlasSize; atlas_size <= max_size; atlas_size *= 2) {
        if (auto positions = try_pack(sizes, atlas_size, padding)) {
            return PackResult{atlas_size, std::move(*positions)};
        }
    }
    return std::nullopt;
}

Rect region_uv(PackedRect position, Size size, int atlas_size) noexcept {
    const auto scale = 1.0F / static_cast<float>(atlas_size);
    return Rect{
        .x = static_cast<float>(position.x) * scale,
        .y = static_cast<float>(position.y) * scale,
        .w = static_cast<float>(size.width) * scale,
        .h = static_cast<float>(size.height) * scale,
    };
}

std::vector<std::byte> compose_atlas(int atlas_size, std::span<const ImageView> images,
                                     std::span<const PackedRect> positions) {
    if (images.size() != positions.size()) {
        throw std::invalid_argument("compose_atlas: one position per image required");
    }
    std::vector<std::byte> atlas(byte_size(atlas_size, atlas_size), std::byte{0});
    const std::size_t atlas_row_bytes = static_cast<std::size_t>(atlas_size) * kBytesPerPixel;

    for (std::size_t i = 0; i < images.size(); ++i) {
        const ImageView& image = images[i];
        const PackedRect at = positions[i];
        if (image.rgba.size() != byte_size(image.width, image.height)) {
            throw std::invalid_argument("compose_atlas: pixel data size mismatch for image " +
                                        std::to_string(i));
        }
        if (at.x < 0 || at.y < 0 || at.x + image.width > atlas_size ||
            at.y + image.height > atlas_size) {
            throw std::invalid_argument("compose_atlas: image " + std::to_string(i) +
                                        " lies outside the atlas");
        }
        const std::size_t row_bytes = static_cast<std::size_t>(image.width) * kBytesPerPixel;
        for (int row = 0; row < image.height; ++row) {
            const std::size_t source = static_cast<std::size_t>(row) * row_bytes;
            const std::size_t target = static_cast<std::size_t>(at.y + row) * atlas_row_bytes +
                                       static_cast<std::size_t>(at.x) * kBytesPerPixel;
            std::memcpy(atlas.data() + target, image.rgba.data() + source, row_bytes);
        }
    }
    return atlas;
}

Atlas build_atlas(Renderer& renderer, std::span<const ImageView> images, int max_size,
                  int padding) {
    std::vector<Size> sizes;
    sizes.reserve(images.size());
    for (const ImageView& image : images) {
        sizes.push_back(Size{image.width, image.height});
    }
    const auto packed = pack_rects(sizes, max_size, padding);
    if (!packed) {
        throw std::runtime_error("Atlas images do not fit in " + std::to_string(max_size) + "x" +
                                 std::to_string(max_size));
    }
    const auto pixels = compose_atlas(packed->atlas_size, images, packed->positions);

    std::vector<Rect> uvs;
    uvs.reserve(images.size());
    for (std::size_t i = 0; i < images.size(); ++i) {
        uvs.push_back(region_uv(packed->positions[i], sizes[i], packed->atlas_size));
    }
    return Atlas{
        .texture = renderer.create_texture(packed->atlas_size, packed->atlas_size, pixels),
        .uvs = std::move(uvs),
        .sizes = std::move(sizes),
    };
}

} // namespace phantasm
