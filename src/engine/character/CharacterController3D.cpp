#include "CharacterController3D.hpp"

#include <algorithm>
#include <cmath>

namespace biofuel::engine::character {

void CharacterController3D::spawn(physics::PhysicsWorld3D& world, const Vector3 startPosition, const Config& config) {
    m_config = config;
    m_position = startPosition;
    m_horizontalVelocity = Vector3{0.0f, 0.0f, 0.0f};
    m_verticalVelocity = 0.0f;
    m_grounded = false;
    m_coyoteTimer = 0.0f;
    m_body = world.createBody({.kind = physics::PhysicsBodyKind::KinematicPosition, .position = startPosition});
    m_collider = world.attachCapsule(m_body, {.halfHeight = config.capsuleHalfHeight, .radius = config.capsuleRadius});
}

void CharacterController3D::despawn(physics::PhysicsWorld3D& world) noexcept {
    if (m_body) {
        world.removeBody(m_body);
    }
    m_body = physics::PhysicsBody3D{};
    m_collider = physics::PhysicsCollider3D{};
}

f32 CharacterController3D::horizontalSpeed() const noexcept {
    return std::sqrt(m_horizontalVelocity.x * m_horizontalVelocity.x
                    + m_horizontalVelocity.z * m_horizontalVelocity.z);
}

void CharacterController3D::step(physics::PhysicsWorld3D& world, const CharacterMoveInput& input, const f32 dt) {
    // 1. Wish direction in world space from local input (x=strafe, y=forward)
    // rotated by camera yaw -- matches FirstPersonCamera::right()/flatForward()'s
    // convention (right = {cosYaw, 0, -sinYaw}, flatForward = {sinYaw, 0, cosYaw}).
    Vector2 axis = input.moveAxis;
    const f32 axisLenSq = axis.x * axis.x + axis.y * axis.y;
    if (axisLenSq > 1.0f) {
        const f32 axisLen = std::sqrt(axisLenSq);
        axis.x /= axisLen;
        axis.y /= axisLen;
    }
    const f32 axisLen = std::sqrt(axis.x * axis.x + axis.y * axis.y);
    const f32 cosYaw = std::cos(input.yawRadians);
    const f32 sinYaw = std::sin(input.yawRadians);
    const Vector3 wishDirRaw{
        cosYaw * axis.x + sinYaw * axis.y,
        0.0f,
        -sinYaw * axis.x + cosYaw * axis.y,
    };
    const Vector3 wishDir = axisLen > 1.0e-5f
        ? Vector3{wishDirRaw.x / axisLen, 0.0f, wishDirRaw.z / axisLen}
        : Vector3{0.0f, 0.0f, 0.0f};
    const f32 wishSpeed = (input.sprint ? m_config.sprintSpeed : m_config.walkSpeed) * axisLen;

    // 2. Ground friction -- two-regime model (research baseline: Source
    // engine's PM_Friction): exponential decay above stopSpeed, a flat
    // deceleration below it. The flat floor is what actually kills the "ice
    // skating" feel a pure-exponential decay leaves forever approaching zero.
    const f32 currentSpeed = horizontalSpeed();
    if (currentSpeed > 1.0e-4f) {
        const f32 control = currentSpeed < m_config.stopSpeed ? m_config.stopSpeed : currentSpeed;
        const f32 drop = control * m_config.groundFriction * dt;
        const f32 newSpeed = std::max(currentSpeed - drop, 0.0f);
        const f32 scale = newSpeed / currentSpeed;
        m_horizontalVelocity.x *= scale;
        m_horizontalVelocity.z *= scale;
    }

    // 3. Accelerate toward wishDir*wishSpeed, capped so it never overshoots
    // (research baseline: reaches wishSpeed in ~0.1s regardless of what that
    // speed is, since the increment itself scales with wishSpeed).
    if (wishSpeed > 0.0f) {
        const f32 currentSpeedInWishDir = m_horizontalVelocity.x * wishDir.x + m_horizontalVelocity.z * wishDir.z;
        const f32 addSpeed = wishSpeed - currentSpeedInWishDir;
        if (addSpeed > 0.0f) {
            const f32 accelSpeed = std::min(m_config.groundAcceleration * dt * wishSpeed, addSpeed);
            m_horizontalVelocity.x += wishDir.x * accelSpeed;
            m_horizontalVelocity.z += wishDir.z * accelSpeed;
        }
    }

    // 4. Vertical: gravity + jump, with coyote time. Reads last step's
    // m_grounded (this step's move hasn't resolved yet) -- updated at the
    // end of this function for the *next* call to read.
    if (m_grounded) {
        m_coyoteTimer = m_config.coyoteTimeSeconds;
        if (!input.jump) {
            m_verticalVelocity = 0.0f;
        }
    } else {
        m_coyoteTimer = std::max(0.0f, m_coyoteTimer - dt);
    }
    if (input.jump && (m_grounded || m_coyoteTimer > 0.0f)) {
        m_verticalVelocity = m_config.jumpSpeed;
        m_grounded = false;
        m_coyoteTimer = 0.0f;
    } else if (!m_grounded) {
        m_verticalVelocity -= m_config.gravity * dt;
    }

    // 5. Resolve the move through the kinematic character controller.
    const Vector3 desiredTranslation{
        m_horizontalVelocity.x * dt,
        m_verticalVelocity * dt,
        m_horizontalVelocity.z * dt,
    };
    const physics::CharacterControllerDesc3D desc{
        .offset = 0.01f,
        // Only snap while already grounded -- snapping every tick would glue
        // a jumping/falling character straight back onto the ground.
        .snapToGround = m_grounded ? 0.2f : 0.0f,
        .autostepMaxHeight = 0.0f, // off by default -- Rapier's own docs call this expensive
    };
    const physics::CharacterMovement3D result = world.moveCharacter(
        m_collider, m_body, m_position, desiredTranslation, dt, desc);
    if (result.valid) {
        m_position.x += result.translation.x;
        m_position.y += result.translation.y;
        m_position.z += result.translation.z;
        world.setBodyPosition(m_body, m_position);
        m_grounded = result.grounded;
    }
}

} // namespace biofuel::engine::character
