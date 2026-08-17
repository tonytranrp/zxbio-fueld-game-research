#include "game/gameplay/WorldPhysicsIntegration.hpp"

#include "engine/physics/PhysicsSystem.hpp"
#include "engine/runtime/typed/Events.hpp"

namespace biofuel::game::gameplay {

using namespace ::biofuel::engine::physics;
namespace rt = ::biofuel::engine::runtime::typed;

// =============================================================================
// Coordinate helpers
// =============================================================================

Vector2 WorldPhysicsIntegration::tileCenter(const TileCoord& coord) const noexcept {
    return Vector2{
        (static_cast<f32>(coord.x) + 0.5f) * m_tileSizeMeters,
        (static_cast<f32>(coord.y) + 0.5f) * m_tileSizeMeters,
    };
}

// =============================================================================
// Body creation
// =============================================================================

PhysicsBody2D WorldPhysicsIntegration::createTileBody(
    const TileCoord& coord,
    PhysicsWorld2D& world) const
{
    const Vector2 center = tileCenter(coord);
    const f32 halfExtent = m_tileSizeMeters * 0.5f;

    PhysicsBodyDesc2D desc{};
    desc.kind = PhysicsBodyKind::Fixed;
    desc.position = center;
    desc.canSleep = true;

    const PhysicsBody2D body = world.createBody(desc);

    BoxColliderDesc2D colliderDesc{};
    colliderDesc.halfExtents = Vector2{halfExtent, halfExtent};
    colliderDesc.density = 0.0f;
    colliderDesc.sensor = false;
    colliderDesc.collisionGroup = CollisionGroup::all();

    [[maybe_unused]] const PhysicsCollider2D collider = world.attachBox(body, colliderDesc);

    return body;
}

// =============================================================================
// Tile collider baking
// =============================================================================

void WorldPhysicsIntegration::bakeTileColliders(
    FarmState& farm,
    PhysicsWorld2D& world)
{
    clearAllColliders(world);

    const usize width = farm.width();
    const usize height = farm.height();

    for (usize y = 0U; y < height; ++y) {
        for (usize x = 0U; x < width; ++x) {
            const Tile* tile = farm.tileAt(x, y);
            if (!tile) {
                continue;
            }

            if (!tileHasPhysicsCollider(tile->type)) {
                continue;
            }

            const TileCoord coord{static_cast<i32>(x), static_cast<i32>(y)};
            const PhysicsBody2D body = createTileBody(coord, world);
            if (body) {
                m_tileBodies[coord] = body;
                publishTileChanged(coord,
                                   static_cast<u8>(tile->type),
                                   tile->buildingId,
                                   /*colliderCreated=*/true);
            }
        }
    }
}

// =============================================================================
// Single-tile removal
// =============================================================================

void WorldPhysicsIntegration::removeTrackedTileBody(
    const TileCoord& coord,
    PhysicsWorld2D& world)
{
    const auto it = m_tileBodies.find(coord);
    if (it == m_tileBodies.end()) {
        return;
    }

    const PhysicsBody2D body = it->second;
    if (body) {
        world.removeBody(body);
    }
    m_tileBodies.erase(it);

    publishTileChanged(coord,
                       /*tileType=*/0U,
                       /*buildingId=*/-1,
                       /*colliderCreated=*/false);
}

void WorldPhysicsIntegration::removeTileCollider(
    const TileCoord& coord,
    PhysicsWorld2D& world)
{
    removeTrackedTileBody(coord, world);
}

// =============================================================================
// Building placement
// =============================================================================

void WorldPhysicsIntegration::placeBuildingCollider(
    const TileCoord& origin,
    const i32 buildingTypeId,
    const i32 buildingId,
    PhysicsWorld2D& world)
{
    for (i32 dy = 0; dy < m_buildingHeight; ++dy) {
        for (i32 dx = 0; dx < m_buildingWidth; ++dx) {
            const TileCoord coord{origin.x + dx, origin.y + dy};
            removeTrackedTileBody(coord, world);
        }
    }

    {
        const auto existing = m_buildingBodies.find(buildingId);
        if (existing != m_buildingBodies.end()) {
            if (existing->second) {
                world.removeBody(existing->second);
            }
            m_buildingBodies.erase(existing);
        }
    }

    const f32 centerX = (static_cast<f32>(origin.x) + static_cast<f32>(m_buildingWidth) * 0.5f) * m_tileSizeMeters;
    const f32 centerY = (static_cast<f32>(origin.y) + static_cast<f32>(m_buildingHeight) * 0.5f) * m_tileSizeMeters;

    const f32 halfWidth = static_cast<f32>(m_buildingWidth) * m_tileSizeMeters * 0.5f;
    const f32 halfHeight = static_cast<f32>(m_buildingHeight) * m_tileSizeMeters * 0.5f;

    PhysicsBodyDesc2D desc{};
    desc.kind = PhysicsBodyKind::Fixed;
    desc.position = Vector2{centerX, centerY};
    desc.canSleep = true;

    const PhysicsBody2D body = world.createBody(desc);

    BoxColliderDesc2D colliderDesc{};
    colliderDesc.halfExtents = Vector2{halfWidth, halfHeight};
    colliderDesc.density = 0.0f;
    colliderDesc.sensor = false;
    colliderDesc.collisionGroup = CollisionGroup::all();

    [[maybe_unused]] const PhysicsCollider2D collider = world.attachBox(body, colliderDesc);

    m_buildingBodies[buildingId] = body;

    {
        ::biofuel::engine::world::BuildingPlacedEvent event{};
        event.origin = origin;
        event.buildingTypeId = buildingTypeId;
        event.buildingId = buildingId;
        event.widthTiles = m_buildingWidth;
        event.heightTiles = m_buildingHeight;
        rt::Events::publish<rt::world::WorldPhysicsBuildingPlaced>(std::move(event));
    }
}

// =============================================================================
// Building removal
// =============================================================================

void WorldPhysicsIntegration::removeBuildingCollider(
    const i32 buildingId,
    PhysicsWorld2D& world)
{
    const auto it = m_buildingBodies.find(buildingId);
    if (it == m_buildingBodies.end()) {
        return;
    }

    if (it->second) {
        world.removeBody(it->second);
    }
    m_buildingBodies.erase(it);

    {
        ::biofuel::engine::world::BuildingRemovedEvent event{};
        event.buildingId = buildingId;
        rt::Events::publish<rt::world::WorldPhysicsBuildingRemoved>(std::move(event));
    }
}

// =============================================================================
// Clear all
// =============================================================================

void WorldPhysicsIntegration::clearAllColliders(
    PhysicsWorld2D& world)
{
    for (const auto& [coord, body] : m_tileBodies) {
        if (body) {
            world.removeBody(body);
        }
        publishTileChanged(coord,
                           /*tileType=*/0U,
                           /*buildingId=*/-1,
                           /*colliderCreated=*/false);
    }
    m_tileBodies.clear();

    for (const auto& [buildingId, body] : m_buildingBodies) {
        if (body) {
            world.removeBody(body);
        }
        {
            ::biofuel::engine::world::BuildingRemovedEvent event{};
            event.buildingId = buildingId;
            rt::Events::publish<rt::world::WorldPhysicsBuildingRemoved>(std::move(event));
        }
    }
    m_buildingBodies.clear();
}

// =============================================================================
// Accessors
// =============================================================================

PhysicsBody2D WorldPhysicsIntegration::tileBodyAt(const TileCoord& coord) const noexcept {
    const auto it = m_tileBodies.find(coord);
    if (it == m_tileBodies.end()) {
        return PhysicsBody2D{};
    }
    return it->second;
}

PhysicsBody2D WorldPhysicsIntegration::buildingBody(const i32 buildingId) const noexcept {
    const auto it = m_buildingBodies.find(buildingId);
    if (it == m_buildingBodies.end()) {
        return PhysicsBody2D{};
    }
    return it->second;
}

// =============================================================================
// Event publishing
// =============================================================================

void WorldPhysicsIntegration::publishTileChanged(
    const TileCoord& coord,
    const u8 tileType,
    const i32 buildingId,
    const bool colliderCreated) noexcept
{
    ::biofuel::engine::world::TileChangedEvent event{};
    event.coord = coord;
    event.tileType = tileType;
    event.buildingId = buildingId;
    event.colliderCreated = colliderCreated;
    rt::Events::publish<rt::world::WorldPhysicsTileChanged>(std::move(event));
}

} // namespace biofuel::game::gameplay
