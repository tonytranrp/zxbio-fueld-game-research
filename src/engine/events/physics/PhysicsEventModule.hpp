#pragma once

#include "engine/events/physics/PhysicsEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::physics {
BIOFUEL_EVENT_TAG(CollisionStarted, ::biofuel::engine::events::physics::PhysicsCollisionStartedEvent);
BIOFUEL_EVENT_TAG(CollisionEnded, ::biofuel::engine::events::physics::PhysicsCollisionEndedEvent);
} // namespace biofuel::engine::runtime::typed::physics

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(physics::CollisionStarted, "physics.collision_started");
BIOFUEL_EVENT_SPEC(physics::CollisionEnded, "physics.collision_ended");
BIOFUEL_EVENT_MODULE(PhysicsEventModule, PhysicsEvents,
    physics::CollisionStarted,
    physics::CollisionEnded)
} // namespace biofuel::engine::runtime::typed
