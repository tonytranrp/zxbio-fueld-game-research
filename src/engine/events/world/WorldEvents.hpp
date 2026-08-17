#pragma once

#include "engine/core/Types.hpp"
#include "engine/core/units/EngineUnits.hpp"
#include <type_traits>

// ------------------------------------------------------------------------------
// World Events - Tile/chunk/world lifecycle and terrain modification events.
// Fired by the world subsystem when tiles change, chunks load/unload, worlds
// are created/destroyed/activated, heightmaps are modified, and buildings are
// placed/removed.
// ------------------------------------------------------------------------------

// ------------------------------------------------------------------------------
// Stub world domain types; not yet consulted by any live system.
// ------------------------------------------------------------------------------
namespace biofuel::engine::events::world {

struct ChunkCoord {
    i32 x = 0;
    i32 y = 0;
};

struct WorldID {
    u32 value = 0U;

    [[nodiscard]] constexpr bool operator==(const WorldID&) const noexcept = default;
};

struct WorldConfig {
    // Placeholder — will expand with world dimensions, terrain params, etc.
};

struct TileData {
    u32 typeId = 0U;
    u32 flags  = 0U;

    [[nodiscard]] constexpr bool operator==(const TileData&) const noexcept = default;
};

} // namespace biofuel::engine::events::world

// ------------------------------------------------------------------------------
// World event structs
// ------------------------------------------------------------------------------
namespace biofuel::engine::events::world {

struct TileChangedEvent {
    ::biofuel::engine::core::units::TileCoord coord{};
    TileData                                   oldData{};
    TileData                                   newData{};
};

struct ChunkLoadedEvent {
    ChunkCoord coord{};
};

struct ChunkUnloadedEvent {
    ChunkCoord coord{};
};

struct WorldCreatedEvent {
    WorldID     worldId{};
    WorldConfig config{};
};

struct WorldDestroyedEvent {
    WorldID worldId{};
};

struct WorldActivatedEvent {
    WorldID worldId{};
};

struct HeightmapModifiedEvent {
    i32 xMin = 0;
    i32 yMin = 0;
    i32 xMax = 0;
    i32 yMax = 0;
};

struct BuildingPlacedEvent {
    i32 tileX        = 0;
    i32 tileY        = 0;
    i32 buildingTypeId = 0;
};

struct BuildingRemovedEvent {
    i32 tileX = 0;
    i32 tileY = 0;
};

} // namespace biofuel::engine::events::world

// ------------------------------------------------------------------------------
// Compile-time verification: every world event must be trivially copyable so
// the entt dispatcher can memcpy them without surprises.
// ------------------------------------------------------------------------------
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::TileChangedEvent>,
              "WorldEvents: TileChangedEvent must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::ChunkLoadedEvent>,
              "WorldEvents: ChunkLoadedEvent must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::ChunkUnloadedEvent>,
              "WorldEvents: ChunkUnloadedEvent must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::WorldCreatedEvent>,
              "WorldEvents: WorldCreatedEvent must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::WorldDestroyedEvent>,
              "WorldEvents: WorldDestroyedEvent must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::WorldActivatedEvent>,
              "WorldEvents: WorldActivatedEvent must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::HeightmapModifiedEvent>,
              "WorldEvents: HeightmapModifiedEvent must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::BuildingPlacedEvent>,
              "WorldEvents: BuildingPlacedEvent must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::BuildingRemovedEvent>,
              "WorldEvents: BuildingRemovedEvent must be trivially copyable");

// Verify stub types are also trivially copyable
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::ChunkCoord>,
              "WorldEvents: ChunkCoord must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::WorldID>,
              "WorldEvents: WorldID must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::WorldConfig>,
              "WorldEvents: WorldConfig must be trivially copyable");
static_assert(std::is_trivially_copyable_v<::biofuel::engine::events::world::TileData>,
              "WorldEvents: TileData must be trivially copyable");
