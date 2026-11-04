#pragma once

#include <phantasm/color.hpp>
#include <phantasm/geometry.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace phantasm {

class Texture;

// Per-sprite instance data, read by the vertex shader as vertex attributes.
struct SpriteInstance {
    Rect rect;  // world space
    Rect uv;
    Color tint;
};
static_assert(sizeof(SpriteInstance) == 48);

// A contiguous range of sorted sprites sharing one texture: one draw call.
struct DrawRun {
    const Texture* texture = nullptr;
    std::uint32_t first = 0;
    std::uint32_t count = 0;
};

// One frame's sprites, stored column-wise (keys, instances, textures) so sorting
// touches only 8-byte keys and sorted instances can be written straight to GPU memory.
class SpriteQueue {
public:
    void clear() noexcept;
    void push(const Texture& texture, int layer, float depth, const SpriteInstance& instance);

    // Sorts by (layer, depth, texture), stably, and returns the draw runs.
    [[nodiscard]] std::vector<DrawRun> sort();
    // Writes instances in sorted order. `out` must hold size() elements.
    // Call after sort(); a plain memcpy when no reordering was needed.
    void write_sorted(std::span<SpriteInstance> out) const;

    [[nodiscard]] std::size_t size() const noexcept { return instances_.size(); }

private:
    std::vector<std::uint64_t> keys_;
    std::vector<SpriteInstance> instances_;
    std::vector<const Texture*> textures_;
    std::vector<std::uint32_t> order_;
    bool reordered_ = false;
};

// 64-bit key: [layer: 16 bits][depth: 32 bits][texture id: 16 bits], ordered as unsigned.
// Layers clamp to the int16 range.
[[nodiscard]] std::uint64_t sort_key(int layer, float depth, std::uint16_t texture_id) noexcept;

// Stable LSD radix sort. Writes the indices of `keys` in sorted order into `order`.
// Returns false if no pass ran, i.e. `order` is the identity and nothing moved.
bool radix_sort(std::span<const std::uint64_t> keys, std::vector<std::uint32_t>& order);

} // namespace phantasm
