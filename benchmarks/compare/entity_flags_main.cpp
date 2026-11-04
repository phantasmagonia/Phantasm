#include <phantasm/entity_store.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

struct Position { float x, y; };
struct Velocity { float x, y; };
struct Visible { float depth, width, height, tint; };
struct Stunned {};

constexpr std::size_t kEntities = 1'000'000;
constexpr std::size_t kToggles = kEntities / 100;
constexpr int kWarmup = 30;
constexpr int kMeasured = 200;

template <typename Update>
void measure(const char* mode, Update&& update) {
    std::vector<double> milliseconds;
    milliseconds.reserve(kMeasured);
    for (int frame = 0; frame < kWarmup + kMeasured; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        update(frame % 2 == 0);
        const auto end = std::chrono::steady_clock::now();
        if (frame >= kWarmup) {
            milliseconds.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }
    }
    std::sort(milliseconds.begin(), milliseconds.end());
    std::printf("%s,%zu,%zu,%d,%.4f\n", mode, kEntities, kToggles, kMeasured,
                milliseconds[milliseconds.size() / 2]);
}

void run_entt() {
    entt::registry registry;
    std::vector<entt::entity> entities;
    entities.reserve(kToggles);
    for (std::size_t i = 0; i < kEntities; ++i) {
        const auto entity = registry.create();
        registry.emplace<Position>(entity, static_cast<float>(i % 1600),
                                   static_cast<float>(i / 1600));
        registry.emplace<Velocity>(entity, 1.0F, 0.5F);
        registry.emplace<Visible>(entity, 1.0F, 8.0F, 12.0F, 1.0F);
        if (i < kToggles) {
            entities.push_back(entity);
        }
    }
    measure("entt", [&](bool enabled) {
        for (const auto entity : entities) {
            if (enabled) {
                registry.emplace<Stunned>(entity);
            } else {
                registry.remove<Stunned>(entity);
            }
        }
    });
    if (registry.all_of<Stunned>(entities.front())) {
        throw std::logic_error("EnTT flag did not toggle off");
    }
}

void run_store(bool sidecar) {
    phantasm::EntityStore store;
    std::vector<phantasm::Entity> entities;
    entities.reserve(kToggles);
    for (std::size_t i = 0; i < kEntities; ++i) {
        const auto entity = store.create(Position{static_cast<float>(i % 1600),
                                                   static_cast<float>(i / 1600)},
                                         Velocity{1.0F, 0.5F}, Visible{1, 8, 12, 1});
        if (i < kToggles) {
            entities.push_back(entity);
        }
    }
    phantasm::EntityFlags flags{store};
    if (sidecar) {
        measure("sidecar", [&](bool enabled) {
            for (const auto entity : entities) {
                flags.set(entity, 1, enabled);
            }
        });
        if (flags.test(entities.front(), 1)) {
            throw std::logic_error("Sidecar flag did not toggle off");
        }
    } else {
        measure("archetype", [&](bool enabled) {
            for (const auto entity : entities) {
                if (enabled) {
                    store.add(entity, Stunned{});
                } else {
                    store.remove<Stunned>(entity);
                }
            }
        });
        if (store.has<Stunned>(entities.front())) {
            throw std::logic_error("Archetype flag did not toggle off");
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        throw std::invalid_argument("Expected entt, archetype, or sidecar");
    }
    const std::string_view mode{argv[1]};
    if (mode == "entt") {
        run_entt();
    } else if (mode == "archetype") {
        run_store(false);
    } else if (mode == "sidecar") {
        run_store(true);
    } else {
        throw std::invalid_argument("Unknown flag benchmark mode");
    }
}
