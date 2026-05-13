#pragma once

#include "engine/core/Types.hpp"
#include "engine/physics/PhysicsTypes.hpp"

namespace biofuel::engine::events::physics {

struct PhysicsCollisionStartedEvent {
    ::biofuel::engine::physics::PhysicsWorldKind world =
        ::biofuel::engine::physics::PhysicsWorldKind::World2D;
    u64 colliderA = 0U;
    u64 colliderB = 0U;
};

struct PhysicsCollisionEndedEvent {
    ::biofuel::engine::physics::PhysicsWorldKind world =
        ::biofuel::engine::physics::PhysicsWorldKind::World2D;
    u64 colliderA = 0U;
    u64 colliderB = 0U;
};

struct PhysicsShapeCreatedEvent {
    ::biofuel::engine::physics::PhysicsWorldKind world =
        ::biofuel::engine::physics::PhysicsWorldKind::World2D;
    ::biofuel::engine::physics::PhysicsShapeRole role =
        ::biofuel::engine::physics::PhysicsShapeRole::Unknown;
    u64 body = 0U;
    u64 collider = 0U;
};

struct PhysicsShapeDestroyedEvent {
    ::biofuel::engine::physics::PhysicsWorldKind world =
        ::biofuel::engine::physics::PhysicsWorldKind::World2D;
    ::biofuel::engine::physics::PhysicsShapeRole role =
        ::biofuel::engine::physics::PhysicsShapeRole::Unknown;
    u64 body = 0U;
    u64 collider = 0U;
};

struct PhysicsShapeGrabStartedEvent {
    ::biofuel::engine::physics::PhysicsWorldKind world =
        ::biofuel::engine::physics::PhysicsWorldKind::World2D;
    u64 shapeBody = 0U;
    u64 shapeCollider = 0U;
    u64 grabberBody = 0U;
    Vector3 point{0.0f, 0.0f, 0.0f};
};

struct PhysicsShapeGrabEndedEvent {
    ::biofuel::engine::physics::PhysicsWorldKind world =
        ::biofuel::engine::physics::PhysicsWorldKind::World2D;
    u64 shapeBody = 0U;
    u64 shapeCollider = 0U;
    u64 grabberBody = 0U;
    Vector3 point{0.0f, 0.0f, 0.0f};
    Vector3 releaseVelocity{0.0f, 0.0f, 0.0f};
};

} // namespace biofuel::engine::events::physics
