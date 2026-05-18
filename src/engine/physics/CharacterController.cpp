#include "engine/physics/CharacterController.hpp"

#include <algorithm>
#include <cmath>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

namespace biofuel::engine::physics {

namespace {

[[nodiscard]] inline f32 degToRad(const f32 degrees) noexcept {
    return degrees * (PI / 180.0f);
}

[[nodiscard]] inline f32 lengthSq(const Vector2 v) noexcept {
    return v.x * v.x + v.y * v.y;
}

[[nodiscard]] inline f32 length(const Vector2 v) noexcept {
    return std::sqrt(lengthSq(v));
}

[[nodiscard]] inline f32 lengthSq(const Vector3 v) noexcept {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

[[nodiscard]] inline f32 length(const Vector3 v) noexcept {
    return std::sqrt(lengthSq(v));
}

[[nodiscard]] inline Vector2 normalize(const Vector2 v) noexcept {
    const f32 len = length(v);
    if (len < 1e-8f) {
        return Vector2{0.0f, 0.0f};
    }
    return Vector2{v.x / len, v.y / len};
}

[[nodiscard]] inline Vector3 normalize(const Vector3 v) noexcept {
    const f32 len = length(v);
    if (len < 1e-8f) {
        return Vector3{0.0f, 0.0f, 0.0f};
    }
    return Vector3{v.x / len, v.y / len, v.z / len};
}

[[nodiscard]] constexpr f32 dot(const Vector2 a, const Vector2 b) noexcept {
    return a.x * b.x + a.y * b.y;
}

[[nodiscard]] constexpr f32 dot(const Vector3 a, const Vector3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

} // namespace

// ------------------------------------------------------------------------------
// CharacterController2D
// ------------------------------------------------------------------------------

CharacterController2D::CharacterController2D(
    PhysicsWorld2D& world,
    const PhysicsBody2D body,
    const CharacterControllerConfig2D& config)
    : m_world(&world)
    , m_body(body)
    , m_config(config)
{
}

void CharacterController2D::move(const Vector2 direction, const f32 dt) {
    if (!m_world || !m_body || dt <= 0.0f) {
        return;
    }

    const auto pose = m_world->bodyPose(m_body);
    if (!pose.valid) {
        return;
    }

    const f32 safeDt = std::min(dt, 0.1f);

    const Vector2 inputDir = normalize(direction);
    const Vector2 desiredDelta{
        inputDir.x * m_config.speed * safeDt,
        inputDir.y * m_config.speed * safeDt,
    };

    // --- Ground detection ---
    f32 groundDist = 0.0f;
    Vector2 groundNorm{0.0f, 1.0f};
    m_grounded = castGroundRay(pose.position, groundDist, groundNorm);
    m_groundNormal = m_grounded ? groundNorm : Vector2{0.0f, 1.0f};

    // --- Horizontal movement with wall sliding ---
    const Vector2 horizontalDelta{desiredDelta.x, 0.0f};
    const f32 hLen = std::abs(horizontalDelta.x);
    Vector2 finalDelta = horizontalDelta;

    if (hLen > 1e-6f) {
        const Vector2 hDir{horizontalDelta.x > 0.0f ? 1.0f : -1.0f, 0.0f};
        const f32 castDist = hLen + m_config.skinWidth;

        const auto hit = m_world->raycast(pose.position, hDir, castDist, true);
        if (hit.has_value()) {
            const Vector2 wallNormal = hit->normal;

            // Slide along wall: project remaining movement onto tangent
            const Vector2 slideDir = normalize(
                Vector2{0.0f, (wallNormal.x > 0.0f ? 1.0f : -1.0f)});

            const f32 penetration = hLen - (hit->timeOfImpact - m_config.skinWidth);
            const f32 slideAmount = std::max(penetration, 0.0f);

            // Cast in slide direction
            const Vector2 slideOrigin{
                pose.position.x + hDir.x * (hit->timeOfImpact - m_config.skinWidth),
                pose.position.y,
            };
            const auto slideHit = m_world->raycast(slideOrigin, slideDir, slideAmount + m_config.skinWidth, true);
            if (slideHit.has_value()) {
                finalDelta = Vector2{
                    hDir.x * std::max(hit->timeOfImpact - m_config.skinWidth, 0.0f),
                    0.0f,
                };
            } else {
                finalDelta = Vector2{
                    hDir.x * std::max(hit->timeOfImpact - m_config.skinWidth, 0.0f),
                    slideDir.y * slideAmount,
                };
            }
        }
    }

    // --- Vertical movement ---
    if (m_grounded) {
        m_verticalVelocity = 0.0f;
        // Snap to ground
        if (groundDist <= m_config.snapToGround) {
            finalDelta.y = -groundDist;
        }
    } else {
        applyGravity(safeDt);
        finalDelta.y += m_verticalVelocity * safeDt;
    }

    // --- Apply final position ---
    const Vector2 newPosition{
        pose.position.x + finalDelta.x,
        pose.position.y + finalDelta.y,
    };
    m_world->setBodyPosition(m_body, newPosition, pose.rotationRadians);
}

void CharacterController2D::jump(const f32 impulse) {
    if (!m_grounded) {
        return;
    }
    m_verticalVelocity = impulse;
    m_grounded = false;
}

void CharacterController2D::teleport(const Vector2 position) {
    if (!m_world || !m_body) {
        return;
    }
    const auto pose = m_world->bodyPose(m_body);
    m_world->setBodyPosition(m_body, position, pose.rotationRadians);
    m_verticalVelocity = 0.0f;
}

bool CharacterController2D::castGroundRay(
    const Vector2 origin,
    f32& outDistance,
    Vector2& outNormal) const
{
    const f32 maxDist = m_config.snapToGround + m_config.skinWidth + 0.5f;
    const auto hit = m_world->raycast(origin, Vector2{0.0f, -1.0f}, maxDist, true);
    if (!hit.has_value()) {
        outDistance = maxDist;
        outNormal = Vector2{0.0f, 1.0f};
        return false;
    }

    outDistance = hit->timeOfImpact;
    outNormal = hit->normal;

    // Check slope angle
    const f32 slopeDot = dot(outNormal, Vector2{0.0f, 1.0f});
    const f32 maxSlopeDot = std::cos(degToRad(m_config.maxSlopeAngle));
    return slopeDot >= maxSlopeDot;
}

void CharacterController2D::applyGravity(const f32 dt) {
    m_verticalVelocity -= m_config.gravity * dt;
}

void CharacterController2D::snapDown() {
    if (!m_world || !m_body) {
        return;
    }
    const auto pose = m_world->bodyPose(m_body);
    if (!pose.valid) {
        return;
    }
    const auto hit = m_world->raycast(pose.position, Vector2{0.0f, -1.0f}, m_config.snapToGround, true);
    if (hit.has_value() && hit->timeOfImpact <= m_config.snapToGround) {
        m_world->setBodyPosition(
            m_body,
            Vector2{pose.position.x, pose.position.y - hit->timeOfImpact},
            pose.rotationRadians);
    }
}

// ------------------------------------------------------------------------------
// CharacterController3D
// ------------------------------------------------------------------------------

CharacterController3D::CharacterController3D(
    PhysicsWorld3D& world,
    const PhysicsBody3D body,
    const CharacterControllerConfig3D& config)
    : m_world(&world)
    , m_body(body)
    , m_config(config)
{
}

void CharacterController3D::move(const Vector3 direction, const f32 dt) {
    if (!m_world || !m_body || dt <= 0.0f) {
        return;
    }

    const auto pose = m_world->bodyPose(m_body);
    if (!pose.valid) {
        return;
    }

    const f32 safeDt = std::min(dt, 0.1f);

    const Vector3 inputDir = normalize(direction);
    const Vector3 desiredDelta{
        inputDir.x * m_config.speed * safeDt,
        0.0f,
        inputDir.z * m_config.speed * safeDt,
    };

    // --- Ground detection ---
    f32 groundDist = 0.0f;
    Vector3 groundNorm{0.0f, 1.0f, 0.0f};
    m_grounded = castGroundRay(pose.position, groundDist, groundNorm);
    m_groundNormal = m_grounded ? groundNorm : Vector3{0.0f, 1.0f, 0.0f};

    // --- Horizontal movement (XZ plane) with wall sliding ---
    const Vector3 horizontalDelta{desiredDelta.x, 0.0f, desiredDelta.z};
    const f32 hLen = length(horizontalDelta);
    Vector3 finalDelta = horizontalDelta;

    if (hLen > 1e-6f) {
        const Vector3 hDir = normalize(horizontalDelta);
        const f32 castDist = hLen + m_config.skinWidth;

        const auto hit = m_world->raycast(pose.position, hDir, castDist, true);
        if (hit.has_value()) {
            const Vector3 wallNormal = hit->normal;

            // Slide direction: project hDir onto plane perpendicular to wall normal
            const f32 proj = dot(hDir, wallNormal);
            Vector3 slideDir{
                hDir.x - wallNormal.x * proj,
                0.0f,
                hDir.z - wallNormal.z * proj,
            };
            const f32 slideLen = length(slideDir);
            if (slideLen > 1e-6f) {
                slideDir = Vector3{slideDir.x / slideLen, 0.0f, slideDir.z / slideLen};
            }

            const f32 penetration = hLen - (hit->timeOfImpact - m_config.skinWidth);
            const f32 slideAmount = std::max(penetration, 0.0f);

            const Vector3 slideOrigin{
                pose.position.x + hDir.x * (hit->timeOfImpact - m_config.skinWidth),
                pose.position.y,
                pose.position.z + hDir.z * (hit->timeOfImpact - m_config.skinWidth),
            };

            if (slideLen > 1e-6f) {
                const auto slideHit = m_world->raycast(slideOrigin, slideDir, slideAmount + m_config.skinWidth, true);
                if (slideHit.has_value()) {
                    finalDelta = Vector3{
                        hDir.x * std::max(hit->timeOfImpact - m_config.skinWidth, 0.0f),
                        0.0f,
                        hDir.z * std::max(hit->timeOfImpact - m_config.skinWidth, 0.0f),
                    };
                } else {
                    finalDelta = Vector3{
                        hDir.x * std::max(hit->timeOfImpact - m_config.skinWidth, 0.0f) + slideDir.x * slideAmount,
                        0.0f,
                        hDir.z * std::max(hit->timeOfImpact - m_config.skinWidth, 0.0f) + slideDir.z * slideAmount,
                    };
                }
            } else {
                finalDelta = Vector3{
                    hDir.x * std::max(hit->timeOfImpact - m_config.skinWidth, 0.0f),
                    0.0f,
                    hDir.z * std::max(hit->timeOfImpact - m_config.skinWidth, 0.0f),
                };
            }
        }
    }

    // --- Vertical movement ---
    if (m_grounded) {
        m_verticalVelocity = 0.0f;
        if (groundDist <= m_config.snapToGround) {
            finalDelta.y = -groundDist;
        }
    } else {
        applyGravity(safeDt);
        finalDelta.y += m_verticalVelocity * safeDt;
    }

    // --- Apply final position ---
    const Vector3 newPosition{
        pose.position.x + finalDelta.x,
        pose.position.y + finalDelta.y,
        pose.position.z + finalDelta.z,
    };
    m_world->setBodyPosition(m_body, newPosition);
}

void CharacterController3D::jump() {
    if (!m_grounded || !m_world || !m_body) {
        return;
    }
    m_verticalVelocity = m_config.jumpImpulse;
    m_grounded = false;
    m_world->setBodyLinearVelocity(
        m_body,
        Vector3{0.0f, m_config.jumpImpulse, 0.0f});
}

void CharacterController3D::teleport(const Vector3 position) {
    if (!m_world || !m_body) {
        return;
    }
    m_world->setBodyPosition(m_body, position);
    m_verticalVelocity = 0.0f;
}

bool CharacterController3D::castGroundRay(
    const Vector3 origin,
    f32& outDistance,
    Vector3& outNormal) const
{
    const f32 maxDist = m_config.snapToGround + m_config.skinWidth + 0.5f;
    const auto hit = m_world->raycast(origin, Vector3{0.0f, -1.0f, 0.0f}, maxDist, true);
    if (!hit.has_value()) {
        outDistance = maxDist;
        outNormal = Vector3{0.0f, 1.0f, 0.0f};
        return false;
    }

    outDistance = hit->timeOfImpact;
    outNormal = hit->normal;

    const f32 slopeDot = dot(outNormal, Vector3{0.0f, 1.0f, 0.0f});
    const f32 maxSlopeDot = std::cos(degToRad(m_config.maxSlopeAngle));
    return slopeDot >= maxSlopeDot;
}

void CharacterController3D::applyGravity(const f32 dt) {
    m_verticalVelocity -= m_config.gravity * dt;
}

void CharacterController3D::snapDown() {
    if (!m_world || !m_body) {
        return;
    }
    const auto pose = m_world->bodyPose(m_body);
    if (!pose.valid) {
        return;
    }
    const auto hit = m_world->raycast(
        pose.position, Vector3{0.0f, -1.0f, 0.0f}, m_config.snapToGround, true);
    if (hit.has_value() && hit->timeOfImpact <= m_config.snapToGround) {
        m_world->setBodyPosition(
            m_body,
            Vector3{
                pose.position.x,
                pose.position.y - hit->timeOfImpact,
                pose.position.z,
            });
    }
}

} // namespace biofuel::engine::physics
