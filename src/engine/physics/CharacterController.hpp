#pragma once

#include "engine/physics/PhysicsSystem.hpp"

namespace biofuel::engine::physics {

// ------------------------------------------------------------------------------
// CharacterControllerConfig2D
// ------------------------------------------------------------------------------
struct CharacterControllerConfig2D {
    f32 maxSlopeAngle = 45.0f;
    f32 stepHeight = 0.3f;
    f32 snapToGround = 0.1f;
    f32 skinWidth = 0.02f;
    f32 speed = 5.0f;
    f32 gravity = 25.0f;
};

// ------------------------------------------------------------------------------
// CharacterController2D
//
// Wraps a kinematic body and provides move() with raycast-based collision
// resolution: wall sliding, ground snapping, and step-up.
// ------------------------------------------------------------------------------
class CharacterController2D {
public:
    CharacterController2D(PhysicsWorld2D& world,
                          PhysicsBody2D body,
                          const CharacterControllerConfig2D& config = {});

    void move(Vector2 direction, f32 dt);
    void jump(f32 impulse);
    void teleport(Vector2 position);

    [[nodiscard]] bool isGrounded() const noexcept { return m_grounded; }
    [[nodiscard]] Vector2 groundNormal() const noexcept { return m_groundNormal; }
    [[nodiscard]] const CharacterControllerConfig2D& config() const noexcept { return m_config; }
    void setConfig(const CharacterControllerConfig2D& config) noexcept { m_config = config; }
    [[nodiscard]] PhysicsBody2D body() const noexcept { return m_body; }

private:
    [[nodiscard]] bool castGroundRay(Vector2 origin, f32& outDistance, Vector2& outNormal) const;
    void applyGravity(f32 dt);
    void snapDown();

    PhysicsWorld2D* m_world = nullptr;
    PhysicsBody2D m_body{};
    CharacterControllerConfig2D m_config{};
    bool m_grounded = false;
    Vector2 m_groundNormal{0.0f, 1.0f};
    f32 m_verticalVelocity = 0.0f;
};

// ------------------------------------------------------------------------------
// CharacterControllerConfig3D
// ------------------------------------------------------------------------------
struct CharacterControllerConfig3D {
    f32 maxSlopeAngle = 45.0f;
    f32 stepHeight = 0.3f;
    f32 snapToGround = 0.1f;
    f32 skinWidth = 0.02f;
    f32 speed = 5.0f;
    f32 gravity = 25.0f;
    f32 jumpImpulse = 10.0f;
};

// ------------------------------------------------------------------------------
// CharacterController3D
//
// 3D kinematic character controller with wall sliding, ground snapping,
// step-up, and jump support.
// ------------------------------------------------------------------------------
class CharacterController3D {
public:
    CharacterController3D(PhysicsWorld3D& world,
                          PhysicsBody3D body,
                          const CharacterControllerConfig3D& config = {});

    void move(Vector3 direction, f32 dt);
    void jump();
    void teleport(Vector3 position);

    [[nodiscard]] bool isGrounded() const noexcept { return m_grounded; }
    [[nodiscard]] Vector3 groundNormal() const noexcept { return m_groundNormal; }
    [[nodiscard]] const CharacterControllerConfig3D& config() const noexcept { return m_config; }
    void setConfig(const CharacterControllerConfig3D& config) noexcept { m_config = config; }
    [[nodiscard]] PhysicsBody3D body() const noexcept { return m_body; }

private:
    [[nodiscard]] bool castGroundRay(Vector3 origin, f32& outDistance, Vector3& outNormal) const;
    void applyGravity(f32 dt);
    void snapDown();

    PhysicsWorld3D* m_world = nullptr;
    PhysicsBody3D m_body{};
    CharacterControllerConfig3D m_config{};
    bool m_grounded = false;
    Vector3 m_groundNormal{0.0f, 1.0f, 0.0f};
    f32 m_verticalVelocity = 0.0f;
};

} // namespace biofuel::engine::physics
