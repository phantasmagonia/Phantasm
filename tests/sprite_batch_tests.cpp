#include <phantasm/sprite_batch.hpp>
#include <phantasm/texture.hpp>
#include <phantasm/sprite.hpp>
#include <phantasm/task_pool.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <cstring>

using phantasm::DrawRun;
using phantasm::SpriteInstance;
using phantasm::SpriteQueue;
using phantasm::Texture;

namespace {

// Tags each instance with a marker in rect.x so tests can see the final order.
void push(SpriteQueue& queue, const Texture& texture, int layer, float depth, float marker) {
    SpriteInstance instance{};
    instance.rect.x = marker;
    queue.push(texture, layer, depth, instance);
}

std::vector<float> sorted_markers(SpriteQueue& queue) {
    std::vector<SpriteInstance> out(queue.size());
    queue.write_sorted(out);
    std::vector<float> markers;
    for (const SpriteInstance& instance : out) {
        markers.push_back(instance.rect.x);
    }
    return markers;
}

} // namespace

TEST_CASE("Sprites are ordered by layer, then depth") {
    const Texture tex{nullptr, nullptr, 1, 1};
    SpriteQueue queue;
    push(queue, tex, 1, 0.0F, 3);
    push(queue, tex, 0, 5.0F, 2);
    push(queue, tex, 0, 1.0F, 1);

    (void)queue.sort();

    CHECK(sorted_markers(queue) == std::vector<float>{1, 2, 3});
}

TEST_CASE("Equal keys keep submission order") {
    const Texture tex{nullptr, nullptr, 1, 1};
    SpriteQueue queue;
    for (int i = 0; i < 5; ++i) {
        push(queue, tex, 0, 0.0F, static_cast<float>(i));
    }

    const auto runs = queue.sort();

    CHECK(runs.size() == 1);
    CHECK(sorted_markers(queue) == std::vector<float>{0, 1, 2, 3, 4});
}

TEST_CASE("Consecutive sprites with the same texture share one draw run") {
    const Texture a{nullptr, nullptr, 1, 1};
    SpriteQueue queue;
    push(queue, a, 0, 0, 0);
    push(queue, a, 0, 1, 1);
    push(queue, a, 0, 2, 2);

    const std::vector<DrawRun> runs = queue.sort();

    REQUIRE(runs.size() == 1);
    CHECK(runs[0].texture == &a);
    CHECK(runs[0].first == 0);
    CHECK(runs[0].count == 3);
}

TEST_CASE("Equal sort keys are grouped by texture to minimize draw calls") {
    const Texture a{nullptr, nullptr, 1, 1};
    const Texture b{nullptr, nullptr, 1, 1};
    SpriteQueue queue;
    push(queue, a, 0, 0, 0);
    push(queue, b, 0, 0, 1);
    push(queue, a, 0, 0, 2);
    push(queue, b, 0, 0, 3);

    CHECK(queue.sort().size() == 2);
}

TEST_CASE("Depth order wins over texture batching") {
    const Texture a{nullptr, nullptr, 1, 1};
    const Texture b{nullptr, nullptr, 1, 1};
    SpriteQueue queue;
    push(queue, a, 0, 0, 0);
    push(queue, b, 0, 1, 1);
    push(queue, a, 0, 2, 2);

    const auto runs = queue.sort();

    REQUIRE(runs.size() == 3);
    CHECK(runs[1].texture == &b);
    CHECK(runs[2].first == 2);
}

TEST_CASE("No sprites means no draw runs") {
    SpriteQueue queue;
    CHECK(queue.sort().empty());
    CHECK(queue.size() == 0);
}

TEST_CASE("Clearing empties the queue for the next frame") {
    const Texture tex{nullptr, nullptr, 1, 1};
    SpriteQueue queue;
    push(queue, tex, 0, 3, 7);
    (void)queue.sort();

    queue.clear();
    push(queue, tex, 0, 1, 9);
    (void)queue.sort();

    CHECK(sorted_markers(queue) == std::vector<float>{9});
}

