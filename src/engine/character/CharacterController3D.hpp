#pragma once

#include "engine/core/Types.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include <raylib.h>

namespace biofuel::engine::character {

// Per-frame input for CharacterController3D::step(). moveAxis is local to the
// look direction (x = strafe right, y = forward), typically from WASD, and is
// NOT required to be normalized -- step() clamps its length itself so
// diagonal movement isn't faster than cardinal movement.
struct CharacterMoveInput {
    Vector2 moveAxis{0.0f, 0.0f};
    f32 yawRadians = 0.0f;
    bool sprint = false;
    bool jump = false;
};

// -----------------------------------------------------------------------------
// CharacterController3D - a kinematic capsule driven by Rapier's
// KinematicCharacterController (PhysicsWorld3D::moveCharacter), with
// Source-engine-style accelerate/friction ground movement on top (research
// baseline: reach top speed in ~0.1s via an accelerate-toward-wishspeed model,
// then a two-regime friction curve -- exponential decay above stopSpeed, a
// flat deceleration below it, which is what actually avoids the "ice skating"
// feel a naive velocity model gets).
//
// Kinematic, not dynamic: PhysicsBodyDesc3D's rotation-lock fields never
// reach Rapier (see engine/physics/README.md), so a dynamic capsule would
// tip over. This class owns its own Rapier handles and must be despawn()'d
// before the PhysicsSystem that created them shuts down -- see
// engine/character/README.md for why this is a plain class owned by a
// screen, not a typed service.
// -----------------------------------------------------------------------------
class CharacterController3D {
public:
    struct Config {
        f32 walkSpeed = 4.0f;                  // m/s -- research baseline ~3.5-4.5 m/s
        f32 sprintSpeed = 7.0f;                 // m/s
        f32 groundAcceleration = 40.0f;         // m/s^2 -- reaches walkSpeed in ~0.1s
        f32 groundFriction = 10.0f;              // m/s^2 flat deceleration below stopSpeed
        f32 stopSpeed = 1.0f;                    // m/s -- below this, friction is flat, not exponential
        f32 jumpSpeed = 5.0f;                    // m/s, applied once on jump
        f32 gravity = 9.8f;                      // m/s^2
        f32 coyoteTimeSeconds = 0.12f;           // research baseline: 0.10-0.15s
        f32 capsuleHalfHeight = 0.5f;            // cylinder half-height; total height = 2*(half+radius)
        f32 capsuleRadius = 0.35f;
    };

    void spawn(physics::PhysicsWorld3D& world, Vector3 startPosition, const Config& config = {});
    void despawn(physics::PhysicsWorld3D& world) noexcept;

    void step(physics::PhysicsWorld3D& world, const CharacterMoveInput& input, f32 dt);

    [[nodiscard]] Vector3 position() const noexcept { return m_position; }
    [[nodiscard]] f32 eyeHeight() const noexcept { return m_config.capsuleHalfHeight + m_config.capsuleRadius; }
    [[nodiscard]] f32 horizontalSpeed() const noexcept;
    [[nodiscard]] bool grounded() const noexcept { return m_grounded; }

private:
    physics::PhysicsBody3D m_body{};
    physics::PhysicsCollider3D m_collider{};
    Vector3 m_position{0.0f, 0.0f, 0.0f};
    Vector3 m_horizontalVelocity{0.0f, 0.0f, 0.0f}; // y is always 0 here
    f32 m_verticalVelocity = 0.0f;
    bool m_grounded = false;
    f32 m_coyoteTimer = 0.0f;
    Config m_config{};
};

} // namespace biofuel::engine::character
