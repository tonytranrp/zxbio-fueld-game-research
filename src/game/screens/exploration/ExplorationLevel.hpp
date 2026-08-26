#pragma once

#include "engine/core/Types.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include <raylib.h>
#include <span>
#include <vector>

namespace biofuel::game::screens {

// A single static box of level geometry: one fixed Rapier cuboid collider
// plus a matching drawn cube. Procedural (not a .glb) for milestone 1 --
// the Rust bridge has no trimesh/heightfield collider yet, so level
// collision has to be cuboid/ball/capsule either way (see
// engine/physics/README.md); starting procedural unblocks movement work
// with zero art dependency, and swapping draw() for .glb instances later
// leaves this collider table untouched.
struct ExplorationLevelBox {
    Vector3 center{0.0f, 0.0f, 0.0f};
    Vector3 halfExtents{1.0f, 1.0f, 1.0f};
    Color color = GRAY;
};

// -----------------------------------------------------------------------------
// ExplorationLevel - the equipment-yard-behind-a-barn block-out (fence,
// barn shell + interior room, loading ramp, side step, crates/drums/hay,
// one landmark placeholder for the eventual Meshy-generated silo).
// -----------------------------------------------------------------------------
class ExplorationLevel {
public:
    void spawnColliders(biofuel::engine::physics::PhysicsWorld3D& world);
    void despawn(biofuel::engine::physics::PhysicsWorld3D& world) noexcept;
    void draw() const;

    [[nodiscard]] std::span<const ExplorationLevelBox> boxes() const noexcept { return m_boxes; }
    [[nodiscard]] Vector3 playerSpawn() const noexcept { return m_playerSpawn; }

private:
    std::vector<ExplorationLevelBox> m_boxes;
    std::vector<biofuel::engine::physics::PhysicsBody3D> m_bodies;
    Vector3 m_playerSpawn{0.0f, 1.0f, -8.0f};
};

} // namespace biofuel::game::screens
