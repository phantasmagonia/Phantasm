#include <phantasm/sprite_batch.hpp>
#include <phantasm/texture.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using phantasm::DrawRun;
using phantasm::SpriteDraw;
using phantasm::Texture;

namespace {
SpriteDraw draw(const Texture& texture, int layer, float depth, float marker = 0.0F) {
    SpriteDraw sprite;
    sprite.texture = &texture;
    sprite.layer = layer;
    sprite.depth = depth;
    sprite.instance.rect.x = marker;
    return sprite;
}
} // namespace

TEST_CASE("Sprites are ordered by layer, then depth") {
    const Texture tex{nullptr, nullptr, 1, 1};
    std::vector<SpriteDraw> sprites{
        draw(tex, 1, 0.0F, 3),
        draw(tex, 0, 5.0F, 2),
        draw(tex, 0, 1.0F, 1),
    };

    phantasm::sort_and_group(sprites);

    CHECK(sprites[0].instance.rect.x == 1);
    CHECK(sprites[1].instance.rect.x == 2);
    CHECK(sprites[2].instance.rect.x == 3);
}

TEST_CASE("Consecutive sprites with the same texture share one draw run") {
    const Texture a{nullptr, nullptr, 1, 1};
    std::vector<SpriteDraw> sprites{draw(a, 0, 0), draw(a, 0, 1), draw(a, 0, 2)};

    const std::vector<DrawRun> runs = phantasm::sort_and_group(sprites);

    REQUIRE(runs.size() == 1);
    CHECK(runs[0].texture == &a);
    CHECK(runs[0].first == 0);
    CHECK(runs[0].count == 3);
}

TEST_CASE("Equal sort keys are grouped by texture to minimize draw calls") {
    const Texture a{nullptr, nullptr, 1, 1};
    const Texture b{nullptr, nullptr, 1, 1};
    std::vector<SpriteDraw> sprites{draw(a, 0, 0), draw(b, 0, 0), draw(a, 0, 0), draw(b, 0, 0)};

    const auto runs = phantasm::sort_and_group(sprites);

    CHECK(runs.size() == 2);
}

TEST_CASE("Depth order wins over texture batching") {
    const Texture a{nullptr, nullptr, 1, 1};
    const Texture b{nullptr, nullptr, 1, 1};
    std::vector<SpriteDraw> sprites{draw(a, 0, 0), draw(b, 0, 1), draw(a, 0, 2)};

    const auto runs = phantasm::sort_and_group(sprites);

    REQUIRE(runs.size() == 3);
    CHECK(runs[1].texture == &b);
    CHECK(runs[2].first == 2);
}

TEST_CASE("No sprites means no draw runs") {
    std::vector<SpriteDraw> sprites;
    CHECK(phantasm::sort_and_group(sprites).empty());
}
