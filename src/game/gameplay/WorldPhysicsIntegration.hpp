#pragma once

#include "engine/core/Types.hpp"
#include "engine/physics/PhysicsTypes.hpp"
#include "engine/core/units/EngineUnits.hpp"
#include "game/gameplay/FarmState.hpp"
#include <unordered_map>

namespace biofuel::engine::physics {
class PhysicsWorld2D;
} // namespace biofuel::engine::physics

namespace biofuel::game::gameplay {

using TileCoord = ::biofuel::engine::core::units::TileCoord;

// ---------------------------------------------------------------------------
// TileCoordHash — enables std::unordered_map<TileCoord, T> lookups
// ---------------------------------------------------------------------------
struct TileCoordHash {
    [[nodiscard]] usize operator()(const TileCoord& coord) const noexcept {
        const u64 a = static_cast<u64>(static_cast<u32>(coord.x));
        const u64 b = static_cast<u64>(static_cast<u32>(coord.y));
        return static_cast<usize>(((a + b) * (a + b + 1U)) / 2U + b);
    }
};

// ---------------------------------------------------------------------------
// TileCoordEqual — required by unordered_map for key comparison
// ---------------------------------------------------------------------------
struct TileCoordEqual {
    [[nodiscard]] bool operator()(const TileCoord& lhs, const TileCoord& rhs) const noexcept {
        return lhs.x == rhs.x && lhs.y == rhs.y;
    }
};

// ---------------------------------------------------------------------------
// WorldPhysicsIntegration — bridges tile-grid data into the 2D physics world.
//
// Lives in the game layer because it depends on FarmState (game type).
// ---------------------------------------------------------------------------
class WorldPhysicsIntegration {
public:
    WorldPhysicsIntegration() = default;
    ~WorldPhysicsIntegration() noexcept = default;

    WorldPhysicsIntegration(const WorldPhysicsIntegration&) = delete;
    WorldPhysicsIntegration& operator=(const WorldPhysicsIntegration&) = delete;
    WorldPhysicsIntegration(WorldPhysicsIntegration&&) = delete;
    WorldPhysicsIntegration& operator=(WorldPhysicsIntegration&&) = delete;

    void setTileSizeMeters(f32 size) noexcept { m_tileSizeMeters = size > 0.0f ? size : 1.0f; }
    [[nodiscard]] f32 tileSizeMeters() const noexcept { return m_tileSizeMeters; }

    void setBuildingFootprint(i32 width, i32 height) noexcept {
        m_buildingWidth = width > 0 ? width : 2;
        m_buildingHeight = height > 0 ? height : 2;
    }
    [[nodiscard]] i32 buildingWidth() const noexcept { return m_buildingWidth; }
    [[nodiscard]] i32 buildingHeight() const noexcept { return m_buildingHeight; }

    void bakeTileColliders(
        FarmState& farm,
        ::biofuel::engine::physics::PhysicsWorld2D& world);

    void removeTileCollider(
        const TileCoord& coord,
        ::biofuel::engine::physics::PhysicsWorld2D& world);

    void placeBuildingCollider(
        const TileCoord& origin,
        i32 buildingTypeId,
        i32 buildingId,
        ::biofuel::engine::physics::PhysicsWorld2D& world);

    void removeBuildingCollider(
        i32 buildingId,
        ::biofuel::engine::physics::PhysicsWorld2D& world);

    void clearAllColliders(
        ::biofuel::engine::physics::PhysicsWorld2D& world);

    [[nodiscard]] const std::unordered_map<TileCoord, ::biofuel::engine::physics::PhysicsBody2D, TileCoordHash, TileCoordEqual>&
    tileBodies() const noexcept { return m_tileBodies; }

    [[nodiscard]] const std::unordered_map<i32, ::biofuel::engine::physics::PhysicsBody2D>&
    buildingBodies() const noexcept { return m_buildingBodies; }

    [[nodiscard]] ::biofuel::engine::physics::PhysicsBody2D tileBodyAt(const TileCoord& coord) const noexcept;
    [[nodiscard]] ::biofuel::engine::physics::PhysicsBody2D buildingBody(i32 buildingId) const noexcept;

private:
    [[nodiscard]] Vector2 tileCenter(const TileCoord& coord) const noexcept;

    [[nodiscard]] ::biofuel::engine::physics::PhysicsBody2D createTileBody(
        const TileCoord& coord,
        ::biofuel::engine::physics::PhysicsWorld2D& world) const;

    static void publishTileChanged(
        const TileCoord& coord,
        u8 tileType,
        i32 buildingId,
        bool colliderCreated) noexcept;

    void removeTrackedTileBody(
        const TileCoord& coord,
        ::biofuel::engine::physics::PhysicsWorld2D& world);

    f32 m_tileSizeMeters = 1.0f;
    i32 m_buildingWidth = 2;
    i32 m_buildingHeight = 2;

    std::unordered_map<TileCoord, ::biofuel::engine::physics::PhysicsBody2D, TileCoordHash, TileCoordEqual> m_tileBodies;
    std::unordered_map<i32, ::biofuel::engine::physics::PhysicsBody2D> m_buildingBodies;
};

} // namespace biofuel::game::gameplay
