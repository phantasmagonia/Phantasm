#include <phantasm/sprite_batch.hpp>

#include <phantasm/texture.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <cstddef>
#include <limits>

namespace phantasm {

namespace {

constexpr int kDigitBits = 8;
constexpr std::size_t kBuckets = std::size_t{1} << kDigitBits;
constexpr int kPasses = 64 / kDigitBits;
constexpr std::uint64_t kDigitMask = kBuckets - 1;

constexpr std::uint32_t kSignBit = 0x8000'0000U;
constexpr int kLayerBias = 32'768;

// Maps a float to an unsigned int with the same ordering (IEEE 754 trick):
// positives get the sign bit set; negatives are bit-inverted.
std::uint32_t orderable_bits(float value) noexcept {
    const auto bits = std::bit_cast<std::uint32_t>(value + 0.0F); // -0 becomes +0
    return (bits & kSignBit) != 0 ? ~bits : bits | kSignBit;
}

// Reused between frames so sorting does not allocate once buffers have grown.
struct SortScratch {
    std::vector<std::uint64_t> keys;
    std::vector<std::uint64_t> keys_swap;
    std::vector<std::uint32_t> order_swap;
};

SortScratch& scratch() {
    thread_local SortScratch instance;
    return instance;
}

} // namespace

std::uint64_t sort_key(int layer, float depth, std::uint16_t texture_id) noexcept {
    const int clamped = std::clamp(layer, static_cast<int>(std::numeric_limits<std::int16_t>::min()),
                                   static_cast<int>(std::numeric_limits<std::int16_t>::max()));
    const auto layer_bits = static_cast<std::uint64_t>(clamped + kLayerBias);
    return (layer_bits << 48) | (static_cast<std::uint64_t>(orderable_bits(depth)) << 16) |
           texture_id;
}

bool radix_sort(std::span<const std::uint64_t> keys, std::vector<std::uint32_t>& order) {
    const std::size_t count = keys.size();
    order.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        order[i] = i;
    }
    if (count < 2) {
        return false;
    }

    // One read pass builds the histograms for all eight digits.
    std::array<std::array<std::uint32_t, kBuckets>, kPasses> histograms{};
    for (const std::uint64_t key : keys) {
        for (int pass = 0; pass < kPasses; ++pass) {
            ++histograms[static_cast<std::size_t>(pass)][(key >> (pass * kDigitBits)) & kDigitMask];
        }
    }

    SortScratch& buffers = scratch();
    buffers.keys.assign(keys.begin(), keys.end());
    buffers.keys_swap.resize(count);
    buffers.order_swap.resize(count);

    bool moved = false;
    for (int pass = 0; pass < kPasses; ++pass) {
        auto& histogram = histograms[static_cast<std::size_t>(pass)];
        const int shift = pass * kDigitBits;
        // Every key has the same digit here: this pass would not change the order.
        if (histogram[(buffers.keys[0] >> shift) & kDigitMask] == count) {
            continue;
        }
        std::uint32_t offset = 0;
        for (std::uint32_t& bucket : histogram) {
            const std::uint32_t bucket_count = bucket;
            bucket = offset;
            offset += bucket_count;
        }
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint64_t key = buffers.keys[i];
            const std::uint32_t target = histogram[(key >> shift) & kDigitMask]++;
            buffers.keys_swap[target] = key;
            buffers.order_swap[target] = order[i];
        }
        buffers.keys.swap(buffers.keys_swap);
        order.swap(buffers.order_swap);
        moved = true;
    }
    return moved;
}

SpriteInstance pack_instance(Rect rect, Rect uv, Color tint) noexcept {
    // Add 0.5 and truncate: rounds like lround for these non-negative values, but
    // compiles to a few instructions instead of a library call per channel.
    const auto unorm16 = [](float value) {
        return static_cast<std::uint16_t>(std::clamp(value, 0.0F, 1.0F) * 65535.0F + 0.5F);
    };
    const auto unorm8 = [](float value) {
        return static_cast<std::uint8_t>(std::clamp(value, 0.0F, 1.0F) * 255.0F + 0.5F);
    };
    return SpriteInstance{
        .rect = rect,
        .uv = {unorm16(uv.x), unorm16(uv.y), unorm16(uv.w), unorm16(uv.h)},
        .tint = {unorm8(tint.r), unorm8(tint.g), unorm8(tint.b), unorm8(tint.a)},
    };
}

void SpriteQueue::clear() noexcept {
    keys_.clear();
    instances_.clear();
    textures_.clear();
    reordered_ = false;
}

void SpriteQueue::push(const Texture& texture, int layer, float depth,
                       const SpriteInstance& instance) {
    keys_.push_back(sort_key(layer, depth, static_cast<std::uint16_t>(texture.id())));
    instances_.push_back(instance);
    textures_.push_back(&texture);
}

std::vector<DrawRun> SpriteQueue::sort() {
    reordered_ = radix_sort(keys_, order_);

    std::vector<DrawRun> runs;
    for (std::uint32_t i = 0; i < instances_.size(); ++i) {
        const Texture* texture = textures_[reordered_ ? order_[i] : i];
        if (runs.empty() || runs.back().texture != texture) {
            runs.push_back(DrawRun{.texture = texture, .first = i, .count = 0});
        }
        ++runs.back().count;
    }
    return runs;
}

void SpriteQueue::write_sorted(std::span<SpriteInstance> out) const {
    if (!reordered_) {
        std::memcpy(out.data(), instances_.data(), instances_.size() * sizeof(SpriteInstance));
        return;
    }
    for (std::size_t i = 0; i < instances_.size(); ++i) {
        out[i] = instances_[order_[i]];
    }
}

} // namespace phantasm
