#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>

namespace biofuel::engine::physics {

enum class PhysicsWorldKind : u8 {
    World2D,
    World3D,
};

enum class PhysicsBodyKind : u8 {
    Fixed,
    Dynamic,
    KinematicPosition,
    KinematicVelocity,
};

enum class PhysicsContactPhase : u8 {
    Started,
    Ended,
};

struct PhysicsBody2D {
    u64 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
};

struct PhysicsCollider2D {
    u64 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
};

struct PhysicsBody3D {
    u64 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
};

struct PhysicsCollider3D {
    u64 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
};

struct PhysicsBodyDesc2D {
    PhysicsBodyKind kind = PhysicsBodyKind::Dynamic;
    Vector2 position{0.0f, 0.0f};
    Vector2 linearVelocity{0.0f, 0.0f};
    f32 rotationRadians = 0.0f;
    f32 angularVelocity = 0.0f;
    bool canSleep = true;
};

struct PhysicsBodyDesc3D {
    PhysicsBodyKind kind = PhysicsBodyKind::Dynamic;
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 linearVelocity{0.0f, 0.0f, 0.0f};
    bool canSleep = true;
};

struct BoxColliderDesc2D {
    Vector2 halfExtents{0.5f, 0.5f};
    f32 density = 1.0f;
    bool sensor = false;
};

struct CircleColliderDesc {
    f32 radius = 0.5f;
    f32 density = 1.0f;
    bool sensor = false;
};

struct CapsuleColliderDesc2D {
    f32 halfHeight = 0.5f;
    f32 radius = 0.25f;
    f32 density = 1.0f;
    bool sensor = false;
};

struct CuboidColliderDesc {
    Vector3 halfExtents{0.5f, 0.5f, 0.5f};
    f32 density = 1.0f;
    bool sensor = false;
};

struct BallColliderDesc {
    f32 radius = 0.5f;
    f32 density = 1.0f;
    bool sensor = false;
};

struct CapsuleColliderDesc3D {
    f32 halfHeight = 0.5f;
    f32 radius = 0.25f;
    f32 density = 1.0f;
    bool sensor = false;
};

struct PhysicsBodyPose2D {
    bool valid = false;
    Vector2 position{0.0f, 0.0f};
    f32 rotationRadians = 0.0f;
};

struct PhysicsBodyPose3D {
    bool valid = false;
    Vector3 position{0.0f, 0.0f, 0.0f};
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
};

struct PhysicsRayHit2D {
    PhysicsCollider2D collider{};
    Vector2 point{0.0f, 0.0f};
    Vector2 normal{0.0f, 0.0f};
    f32 timeOfImpact = 0.0f;
};

struct PhysicsRayHit3D {
    PhysicsCollider3D collider{};
    Vector3 point{0.0f, 0.0f, 0.0f};
    Vector3 normal{0.0f, 0.0f, 0.0f};
    f32 timeOfImpact = 0.0f;
};

struct PhysicsContactEvent {
    PhysicsWorldKind world = PhysicsWorldKind::World2D;
    PhysicsContactPhase phase = PhysicsContactPhase::Started;
    u64 colliderA = 0U;
    u64 colliderB = 0U;
};

struct PixelToMeterScale {
    f32 pixelsPerMeter = 32.0f;

    [[nodiscard]] constexpr f32 pixelsToMeters(const f32 pixels) const noexcept {
        return pixelsPerMeter <= 0.0f ? pixels : pixels / pixelsPerMeter;
    }

    [[nodiscard]] constexpr f32 metersToPixels(const f32 meters) const noexcept {
        return pixelsPerMeter <= 0.0f ? meters : meters * pixelsPerMeter;
    }

    [[nodiscard]] constexpr Vector2 pixelsToMeters(const Vector2 pixels) const noexcept {
        return Vector2{pixelsToMeters(pixels.x), pixelsToMeters(pixels.y)};
    }

    [[nodiscard]] constexpr Vector2 metersToPixels(const Vector2 meters) const noexcept {
        return Vector2{metersToPixels(meters.x), metersToPixels(meters.y)};
    }
};

} // namespace biofuel::engine::physics
