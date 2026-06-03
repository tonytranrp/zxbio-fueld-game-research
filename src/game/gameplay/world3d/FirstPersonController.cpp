#include "game/gameplay/world3d/FirstPersonController.hpp"

#include <algorithm>
#include <cmath>

namespace biofuel::game::gameplay::world3d {

void FirstPersonController::reset(const Vector3 feetPosition) noexcept {
    m_position = feetPosition;
    m_yaw = 0.0f;
    m_pitch = 0.0f;
    m_verticalVelocity = 0.0f;
    m_horizontalSpeed = 0.0f;
    m_grounded = false;
}

void FirstPersonController::update(const f32 dt, const HeightFn& groundHeightAt) noexcept {
    if (dt <= 0.0f) {
        return;
    }

    // --- Mouse look -----------------------------------------------------------
    const Vector2 mouse = GetMouseDelta();
    m_yaw -= mouse.x * m_config.mouseSensitivity;
    m_pitch = std::clamp(
        m_pitch - mouse.y * m_config.mouseSensitivity,
        -m_config.pitchLimit,
        m_config.pitchLimit);

    // --- Horizontal movement basis (flat, from yaw) ---------------------------
    const f32 sinYaw = std::sin(m_yaw);
    const f32 cosYaw = std::cos(m_yaw);
    const Vector3 forward{sinYaw, 0.0f, cosYaw};
    const Vector3 right{-cosYaw, 0.0f, sinYaw};

    Vector3 wish{0.0f, 0.0f, 0.0f};
    if (IsKeyDown(KEY_W)) { wish.x += forward.x; wish.z += forward.z; }
    if (IsKeyDown(KEY_S)) { wish.x -= forward.x; wish.z -= forward.z; }
    if (IsKeyDown(KEY_D)) { wish.x += right.x;   wish.z += right.z; }
    if (IsKeyDown(KEY_A)) { wish.x -= right.x;   wish.z -= right.z; }

    const f32 wishLen = std::sqrt(wish.x * wish.x + wish.z * wish.z);
    f32 speed = m_config.moveSpeed;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        speed *= m_config.sprintMultiplier;
    }
    if (wishLen > 0.0001f) {
        wish.x = (wish.x / wishLen) * speed;
        wish.z = (wish.z / wishLen) * speed;
        m_position.x += wish.x * dt;
        m_position.z += wish.z * dt;
        m_horizontalSpeed = speed;
    } else {
        m_horizontalSpeed = 0.0f;
    }

    // --- Jump + gravity + ground snapping ------------------------------------
    const f32 groundUnderfoot = groundHeightAt(m_position.x, m_position.z);
    if (m_grounded && IsKeyPressed(KEY_SPACE)) {
        m_verticalVelocity = m_config.jumpSpeed;
        m_grounded = false;
    }

    m_verticalVelocity -= m_config.gravity * dt;
    m_position.y += m_verticalVelocity * dt;

    // Sample again after the horizontal step so walking up slopes lifts us.
    const f32 ground = std::max(groundUnderfoot, groundHeightAt(m_position.x, m_position.z));
    if (m_position.y <= ground) {
        m_position.y = ground;
        m_verticalVelocity = 0.0f;
        m_grounded = true;
    } else {
        m_grounded = false;
    }
}

Vector3 FirstPersonController::eyePosition() const noexcept {
    return Vector3{m_position.x, m_position.y + m_config.eyeHeight, m_position.z};
}

Camera3D FirstPersonController::camera() const noexcept {
    const Vector3 eye = eyePosition();
    const f32 cosPitch = std::cos(m_pitch);
    const Vector3 dir{
        cosPitch * std::sin(m_yaw),
        std::sin(m_pitch),
        cosPitch * std::cos(m_yaw),
    };
    return Camera3D{
        .position = eye,
        .target = Vector3{eye.x + dir.x, eye.y + dir.y, eye.z + dir.z},
        .up = Vector3{0.0f, 1.0f, 0.0f},
        .fovy = 70.0f,
        .projection = CAMERA_PERSPECTIVE,
    };
}

} // namespace biofuel::game::gameplay::world3d
