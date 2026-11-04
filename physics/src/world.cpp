#include <phantasm/physics/world.hpp>

#include <box2d/box2d.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

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

BodyHandle body_of(b2ShapeId shape) noexcept {
    return b2Shape_IsValid(shape) ? BodyHandle{b2StoreBodyId(b2Shape_GetBody(shape))} : BodyHandle{};
}

template <typename Shape>
b2ShapeDef make_shape_def(const Shape& shape) noexcept {
    b2ShapeDef def = b2DefaultShapeDef();
    def.density = shape.density;
    def.material.friction = shape.friction;
    def.material.restitution = shape.restitution;
    def.filter.categoryBits = shape.filter.category;
    def.filter.maskBits = shape.filter.mask;
    def.isSensor = shape.sensor;
    // Box2D needs the flag on both the sensor and the visiting shape.
    def.enableSensorEvents = shape.sensor || shape.triggers_sensors;
    return def;
}

b2QueryFilter query_filter(std::uint64_t categories) noexcept {
    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = ~std::uint64_t{0}; // ignore shapes' own masks
    filter.maskBits = categories;
    return filter;
}

bool collect_body(b2ShapeId shape, void* context) {
    static_cast<std::vector<std::uint64_t>*>(context)->push_back(
        b2StoreBodyId(b2Shape_GetBody(shape)));
    return true; // keep going
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
    const b2ShapeDef shape = make_shape_def(box);
    const b2Polygon polygon =
        b2MakeBox(box.half_extents.x / units_per_meter_, box.half_extents.y / units_per_meter_);
    b2CreatePolygonShape(load(body), &shape, &polygon);
}

void World::add_circle(BodyHandle body, const Circle& circle) {
    if (!is_valid(body)) {
        return;
    }
    const b2ShapeDef shape = make_shape_def(circle);
    const b2Circle geometry{b2Vec2{0.0F, 0.0F}, circle.radius / units_per_meter_};
    b2CreateCircleShape(load(body), &shape, &geometry);
}

void World::step(float seconds) {
    if (!(seconds > 0.0F)) {
        return;
    }
    const b2WorldId world = b2LoadWorldId(world_id_);
    b2World_Step(world, seconds, sub_steps_);

    // Box2D keeps only the latest step's events; collect them so none are lost when a
    // frame runs several fixed steps before the game reads them.
    const b2SensorEvents events = b2World_GetSensorEvents(world);
    for (int i = 0; i < events.beginCount; ++i) {
        const b2SensorBeginTouchEvent& event = events.beginEvents[i];
        sensor_events_.push_back(
            {body_of(event.sensorShapeId), body_of(event.visitorShapeId), true});
    }
    for (int i = 0; i < events.endCount; ++i) {
        const b2SensorEndTouchEvent& event = events.endEvents[i];
        sensor_events_.push_back(
            {body_of(event.sensorShapeId), body_of(event.visitorShapeId), false});
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

std::optional<RayHit> World::raycast(Vec2 from, Vec2 to, std::uint64_t categories) const {
    const b2Vec2 translation{(to.x - from.x) / units_per_meter_, (to.y - from.y) / units_per_meter_};
    if (translation.x == 0.0F && translation.y == 0.0F) {
        return std::nullopt;
    }
    const b2Vec2 origin{from.x / units_per_meter_, from.y / units_per_meter_};
    const b2RayResult result = b2World_CastRayClosest(b2LoadWorldId(world_id_), b2ToPos(origin),
                                                      translation, query_filter(categories));
    if (!result.hit) {
        return std::nullopt;
    }
    const b2Vec2 point = b2ToVec2(result.point);
    return RayHit{
        .body = body_of(result.shapeId),
        .point = {point.x * units_per_meter_, point.y * units_per_meter_},
        .normal = {result.normal.x, result.normal.y},
        .fraction = result.fraction,
    };
}

std::vector<BodyHandle> World::query_area(Rect area, std::uint64_t categories) const {
    const b2AABB box{
        b2Vec2{area.x / units_per_meter_, area.y / units_per_meter_},
        b2Vec2{(area.x + area.w) / units_per_meter_, (area.y + area.h) / units_per_meter_},
    };
    std::vector<std::uint64_t> ids;
    b2World_OverlapAABB(b2LoadWorldId(world_id_), b2Pos_zero, box, query_filter(categories),
                        collect_body, &ids);
    std::ranges::sort(ids);
    const auto duplicates = std::ranges::unique(ids);
    ids.erase(duplicates.begin(), duplicates.end());

    std::vector<BodyHandle> bodies;
    bodies.reserve(ids.size());
    for (const std::uint64_t id : ids) {
        bodies.push_back(BodyHandle{id});
    }
    return bodies;
}

std::vector<SensorEvent> World::take_sensor_events() {
    return std::exchange(sensor_events_, {});
}

} // namespace phantasm::physics
