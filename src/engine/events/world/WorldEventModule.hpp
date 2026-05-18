#pragma once

#include "engine/events/world/WorldEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

// ------------------------------------------------------------------------------
// WorldEventModule — Typed event registration for general world events.
//
// Registers world lifecycle, tile/chunk state, heightmap, and building
// placement events from the engine::events::world namespace.
//
// Physics-collider events (engine::world namespace) are registered separately
// in engine/world/WorldPhysicsEventModule.hpp with distinct tag names.
// ------------------------------------------------------------------------------

namespace biofuel::engine::runtime::typed::world {
BIOFUEL_EVENT_TAG(TileChanged, ::biofuel::engine::events::world::TileChangedEvent);
BIOFUEL_EVENT_TAG(ChunkLoaded, ::biofuel::engine::events::world::ChunkLoadedEvent);
BIOFUEL_EVENT_TAG(ChunkUnloaded, ::biofuel::engine::events::world::ChunkUnloadedEvent);
BIOFUEL_EVENT_TAG(WorldCreated, ::biofuel::engine::events::world::WorldCreatedEvent);
BIOFUEL_EVENT_TAG(WorldDestroyed, ::biofuel::engine::events::world::WorldDestroyedEvent);
BIOFUEL_EVENT_TAG(WorldActivated, ::biofuel::engine::events::world::WorldActivatedEvent);
BIOFUEL_EVENT_TAG(HeightmapModified, ::biofuel::engine::events::world::HeightmapModifiedEvent);
BIOFUEL_EVENT_TAG(BuildingPlaced, ::biofuel::engine::events::world::BuildingPlacedEvent);
BIOFUEL_EVENT_TAG(BuildingRemoved, ::biofuel::engine::events::world::BuildingRemovedEvent);
} // namespace biofuel::engine::runtime::typed::world

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(world::TileChanged, "world.tile_changed");
BIOFUEL_EVENT_SPEC(world::ChunkLoaded, "world.chunk_loaded");
BIOFUEL_EVENT_SPEC(world::ChunkUnloaded, "world.chunk_unloaded");
BIOFUEL_EVENT_SPEC(world::WorldCreated, "world.world_created");
BIOFUEL_EVENT_SPEC(world::WorldDestroyed, "world.world_destroyed");
BIOFUEL_EVENT_SPEC(world::WorldActivated, "world.world_activated");
BIOFUEL_EVENT_SPEC(world::HeightmapModified, "world.heightmap_modified");
BIOFUEL_EVENT_SPEC(world::BuildingPlaced, "world.building_placed");
BIOFUEL_EVENT_SPEC(world::BuildingRemoved, "world.building_removed");
BIOFUEL_EVENT_MODULE(WorldEventModule, WorldEvents,
    world::TileChanged,
    world::ChunkLoaded,
    world::ChunkUnloaded,
    world::WorldCreated,
    world::WorldDestroyed,
    world::WorldActivated,
    world::HeightmapModified,
    world::BuildingPlaced,
    world::BuildingRemoved)
} // namespace biofuel::engine::runtime::typed
