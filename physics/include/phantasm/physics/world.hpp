#pragma once

#include <phantasm/geometry.hpp>

#include <cstddef>
#include <cstdint>

namespace phantasm::physics {

// All values are in engine world units (e.g. pixels) and seconds; the world converts
// to meters internally, where the physics solver is tuned. For isometric games,
// simulate in ground/grid space and project to the screen when drawing.
struct WorldConfig {
    Vec2 gravity{};               // world units / s^2; y points down. Zero for top-down.
    float units_per_meter = 32.0F;
    int sub_steps = 4;            // solver sub-steps per step: more = stiffer, slower
};

enum class BodyType { Static, Kinematic, Dynamic };

struct BodyDef {
    BodyType type = BodyType::Dynamic;
    Vec2 position{};
    Vec2 velocity{};
    float linear_damping = 0.0F;
    bool fixed_rotation = true; // typical for characters and top-down games
};

struct Box {
    Vec2 half_extents{};
    float density = 1.0F;
    float friction = 0.6F;
    float restitution = 0.0F;
};

struct Circle {
    float radius = 0.0F;
    float density = 1.0F;
    float friction = 0.6F;
    float restitution = 0.0F;
};

// Opaque, copyable reference to a body. Stays safe to use after the body is destroyed:
// is_valid() turns false and other calls ignore it.
struct BodyHandle {
    std::uint64_t value = 0;
    friend bool operator==(BodyHandle, BodyHandle) = default;
};

// Owns one physics world (RAII). Not thread-safe: use from one thread.
class World {
public:
    // Throws std::invalid_argument on non-positive units_per_meter or sub_steps.
    explicit World(const WorldConfig& config);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    [[nodiscard]] BodyHandle create_body(const BodyDef& def);
    void destroy_body(BodyHandle body);
    void add_box(BodyHandle body, const Box& box);
    void add_circle(BodyHandle body, const Circle& circle);

    // Advances the simulation. Use a fixed step (see phantasm::FixedStep).
    void step(float seconds);

    [[nodiscard]] bool is_valid(BodyHandle body) const;
    [[nodiscard]] Vec2 position(BodyHandle body) const;
    [[nodiscard]] Vec2 velocity(BodyHandle body) const;
    void set_velocity(BodyHandle body, Vec2 velocity);
    [[nodiscard]] std::size_t body_count() const;

private:
    float units_per_meter_;
    int sub_steps_;
    std::uint32_t world_id_; // stored b2WorldId; keeps Box2D out of this header
};

} // namespace phantasm::physics