TEST_CASE("Packed sprite instances take 32 bytes") {
    STATIC_REQUIRE(sizeof(SpriteInstance) == 32);
}

TEST_CASE("Packing keeps the rect exact and quantizes uv and tint") {
    const auto instance = phantasm::pack_instance(phantasm::Rect{1.5F, -2.25F, 64.0F, 32.0F},
                                                  phantasm::Rect{0.0F, 0.5F, 1.0F, 0.25F},
                                                  phantasm::Color{1.0F, 0.5F, 0.0F, 1.0F});

    CHECK(instance.rect.x == 1.5F);
    CHECK(instance.rect.h == 32.0F);
    CHECK(instance.uv[0] == 0);
    CHECK(instance.uv[1] == 32768);
    CHECK(instance.uv[2] == 65535);
    CHECK(instance.uv[3] == 16384);
    CHECK(instance.tint[0] == 255);
    CHECK(instance.tint[1] == 128);
    CHECK(instance.tint[2] == 0);
    CHECK(instance.tint[3] == 255);
}

TEST_CASE("Out-of-range uv and tint values are clamped") {
    const auto instance = phantasm::pack_instance(phantasm::Rect{},
                                                  phantasm::Rect{-0.5F, 2.0F, 0.0F, 0.0F},
                                                  phantasm::Color{-1.0F, 3.0F, 0.0F, 0.0F});

    CHECK(instance.uv[0] == 0);
    CHECK(instance.uv[1] == 65535);
    CHECK(instance.tint[0] == 0);
    CHECK(instance.tint[1] == 255);
}

TEST_CASE("Bulk preparation and parallel gathering match scalar sprite submission") {
    const Texture texture{nullptr, nullptr, 1, 1};
    phantasm::TaskPool pool{3};
    std::vector<phantasm::Sprite> sprites(150'003);
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        sprites[i] = {.position = {static_cast<float>(i % 200), static_cast<float>(i % 100)},
                      .size = {8, 12}, .tint = {1, 0.5F, 0.25F, 0.75F},
                      .pivot = {0.5F, 1}, .layer = static_cast<int>(i % 3),
                      .depth = static_cast<float>(i % 7)};
    }
    const phantasm::Rect view{0, 0, 100, 100};
    SpriteQueue scalar;
    SpriteQueue bulk;
    push(scalar, texture, -1, 0, -1);
    push(bulk, texture, -1, 0, -1);
    std::size_t visible = 0;
    for (const auto& sprite : sprites) {
        const auto rect = phantasm::sprite_world_rect(sprite);
        if (phantasm::intersects(rect, view)) {
            scalar.push(texture, sprite.layer, sprite.depth,
                        phantasm::pack_instance(rect, sprite.uv, sprite.tint));
            ++visible;
        }
    }
    CHECK(bulk.append(texture, sprites, view, &pool) == visible);
    const auto scalar_runs = scalar.sort();
    const auto bulk_runs = bulk.sort();
    REQUIRE(scalar_runs.size() == bulk_runs.size());
    for (std::size_t i = 0; i < scalar_runs.size(); ++i) {
        CHECK(scalar_runs[i].first == bulk_runs[i].first);
        CHECK(scalar_runs[i].count == bulk_runs[i].count);
        CHECK(scalar_runs[i].texture == bulk_runs[i].texture);
    }
    std::vector<SpriteInstance> expected(scalar.size());
    std::vector<SpriteInstance> actual(bulk.size());
    scalar.write_sorted(expected);
    bulk.write_sorted(actual, &pool);
    REQUIRE(expected.size() == actual.size());
    CHECK(std::memcmp(expected.data(), actual.data(), actual.size() * sizeof(SpriteInstance)) == 0);

    bulk.clear();
    CHECK(bulk.append(texture, sprites, {1000, 1000, 1, 1}, &pool) == 0);
    CHECK(bulk.sort().empty());
    CHECK(bulk.append(texture, {}, view, &pool) == 0);
}
