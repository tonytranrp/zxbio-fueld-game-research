#pragma once

#include "engine/events/physics/PhysicsEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::physics {
BIOFUEL_EVENT_TAG(CollisionStarted, ::biofuel::engine::events::physics::PhysicsCollisionStartedEvent);
BIOFUEL_EVENT_TAG(CollisionEnded, ::biofuel::engine::events::physics::PhysicsCollisionEndedEvent);
BIOFUEL_EVENT_TAG(ShapeCreated, ::biofuel::engine::events::physics::PhysicsShapeCreatedEvent);
BIOFUEL_EVENT_TAG(ShapeDestroyed, ::biofuel::engine::events::physics::PhysicsShapeDestroyedEvent);
BIOFUEL_EVENT_TAG(ShapeGrabStarted, ::biofuel::engine::events::physics::PhysicsShapeGrabStartedEvent);
BIOFUEL_EVENT_TAG(ShapeGrabEnded, ::biofuel::engine::events::physics::PhysicsShapeGrabEndedEvent);
} // namespace biofuel::engine::runtime::typed::physics

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(physics::CollisionStarted, "physics.collision_started");
BIOFUEL_EVENT_SPEC(physics::CollisionEnded, "physics.collision_ended");
BIOFUEL_EVENT_SPEC(physics::ShapeCreated, "physics.shape_created");
BIOFUEL_EVENT_SPEC(physics::ShapeDestroyed, "physics.shape_destroyed");
BIOFUEL_EVENT_SPEC(physics::ShapeGrabStarted, "physics.shape_grab_started");
BIOFUEL_EVENT_SPEC(physics::ShapeGrabEnded, "physics.shape_grab_ended");
BIOFUEL_EVENT_MODULE(PhysicsEventModule, PhysicsEvents,
    physics::CollisionStarted,
    physics::CollisionEnded,
    physics::ShapeCreated,
    physics::ShapeDestroyed,
    physics::ShapeGrabStarted,
    physics::ShapeGrabEnded)
} // namespace biofuel::engine::runtime::typed
