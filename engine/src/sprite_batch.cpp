#include <phantasm/sprite_batch.hpp>

#include <phantasm/texture.hpp>

#include <algorithm>
#include <array>
#include <bit>
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
    std::vector<std::uint64_t> input_keys;
    std::vector<std::uint64_t> keys;
    std::vector<std::uint64_t> keys_swap;
    std::vector<std::uint32_t> order_swap;
    std::vector<std::uint32_t> order;
    std::vector<SpriteDraw> sorted;
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

void radix_sort(std::span<const std::uint64_t> keys, std::vector<std::uint32_t>& order) {
    const std::size_t count = keys.size();
    order.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        order[i] = i;
    }
    if (count < 2) {
        return;
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
    }
}

std::vector<DrawRun> sort_and_group(std::vector<SpriteDraw>& sprites) {
    SortScratch& buffers = scratch();
    std::vector<std::uint64_t>& keys = buffers.input_keys;
    keys.resize(sprites.size());
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        const SpriteDraw& sprite = sprites[i];
        const auto texture_id =
            static_cast<std::uint16_t>(sprite.texture != nullptr ? sprite.texture->id() : 0);
        keys[i] = sort_key(sprite.layer, sprite.depth, texture_id);
    }
    radix_sort(keys, buffers.order);

    buffers.sorted.resize(sprites.size());
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        buffers.sorted[i] = sprites[buffers.order[i]];
    }
    sprites.swap(buffers.sorted);

    std::vector<DrawRun> runs;
    for (std::uint32_t i = 0; i < sprites.size(); ++i) {
        if (runs.empty() || runs.back().texture != sprites[i].texture) {
            runs.push_back(DrawRun{.texture = sprites[i].texture, .first = i, .count = 0});
        }
        ++runs.back().count;
    }
    return runs;
}

} // namespace phantasm
