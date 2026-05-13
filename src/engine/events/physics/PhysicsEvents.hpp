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

} // namespace biofuel::engine::events::physics
