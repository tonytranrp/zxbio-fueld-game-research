#include "ExplorationLevel.hpp"

namespace biofuel::game::screens {

namespace {

// Mirrors Engine/game/src/level.rs's level_boxes() constants exactly --
// collider-only here (no color/draw), since GameWorldService owns rendering
// for this same layout in its own, separate Rapier world.
constexpr f32 kGroundHalfSize = 14.0f;
constexpr f32 kBoundaryHalfHeight = 1.5f;
constexpr f32 kBoundaryThickness = 0.15f;
constexpr f32 kBarnWallHeight = 1.6f;   // half-height
constexpr f32 kBarnWallThickness = 0.15f;
constexpr f32 kBarnHalfWidth = 4.5f;    // along X
constexpr f32 kBarnHalfDepth = 5.5f;    // along Z
constexpr f32 kBarnCenterZ = 9.0f;
constexpr f32 kDoorHalfWidth = 1.1f;

} // namespace

void ExplorationLevel::spawnColliders(biofuel::engine::physics::PhysicsWorld3D& world) {
    using biofuel::engine::physics::PhysicsBodyKind;

    m_bodies.clear();

    const auto addBox = [&](const Vector3 center, const Vector3 halfExtents) {
        const auto body = world.createBody({.kind = PhysicsBodyKind::Fixed, .position = center});
        (void)world.attachCuboid(body, {.halfExtents = halfExtents});
        m_bodies.push_back(body);
    };

    // Ground (top surface at y=0).
    addBox(Vector3{0.0f, -0.1f, 0.0f}, Vector3{kGroundHalfSize, 0.1f, kGroundHalfSize});

    // Perimeter boundary (bounds the walkable space).
    const f32 b = kGroundHalfSize;
    addBox(Vector3{0.0f, kBoundaryHalfHeight, -b}, Vector3{b, kBoundaryHalfHeight, kBoundaryThickness});
    addBox(Vector3{0.0f, kBoundaryHalfHeight, b}, Vector3{b, kBoundaryHalfHeight, kBoundaryThickness});
    addBox(Vector3{-b, kBoundaryHalfHeight, 0.0f}, Vector3{kBoundaryThickness, kBoundaryHalfHeight, b});
    addBox(Vector3{b, kBoundaryHalfHeight, 0.0f}, Vector3{kBoundaryThickness, kBoundaryHalfHeight, b});

    // Barn shell: back + two side walls solid; front wall split around a
    // doorway gap so the player can walk through into the small interior.
    const f32 backZ = kBarnCenterZ + kBarnHalfDepth;
    const f32 frontZ = kBarnCenterZ - kBarnHalfDepth;
    addBox(Vector3{0.0f, kBarnWallHeight, backZ}, Vector3{kBarnHalfWidth, kBarnWallHeight, kBarnWallThickness});
    addBox(Vector3{-kBarnHalfWidth, kBarnWallHeight, kBarnCenterZ}, Vector3{kBarnWallThickness, kBarnWallHeight, kBarnHalfDepth});
    addBox(Vector3{kBarnHalfWidth, kBarnWallHeight, kBarnCenterZ}, Vector3{kBarnWallThickness, kBarnWallHeight, kBarnHalfDepth});
    // Front wall, left and right of the doorway gap.
    const f32 frontSegmentHalfWidth = (kBarnHalfWidth - kDoorHalfWidth) * 0.5f;
    const f32 frontSegmentOffset = kDoorHalfWidth + frontSegmentHalfWidth;
    addBox(Vector3{-frontSegmentOffset, kBarnWallHeight, frontZ}, Vector3{frontSegmentHalfWidth, kBarnWallHeight, kBarnWallThickness});
    addBox(Vector3{frontSegmentOffset, kBarnWallHeight, frontZ}, Vector3{frontSegmentHalfWidth, kBarnWallHeight, kBarnWallThickness});
    // Flat roof cap.
    addBox(Vector3{0.0f, kBarnWallHeight * 2.0f + 0.1f, kBarnCenterZ}, Vector3{kBarnHalfWidth + 0.2f, 0.1f, kBarnHalfDepth + 0.2f});

    // Scattered obstacles for scale reference and movement variety.
    addBox(Vector3{-3.0f, 0.35f, -2.0f}, Vector3{0.35f, 0.35f, 0.35f});   // crate
    addBox(Vector3{-2.0f, 0.35f, -2.6f}, Vector3{0.35f, 0.35f, 0.35f});   // crate
    addBox(Vector3{3.2f, 0.45f, -3.0f}, Vector3{0.3f, 0.45f, 0.3f});      // fuel drum (stand-in box)
    addBox(Vector3{3.9f, 0.45f, -3.4f}, Vector3{0.3f, 0.45f, 0.3f});      // fuel drum (stand-in box)
    addBox(Vector3{-4.5f, 0.4f, 3.0f}, Vector3{0.6f, 0.4f, 0.4f});        // hay bale (stand-in box)

    // Landmark placeholder -- swap for the Meshy-generated fuel silo once it exists.
    addBox(Vector3{9.0f, 2.5f, -9.0f}, Vector3{1.0f, 2.5f, 1.0f});
}

void ExplorationLevel::despawn(biofuel::engine::physics::PhysicsWorld3D& world) noexcept {
    for (const auto body : m_bodies) {
        world.removeBody(body);
    }
    m_bodies.clear();
}

} // namespace biofuel::game::screens
