#pragma once

#include "engine/runtime/typed/EventDeclare.hpp"
#include "engine/world/WorldEvents.hpp"

// ---------------------------------------------------------------------------
// WorldPhysicsEventModule — physics-specific world event registrations.
//
// Uses distinct tag names (WorldPhysicsTileChanged, WorldPhysicsBuildingPlaced,
// WorldPhysicsBuildingRemoved) to avoid collision with the general-purpose
// world event tags registered in engine/events/world/WorldEventModule.hpp.
// ---------------------------------------------------------------------------

namespace biofuel::engine::runtime::typed::world {
BIOFUEL_EVENT_TAG(WorldPhysicsTileChanged, ::biofuel::engine::world::TileChangedEvent);
BIOFUEL_EVENT_TAG(WorldPhysicsBuildingPlaced, ::biofuel::engine::world::BuildingPlacedEvent);
BIOFUEL_EVENT_TAG(WorldPhysicsBuildingRemoved, ::biofuel::engine::world::BuildingRemovedEvent);
} // namespace biofuel::engine::runtime::typed::world

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(world::WorldPhysicsTileChanged, "world.physics_tile_changed");
BIOFUEL_EVENT_SPEC(world::WorldPhysicsBuildingPlaced, "world.physics_building_placed");
BIOFUEL_EVENT_SPEC(world::WorldPhysicsBuildingRemoved, "world.physics_building_removed");
BIOFUEL_EVENT_MODULE(WorldPhysicsEventModule, WorldPhysicsEvents,
    world::WorldPhysicsTileChanged,
    world::WorldPhysicsBuildingPlaced,
    world::WorldPhysicsBuildingRemoved)
} // namespace biofuel::engine::runtime::typed
