#pragma once

#include "engine/core/Types.hpp"
#include "engine/core/units/EngineUnits.hpp"

namespace biofuel::engine::world {

/// Fired when a tile's physics collider is created, destroyed, or modified.
/// This allows other systems (e.g. rendering, audio) to react to
/// world-physics state changes.
struct TileChangedEvent {
    /// The tile coordinate that changed.
    ::biofuel::engine::core::units::TileCoord coord{};

    /// The new tile type after the change.
    u8 tileType = 0U;

    /// If non-negative, the building ID associated with this tile change.
    i32 buildingId = -1;

    /// True if a physics collider was created for this tile; false if removed.
    bool colliderCreated = false;
};

/// Fired when a building collider is placed, removing underlying tile colliders.
struct BuildingPlacedEvent {
    /// Top-left tile coordinate of the building footprint.
    ::biofuel::engine::core::units::TileCoord origin{};

    /// Building type identifier.
    i32 buildingTypeId = 0;

    /// The assigned building instance ID.
    i32 buildingId = 0;

    /// Building footprint width in tiles.
    i32 widthTiles = 2;

    /// Building footprint height in tiles.
    i32 heightTiles = 2;
};

/// Fired when a building collider is removed and underlying tile colliders
/// are restored.
struct BuildingRemovedEvent {
    i32 buildingId = 0;
};

} // namespace biofuel::engine::world
