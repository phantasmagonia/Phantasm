#include <phantasm/physics/world.hpp>

#include <box2d/box2d.h>

#include <stdexcept>

namespace phantasm::physics {

namespace {

b2BodyType to_box2d(BodyType type) noexcept {
    switch (type) {
    case BodyType::Static: return b2_staticBody;
    case BodyType::Kinematic: return b2_kinematicBody;
    case BodyType::Dynamic: return b2_dynamicBody;
    }
    return b2_dynamicBody;
}

b2BodyId load(BodyHandle body) noexcept {
    return b2LoadBodyId(body.value);
}

} // namespace

World::World(const WorldConfig& config)
    : units_per_meter_{config.units_per_meter}, sub_steps_{config.sub_steps} {
    if (!(config.units_per_meter > 0.0F) || config.sub_steps < 1) {
        throw std::invalid_argument("WorldConfig needs units_per_meter > 0 and sub_steps >= 1");
    }
    b2WorldDef def = b2DefaultWorldDef();
    def.gravity = b2Vec2{config.gravity.x / units_per_meter_, config.gravity.y / units_per_meter_};
    world_id_ = b2StoreWorldId(b2CreateWorld(&def));
}

World::~World() {
    b2DestroyWorld(b2LoadWorldId(world_id_));
}

BodyHandle World::create_body(const BodyDef& def) {
    b2BodyDef body = b2DefaultBodyDef();
    body.type = to_box2d(def.type);
    body.position = b2ToPos(
        b2Vec2{def.position.x / units_per_meter_, def.position.y / units_per_meter_});
    body.linearVelocity = b2Vec2{def.velocity.x / units_per_meter_, def.velocity.y / units_per_meter_};
    body.linearDamping = def.linear_damping;
    body.motionLocks.angularZ = def.fixed_rotation;
    return BodyHandle{b2StoreBodyId(b2CreateBody(b2LoadWorldId(world_id_), &body))};
}

void World::destroy_body(BodyHandle body) {
    if (is_valid(body)) {
        b2DestroyBody(load(body));
    }
}

void World::add_box(BodyHandle body, const Box& box) {
    if (!is_valid(body)) {
        return;
    }
    b2ShapeDef shape = b2DefaultShapeDef();
    shape.density = box.density;
    shape.material.friction = box.friction;
    shape.material.restitution = box.restitution;
    const b2Polygon polygon =
        b2MakeBox(box.half_extents.x / units_per_meter_, box.half_extents.y / units_per_meter_);
    b2CreatePolygonShape(load(body), &shape, &polygon);
}

void World::add_circle(BodyHandle body, const Circle& circle) {
    if (!is_valid(body)) {
        return;
    }
    b2ShapeDef shape = b2DefaultShapeDef();
    shape.density = circle.density;
    shape.material.friction = circle.friction;
    shape.material.restitution = circle.restitution;
    const b2Circle geometry{b2Vec2{0.0F, 0.0F}, circle.radius / units_per_meter_};
    b2CreateCircleShape(load(body), &shape, &geometry);
}

void World::step(float seconds) {
    if (seconds > 0.0F) {
        b2World_Step(b2LoadWorldId(world_id_), seconds, sub_steps_);
    }
}

bool World::is_valid(BodyHandle body) const {
    return body.value != 0 && b2Body_IsValid(load(body));
}

Vec2 World::position(BodyHandle body) const {
    if (!is_valid(body)) {
        return {};
    }
    const b2Vec2 meters = b2ToVec2(b2Body_GetPosition(load(body)));
    return Vec2{meters.x * units_per_meter_, meters.y * units_per_meter_};
}

Vec2 World::velocity(BodyHandle body) const {
    if (!is_valid(body)) {
        return {};
    }
    const b2Vec2 meters = b2Body_GetLinearVelocity(load(body));
    return Vec2{meters.x * units_per_meter_, meters.y * units_per_meter_};
}

void World::set_velocity(BodyHandle body, Vec2 velocity) {
    if (is_valid(body)) {
        b2Body_SetLinearVelocity(load(body),
                                 b2Vec2{velocity.x / units_per_meter_, velocity.y / units_per_meter_});
    }
}

std::size_t World::body_count() const {
    return static_cast<std::size_t>(b2World_GetCounters(b2LoadWorldId(world_id_)).bodyCount);
}

} // namespace phantasm::physics
