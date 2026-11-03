#include <phantasm/sprite_batch.hpp>

#include <algorithm>
#include <functional>

namespace phantasm {

namespace {

bool draws_before(const SpriteDraw& a, const SpriteDraw& b) noexcept {
    if (a.layer != b.layer) {
        return a.layer < b.layer;
    }
    if (a.depth != b.depth) {
        return a.depth < b.depth;
    }
    // std::less gives a total order for unrelated pointers; plain < does not.
    return std::less<const Texture*>{}(a.texture, b.texture);
}

} // namespace

std::vector<DrawRun> sort_and_group(std::vector<SpriteDraw>& sprites) {
    std::stable_sort(sprites.begin(), sprites.end(), draws_before);

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
