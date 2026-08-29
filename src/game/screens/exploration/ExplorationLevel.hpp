#pragma once

#include "engine/core/Types.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include <raylib.h>
#include <vector>

namespace biofuel::game::screens {

// -----------------------------------------------------------------------------
// ExplorationLevel - static collider geometry for the real player's physics
// world (PhysicsWorld3D). Visuals for this same layout are Bevy/native-Rapier
// -owned and drawn via GameWorldService, but that world is a second, separate
// Rapier instance (see GameWorldService's own doc comment) -- the real player,
// driven entirely by CharacterController3D against PhysicsWorld3D, needs its
// own colliders here to actually stand on. Geometry must stay in sync with
// Engine/game/src/level.rs's level_boxes().
// -----------------------------------------------------------------------------
class ExplorationLevel {
public:
    void spawnColliders(biofuel::engine::physics::PhysicsWorld3D& world);
    void despawn(biofuel::engine::physics::PhysicsWorld3D& world) noexcept;

    [[nodiscard]] Vector3 playerSpawn() const noexcept { return m_playerSpawn; }

private:
    std::vector<biofuel::engine::physics::PhysicsBody3D> m_bodies;
    Vector3 m_playerSpawn{0.0f, 1.0f, -8.0f};
};

} // namespace biofuel::game::screens
