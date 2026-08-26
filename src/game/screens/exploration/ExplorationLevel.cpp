#include "ExplorationLevel.hpp"

namespace biofuel::game::screens {

namespace {

// Flat-ground block-out for milestone 1 (see engine/character/README.md and
// CharacterController3D::Config -- autostep is off by default, so ramps/
// steps are deliberately left out here until that gets validated and turned
// on; every surface below is at the same y=0 ground level). Barn positioned
// on the north side of a ~26x26m fenced yard with a doorway gap the player
// walks through into a small interior room.
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

    m_boxes.clear();
    m_bodies.clear();

    const auto addBox = [&](const Vector3 center, const Vector3 halfExtents, const Color color) {
        const auto body = world.createBody({.kind = PhysicsBodyKind::Fixed, .position = center});
        (void)world.attachCuboid(body, {.halfExtents = halfExtents});
        m_bodies.push_back(body);
        m_boxes.push_back(ExplorationLevelBox{.center = center, .halfExtents = halfExtents, .color = color});
    };

    // Ground (top surface at y=0).
    addBox(Vector3{0.0f, -0.1f, 0.0f}, Vector3{kGroundHalfSize, 0.1f, kGroundHalfSize}, Color{110, 100, 80, 255});

    // Perimeter boundary (plain walls for now -- visual fence detail is a
    // later art pass, this is purely to bound the walkable space).
    const f32 b = kGroundHalfSize;
    addBox(Vector3{0.0f, kBoundaryHalfHeight, -b}, Vector3{b, kBoundaryHalfHeight, kBoundaryThickness}, BEIGE);
    addBox(Vector3{0.0f, kBoundaryHalfHeight, b}, Vector3{b, kBoundaryHalfHeight, kBoundaryThickness}, BEIGE);
    addBox(Vector3{-b, kBoundaryHalfHeight, 0.0f}, Vector3{kBoundaryThickness, kBoundaryHalfHeight, b}, BEIGE);
    addBox(Vector3{b, kBoundaryHalfHeight, 0.0f}, Vector3{kBoundaryThickness, kBoundaryHalfHeight, b}, BEIGE);

    // Barn shell: back + two side walls solid; front wall split around a
    // doorway gap so the player can walk through into the small interior.
    const Color barnColor{150, 90, 60, 255};
    const f32 backZ = kBarnCenterZ + kBarnHalfDepth;
    const f32 frontZ = kBarnCenterZ - kBarnHalfDepth;
    addBox(Vector3{0.0f, kBarnWallHeight, backZ}, Vector3{kBarnHalfWidth, kBarnWallHeight, kBarnWallThickness}, barnColor);
    addBox(Vector3{-kBarnHalfWidth, kBarnWallHeight, kBarnCenterZ}, Vector3{kBarnWallThickness, kBarnWallHeight, kBarnHalfDepth}, barnColor);
    addBox(Vector3{kBarnHalfWidth, kBarnWallHeight, kBarnCenterZ}, Vector3{kBarnWallThickness, kBarnWallHeight, kBarnHalfDepth}, barnColor);
    // Front wall, left and right of the doorway gap.
    const f32 frontSegmentHalfWidth = (kBarnHalfWidth - kDoorHalfWidth) * 0.5f;
    const f32 frontSegmentOffset = kDoorHalfWidth + frontSegmentHalfWidth;
    addBox(Vector3{-frontSegmentOffset, kBarnWallHeight, frontZ}, Vector3{frontSegmentHalfWidth, kBarnWallHeight, kBarnWallThickness}, barnColor);
    addBox(Vector3{frontSegmentOffset, kBarnWallHeight, frontZ}, Vector3{frontSegmentHalfWidth, kBarnWallHeight, kBarnWallThickness}, barnColor);
    // Flat roof cap.
    addBox(Vector3{0.0f, kBarnWallHeight * 2.0f + 0.1f, kBarnCenterZ}, Vector3{kBarnHalfWidth + 0.2f, 0.1f, kBarnHalfDepth + 0.2f}, DARKBROWN);

    // Scattered obstacles for scale reference and movement variety.
    addBox(Vector3{-3.0f, 0.35f, -2.0f}, Vector3{0.35f, 0.35f, 0.35f}, Color{170, 140, 90, 255});   // crate
    addBox(Vector3{-2.0f, 0.35f, -2.6f}, Vector3{0.35f, 0.35f, 0.35f}, Color{170, 140, 90, 255});   // crate
    addBox(Vector3{3.2f, 0.45f, -3.0f}, Vector3{0.3f, 0.45f, 0.3f}, Color{90, 90, 95, 255});        // fuel drum (stand-in box)
    addBox(Vector3{3.9f, 0.45f, -3.4f}, Vector3{0.3f, 0.45f, 0.3f}, Color{90, 90, 95, 255});        // fuel drum (stand-in box)
    addBox(Vector3{-4.5f, 0.4f, 3.0f}, Vector3{0.6f, 0.4f, 0.4f}, Color{190, 170, 110, 255});       // hay bale (stand-in box)

    // Landmark placeholder -- swap for the Meshy-generated fuel silo once
    // it exists (assets/models/, see the environment-scoping plan). Kept
    // tall and visible from anywhere in the yard as a scale/orientation
    // reference in the meantime.
    addBox(Vector3{9.0f, 2.5f, -9.0f}, Vector3{1.0f, 2.5f, 1.0f}, Color{140, 60, 50, 255});

    m_playerSpawn = Vector3{0.0f, 1.0f, -8.0f};
}

void ExplorationLevel::despawn(biofuel::engine::physics::PhysicsWorld3D& world) noexcept {
    for (const auto body : m_bodies) {
        world.removeBody(body);
    }
    m_bodies.clear();
    m_boxes.clear();
}

void ExplorationLevel::draw() const {
    for (const auto& box : m_boxes) {
        const Vector3 size{box.halfExtents.x * 2.0f, box.halfExtents.y * 2.0f, box.halfExtents.z * 2.0f};
        DrawCube(box.center, size.x, size.y, size.z, box.color);
        DrawCubeWires(box.center, size.x, size.y, size.z, Color{0, 0, 0, 60});
    }
}

} // namespace biofuel::game::screens
