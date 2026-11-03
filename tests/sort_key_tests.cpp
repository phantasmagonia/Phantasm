#include <phantasm/sprite_batch.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <vector>

using phantasm::radix_sort;
using phantasm::sort_key;

TEST_CASE("Sort keys order by depth, including negative depths") {
    CHECK(sort_key(0, -5.0F, 0) < sort_key(0, -1.0F, 0));
    CHECK(sort_key(0, -1.0F, 0) < sort_key(0, 0.0F, 0));
    CHECK(sort_key(0, 0.0F, 0) < sort_key(0, 0.5F, 0));
    CHECK(sort_key(0, 0.5F, 0) < sort_key(0, 1000.0F, 0));
}

TEST_CASE("Negative zero and positive zero depth are equal") {
    CHECK(sort_key(0, -0.0F, 0) == sort_key(0, 0.0F, 0));
}

TEST_CASE("Layer outranks depth, including negative layers") {
    CHECK(sort_key(-1, 1000.0F, 0) < sort_key(0, -1000.0F, 0));
    CHECK(sort_key(0, 1000.0F, 0) < sort_key(1, -1000.0F, 0));
}

TEST_CASE("Texture id only breaks ties") {
    CHECK(sort_key(0, 1.0F, 9) < sort_key(0, 2.0F, 0));
    CHECK(sort_key(0, 1.0F, 0) < sort_key(0, 1.0F, 9));
}

TEST_CASE("Out-of-range layers clamp instead of wrapping") {
    constexpr int kHuge = std::numeric_limits<int>::max();
    CHECK(sort_key(kHuge, 0.0F, 0) > sort_key(1000, 0.0F, 0));
    CHECK(sort_key(-kHuge, 0.0F, 0) < sort_key(-1000, 0.0F, 0));
}

TEST_CASE("Radix sort orders keys and keeps equal keys in original order") {
    std::vector<std::uint64_t> keys{30, 10, 20, 10, 0xFFFF'FFFF'FFFF'FFFFULL, 0};
    std::vector<std::uint32_t> order;

    radix_sort(keys, order);

    CHECK(order == std::vector<std::uint32_t>{5, 1, 3, 2, 0, 4});
}

TEST_CASE("Radix sort handles empty input") {
    std::vector<std::uint64_t> keys;
    std::vector<std::uint32_t> order{7};

    radix_sort(keys, order);

    CHECK(order.empty());
}
