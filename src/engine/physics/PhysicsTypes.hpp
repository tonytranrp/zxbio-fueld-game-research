#pragma once

#include "engine/core/Types.hpp"
#include "engine/core/units/EngineUnits.hpp"
#include <raylib.h>
#include <algorithm>
#include <type_traits>

namespace biofuel::engine::physics {

using PixelToMeterScale = ::biofuel::engine::core::units::PixelToMeterScale;

// =============================================================================
// Enums — all enum class, u8 backing, verify with static_assert
// =============================================================================

enum class PhysicsWorldKind : u8 {
    World2D,
    World3D,
};
static_assert(sizeof(PhysicsWorldKind) == 1,
              "PhysicsWorldKind must be 1 byte (u8 backing) for FFI");

enum class PhysicsBodyKind : u8 {
    Fixed,
    Dynamic,
    KinematicPosition,
    KinematicVelocity,
};
static_assert(sizeof(PhysicsBodyKind) == 1,
              "PhysicsBodyKind must be 1 byte (u8 backing) for FFI");

enum class PhysicsContactPhase : u8 {
    Started,
    Ended,
};
static_assert(sizeof(PhysicsContactPhase) == 1,
              "PhysicsContactPhase must be 1 byte (u8 backing) for FFI");

enum class PhysicsShapeRole : u8 {
    Unknown,
    StaticScene,
    DynamicProp,
    KinematicInteractor,
    Sensor,
};
static_assert(sizeof(PhysicsShapeRole) == 1,
              "PhysicsShapeRole must be 1 byte (u8 backing) for FFI");

enum class JointType : u8 {
    Fixed,
    Revolute,
    Prismatic,
    Spherical,
    Spring,
};
static_assert(sizeof(JointType) == 1,
              "JointType must be 1 byte (u8 backing) for FFI");

// =============================================================================
// Opaque handles — wrap Rapier u64 indices, must pack to exactly 8 bytes
// =============================================================================

struct PhysicsBody2D {
    u64 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
    [[nodiscard]] constexpr bool operator==(const PhysicsBody2D&) const noexcept = default;
};
static_assert(sizeof(PhysicsBody2D) == 8,
              "PhysicsBody2D must be exactly 8 bytes to match Rapier's u64 handle packing");
static_assert(std::is_trivially_copyable_v<PhysicsBody2D>,
              "PhysicsBody2D must be trivially copyable — it crosses FFI boundary as u64");

struct PhysicsCollider2D {
    u64 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
    [[nodiscard]] constexpr bool operator==(const PhysicsCollider2D&) const noexcept = default;
};
static_assert(sizeof(PhysicsCollider2D) == 8,
              "PhysicsCollider2D must be exactly 8 bytes to match Rapier's u64 handle packing");
static_assert(std::is_trivially_copyable_v<PhysicsCollider2D>,
              "PhysicsCollider2D must be trivially copyable — it crosses FFI boundary as u64");

struct PhysicsBody3D {
    u64 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
    [[nodiscard]] constexpr bool operator==(const PhysicsBody3D&) const noexcept = default;
};
static_assert(sizeof(PhysicsBody3D) == 8,
              "PhysicsBody3D must be exactly 8 bytes to match Rapier's u64 handle packing");
static_assert(std::is_trivially_copyable_v<PhysicsBody3D>,
              "PhysicsBody3D must be trivially copyable — it crosses FFI boundary as u64");

struct PhysicsCollider3D {
    u64 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
    [[nodiscard]] constexpr bool operator==(const PhysicsCollider3D&) const noexcept = default;
};
static_assert(sizeof(PhysicsCollider3D) == 8,
              "PhysicsCollider3D must be exactly 8 bytes to match Rapier's u64 handle packing");
static_assert(std::is_trivially_copyable_v<PhysicsCollider3D>,
              "PhysicsCollider3D must be trivially copyable — it crosses FFI boundary as u64");

struct Joint2D {
    u64 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
    [[nodiscard]] constexpr bool operator==(const Joint2D&) const noexcept = default;
};
static_assert(sizeof(Joint2D) == 8,
              "Joint2D must be exactly 8 bytes to match Rapier's u64 handle packing");
static_assert(std::is_trivially_copyable_v<Joint2D>,
              "Joint2D must be trivially copyable — it crosses FFI boundary as u64");

struct Joint3D {
    u64 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
    [[nodiscard]] constexpr bool operator==(const Joint3D&) const noexcept = default;
};
static_assert(sizeof(Joint3D) == 8,
              "Joint3D must be exactly 8 bytes to match Rapier's u64 handle packing");
static_assert(std::is_trivially_copyable_v<Joint3D>,
              "Joint3D must be trivially copyable — it crosses FFI boundary as u64");

// =============================================================================
// Strongly-typed physics value types — prevent accidental confusion of f32/Vector
// =============================================================================

// --- Scalar physics quantities ---

struct Mass {
    f32 value = 0.0f;

    constexpr Mass() noexcept = default;
    explicit constexpr Mass(const f32 v) noexcept : value(v < 0.0f ? 0.0f : v) {}

    [[nodiscard]] constexpr bool operator==(const Mass&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const Mass&) const noexcept = default;
};
static_assert(sizeof(Mass) == 4, "Mass must be exactly 4 bytes (1 f32)");

struct Density {
    f32 value = 0.0f;

    constexpr Density() noexcept = default;
    explicit constexpr Density(const f32 v) noexcept : value(v < 0.0f ? 0.0f : v) {}

    [[nodiscard]] constexpr bool operator==(const Density&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const Density&) const noexcept = default;
};
static_assert(sizeof(Density) == 4, "Density must be exactly 4 bytes (1 f32)");

struct Friction {
    f32 value = 0.5f;

    constexpr Friction() noexcept = default;
    explicit constexpr Friction(const f32 v) noexcept
        : value(std::clamp(v, 0.0f, 1.0f)) {}

    [[nodiscard]] constexpr bool operator==(const Friction&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const Friction&) const noexcept = default;
};
static_assert(Friction{0.0f}.value == 0.0f, "Friction lower clamp failed at compile-time");
static_assert(Friction{1.0f}.value == 1.0f, "Friction upper clamp failed at compile-time");
static_assert(Friction{-0.5f}.value == 0.0f, "Friction sub-zero clamp failed at compile-time");
static_assert(Friction{2.0f}.value == 1.0f, "Friction above-one clamp failed at compile-time");
static_assert(sizeof(Friction) == 4, "Friction must be exactly 4 bytes (1 f32)");

struct Restitution {
    f32 value = 0.0f;

    constexpr Restitution() noexcept = default;
    explicit constexpr Restitution(const f32 v) noexcept
        : value(std::clamp(v, 0.0f, 1.0f)) {}

    [[nodiscard]] constexpr bool operator==(const Restitution&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const Restitution&) const noexcept = default;
};
static_assert(Restitution{0.0f}.value == 0.0f, "Restitution lower clamp failed at compile-time");
static_assert(Restitution{1.0f}.value == 1.0f, "Restitution upper clamp failed at compile-time");
static_assert(Restitution{-0.5f}.value == 0.0f, "Restitution sub-zero clamp failed at compile-time");
static_assert(Restitution{2.0f}.value == 1.0f, "Restitution above-one clamp failed at compile-time");
static_assert(sizeof(Restitution) == 4, "Restitution must be exactly 4 bytes (1 f32)");

// --- Vector physics quantities ---

struct Force2D {
    Vector2 value{0.0f, 0.0f};

    constexpr Force2D() noexcept = default;
    explicit constexpr Force2D(const Vector2 v) noexcept : value(v) {}
    constexpr Force2D(const f32 x, const f32 y) noexcept : value{x, y} {}

    [[nodiscard]] constexpr bool operator==(const Force2D&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const Force2D&) const noexcept = default;
};
static_assert(sizeof(Force2D) == 8, "Force2D must be exactly 8 bytes (Vector2 = 2 f32)");

struct Force3D {
    Vector3 value{0.0f, 0.0f, 0.0f};

    constexpr Force3D() noexcept = default;
    explicit constexpr Force3D(const Vector3 v) noexcept : value(v) {}
    constexpr Force3D(const f32 x, const f32 y, const f32 z) noexcept : value{x, y, z} {}

    [[nodiscard]] constexpr bool operator==(const Force3D&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const Force3D&) const noexcept = default;
};
static_assert(sizeof(Force3D) == 12, "Force3D must be exactly 12 bytes (Vector3 = 3 f32)");

struct Impulse2D {
    Vector2 value{0.0f, 0.0f};

    constexpr Impulse2D() noexcept = default;
    explicit constexpr Impulse2D(const Vector2 v) noexcept : value(v) {}
    constexpr Impulse2D(const f32 x, const f32 y) noexcept : value{x, y} {}

    [[nodiscard]] constexpr bool operator==(const Impulse2D&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const Impulse2D&) const noexcept = default;
};
static_assert(sizeof(Impulse2D) == 8, "Impulse2D must be exactly 8 bytes (Vector2 = 2 f32)");

struct Impulse3D {
    Vector3 value{0.0f, 0.0f, 0.0f};

    constexpr Impulse3D() noexcept = default;
    explicit constexpr Impulse3D(const Vector3 v) noexcept : value(v) {}
    constexpr Impulse3D(const f32 x, const f32 y, const f32 z) noexcept : value{x, y, z} {}

    [[nodiscard]] constexpr bool operator==(const Impulse3D&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const Impulse3D&) const noexcept = default;
};
static_assert(sizeof(Impulse3D) == 12, "Impulse3D must be exactly 12 bytes (Vector3 = 3 f32)");

struct AngularVelocity2D {
    f32 value = 0.0f;

    constexpr AngularVelocity2D() noexcept = default;
    explicit constexpr AngularVelocity2D(const f32 v) noexcept : value(v) {}

    [[nodiscard]] constexpr bool operator==(const AngularVelocity2D&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const AngularVelocity2D&) const noexcept = default;
};
static_assert(sizeof(AngularVelocity2D) == 4, "AngularVelocity2D must be exactly 4 bytes (1 f32)");

struct AngularVelocity3D {
    Vector3 value{0.0f, 0.0f, 0.0f};

    constexpr AngularVelocity3D() noexcept = default;
    explicit constexpr AngularVelocity3D(const Vector3 v) noexcept : value(v) {}
    constexpr AngularVelocity3D(const f32 x, const f32 y, const f32 z) noexcept : value{x, y, z} {}

    [[nodiscard]] constexpr bool operator==(const AngularVelocity3D&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const AngularVelocity3D&) const noexcept = default;
};
static_assert(sizeof(AngularVelocity3D) == 12, "AngularVelocity3D must be exactly 12 bytes (Vector3 = 3 f32)");

// --- Interaction group strong types ---

struct CollisionGroup {
    u16 group = 0x0001;
    u16 mask = 0xFFFF;

    constexpr CollisionGroup() noexcept = default;
    explicit constexpr CollisionGroup(const u16 g, const u16 m) noexcept : group(g), mask(m) {}

    [[nodiscard]] static constexpr CollisionGroup all() noexcept { return CollisionGroup{0x0001, 0xFFFF}; }
    [[nodiscard]] static constexpr CollisionGroup none() noexcept { return CollisionGroup{0, 0}; }
    [[nodiscard]] static constexpr CollisionGroup groupOnly(const u16 g) noexcept { return CollisionGroup{g, g}; }

    [[nodiscard]] constexpr bool operator==(const CollisionGroup&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const CollisionGroup&) const noexcept = default;

    [[nodiscard]] constexpr bool collidesWith(const CollisionGroup& other) const noexcept {
        return (mask & other.group) != 0U && (other.mask & group) != 0U;
    }
};
static_assert(sizeof(CollisionGroup) == 4,
              "CollisionGroup must be 4 bytes (u16 + u16) for FFI");
static_assert(std::is_trivially_copyable_v<CollisionGroup>,
              "CollisionGroup must be trivially copyable — it crosses FFI boundary");

struct SolverGroup {
    u16 group = 0;
    u16 mask = 0xFFFF;

    constexpr SolverGroup() noexcept = default;
    explicit constexpr SolverGroup(const u16 g, const u16 m) noexcept : group(g), mask(m) {}

    [[nodiscard]] static constexpr SolverGroup all() noexcept { return SolverGroup{0, 0xFFFF}; }
    [[nodiscard]] static constexpr SolverGroup none() noexcept { return SolverGroup{0, 0}; }
    [[nodiscard]] static constexpr SolverGroup groupOnly(const u16 g) noexcept { return SolverGroup{g, g}; }

    [[nodiscard]] constexpr bool operator==(const SolverGroup&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const SolverGroup&) const noexcept = default;

    [[nodiscard]] constexpr bool interactsWith(const SolverGroup& other) const noexcept {
        return (mask & other.group) != 0U && (other.mask & group) != 0U;
    }
};
static_assert(sizeof(SolverGroup) == 4,
              "SolverGroup must be 4 bytes (u16 + u16) for FFI");
static_assert(std::is_trivially_copyable_v<SolverGroup>,
              "SolverGroup must be trivially copyable — it crosses FFI boundary");

// =============================================================================
// Body descriptors — FFI-facing, must be trivially copyable
// =============================================================================

struct PhysicsBodyDesc2D {
    PhysicsBodyKind kind = PhysicsBodyKind::Dynamic;
    Vector2 position{0.0f, 0.0f};
    Vector2 linearVelocity{0.0f, 0.0f};
    f32 rotationRadians = 0.0f;
    f32 angularVelocity = 0.0f;
    f32 linearDamping = 0.0f;
    f32 angularDamping = 0.0f;
    f32 gravityScale = 1.0f;
    bool enableCcd = false;
    bool canSleep = true;
    bool lockTranslationX = false;
    bool lockTranslationY = false;
    bool lockRotation = false;

    [[nodiscard]] constexpr bool operator==(const PhysicsBodyDesc2D& o) const noexcept {
        return position.x == o.position.x && position.y == o.position.y
            && linearVelocity.x == o.linearVelocity.x && linearVelocity.y == o.linearVelocity.y
            && rotationRadians == o.rotationRadians && angularVelocity == o.angularVelocity
            && linearDamping == o.linearDamping && angularDamping == o.angularDamping
            && gravityScale == o.gravityScale && enableCcd == o.enableCcd
            && canSleep == o.canSleep && lockTranslationX == o.lockTranslationX
            && lockTranslationY == o.lockTranslationY && lockRotation == o.lockRotation
            && kind == o.kind;
    }
};
static_assert(std::is_trivially_copyable_v<PhysicsBodyDesc2D>,
              "PhysicsBodyDesc2D must be trivially copyable — it crosses FFI boundary");
static_assert(sizeof(PhysicsBodyDesc2D) <= 64,
              "PhysicsBodyDesc2D must fit within 64 bytes for FFI stack copy");

struct PhysicsBodyDesc3D {
    PhysicsBodyKind kind = PhysicsBodyKind::Dynamic;
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 linearVelocity{0.0f, 0.0f, 0.0f};
    f32 linearDamping = 0.0f;
    f32 angularDamping = 0.0f;
    f32 gravityScale = 1.0f;
    bool enableCcd = false;
    bool canSleep = true;
    bool lockTranslationX = false;
    bool lockTranslationY = false;
    bool lockTranslationZ = false;
    bool lockRotation = false;

    [[nodiscard]] constexpr bool operator==(const PhysicsBodyDesc3D& o) const noexcept {
        return position.x == o.position.x && position.y == o.position.y && position.z == o.position.z
            && linearVelocity.x == o.linearVelocity.x && linearVelocity.y == o.linearVelocity.y && linearVelocity.z == o.linearVelocity.z
            && linearDamping == o.linearDamping && angularDamping == o.angularDamping
            && gravityScale == o.gravityScale && enableCcd == o.enableCcd
            && canSleep == o.canSleep && lockTranslationX == o.lockTranslationX
            && lockTranslationY == o.lockTranslationY && lockTranslationZ == o.lockTranslationZ
            && lockRotation == o.lockRotation && kind == o.kind;
    }
};
static_assert(std::is_trivially_copyable_v<PhysicsBodyDesc3D>,
              "PhysicsBodyDesc3D must be trivially copyable — it crosses FFI boundary");
static_assert(sizeof(PhysicsBodyDesc3D) <= 80,
              "PhysicsBodyDesc3D must fit within 80 bytes for FFI stack copy");

// =============================================================================
// Joint descriptors — FFI-facing, must be trivially copyable
// =============================================================================

struct JointDesc2D {
    JointType type = JointType::Fixed;
    u64 bodyA = 0U;
    u64 bodyB = 0U;
    Vector2 anchorA{0.0f, 0.0f};
    Vector2 anchorB{0.0f, 0.0f};

    [[nodiscard]] constexpr bool operator==(const JointDesc2D& o) const noexcept {
        return type == o.type && bodyA == o.bodyA && bodyB == o.bodyB
            && anchorA.x == o.anchorA.x && anchorA.y == o.anchorA.y
            && anchorB.x == o.anchorB.x && anchorB.y == o.anchorB.y;
    }
};
static_assert(std::is_trivially_copyable_v<JointDesc2D>,
              "JointDesc2D must be trivially copyable — it crosses FFI boundary");
static_assert(sizeof(JointDesc2D) <= 48,
              "JointDesc2D must fit within 48 bytes for FFI stack copy");

struct JointDesc3D {
    JointType type = JointType::Fixed;
    u64 bodyA = 0U;
    u64 bodyB = 0U;
    Vector3 anchorA{0.0f, 0.0f, 0.0f};
    Vector3 anchorB{0.0f, 0.0f, 0.0f};

    [[nodiscard]] constexpr bool operator==(const JointDesc3D& o) const noexcept {
        return type == o.type && bodyA == o.bodyA && bodyB == o.bodyB
            && anchorA.x == o.anchorA.x && anchorA.y == o.anchorA.y && anchorA.z == o.anchorA.z
            && anchorB.x == o.anchorB.x && anchorB.y == o.anchorB.y && anchorB.z == o.anchorB.z;
    }
};
static_assert(std::is_trivially_copyable_v<JointDesc3D>,
              "JointDesc3D must be trivially copyable — it crosses FFI boundary");
static_assert(sizeof(JointDesc3D) <= 64,
              "JointDesc3D must fit within 64 bytes for FFI stack copy");

// =============================================================================
// Collider descriptors — FFI-facing, must be trivially copyable
// =============================================================================

struct BoxColliderDesc2D {
    Vector2 halfExtents{0.5f, 0.5f};
    f32 density = 1.0f;
    bool sensor = false;
    CollisionGroup collisionGroup{};

    [[nodiscard]] constexpr bool operator==(const BoxColliderDesc2D& o) const noexcept {
        return halfExtents.x == o.halfExtents.x && halfExtents.y == o.halfExtents.y
            && density == o.density && sensor == o.sensor && collisionGroup == o.collisionGroup;
    }
};
static_assert(std::is_trivially_copyable_v<BoxColliderDesc2D>,
              "BoxColliderDesc2D must be trivially copyable — it crosses FFI boundary");

struct CircleColliderDesc {
    f32 radius = 0.5f;
    f32 density = 1.0f;
    bool sensor = false;
    CollisionGroup collisionGroup{};

    [[nodiscard]] constexpr bool operator==(const CircleColliderDesc&) const noexcept = default;
};
static_assert(std::is_trivially_copyable_v<CircleColliderDesc>,
              "CircleColliderDesc must be trivially copyable — it crosses FFI boundary");

struct CapsuleColliderDesc2D {
    f32 halfHeight = 0.5f;
    f32 radius = 0.25f;
    f32 density = 1.0f;
    bool sensor = false;
    CollisionGroup collisionGroup{};

    [[nodiscard]] constexpr bool operator==(const CapsuleColliderDesc2D&) const noexcept = default;
};
static_assert(std::is_trivially_copyable_v<CapsuleColliderDesc2D>,
              "CapsuleColliderDesc2D must be trivially copyable — it crosses FFI boundary");

struct CuboidColliderDesc {
    Vector3 halfExtents{0.5f, 0.5f, 0.5f};
    f32 density = 1.0f;
    bool sensor = false;
    CollisionGroup collisionGroup{};

    [[nodiscard]] constexpr bool operator==(const CuboidColliderDesc& o) const noexcept {
        return halfExtents.x == o.halfExtents.x && halfExtents.y == o.halfExtents.y && halfExtents.z == o.halfExtents.z
            && density == o.density && sensor == o.sensor && collisionGroup == o.collisionGroup;
    }
};
static_assert(std::is_trivially_copyable_v<CuboidColliderDesc>,
              "CuboidColliderDesc must be trivially copyable — it crosses FFI boundary");

struct BallColliderDesc {
    f32 radius = 0.5f;
    f32 density = 1.0f;
    bool sensor = false;
    CollisionGroup collisionGroup{};

    [[nodiscard]] constexpr bool operator==(const BallColliderDesc&) const noexcept = default;
};
static_assert(std::is_trivially_copyable_v<BallColliderDesc>,
              "BallColliderDesc must be trivially copyable — it crosses FFI boundary");

struct CapsuleColliderDesc3D {
    f32 halfHeight = 0.5f;
    f32 radius = 0.25f;
    f32 density = 1.0f;
    bool sensor = false;
    CollisionGroup collisionGroup{};

    [[nodiscard]] constexpr bool operator==(const CapsuleColliderDesc3D&) const noexcept = default;
};
static_assert(std::is_trivially_copyable_v<CapsuleColliderDesc3D>,
              "CapsuleColliderDesc3D must be trivially copyable — it crosses FFI boundary");

// =============================================================================
// Pose structs — returned from Rapier queries, FFI-facing
// =============================================================================

struct PhysicsBodyPose2D {
    bool valid = false;
    Vector2 position{0.0f, 0.0f};
    f32 rotationRadians = 0.0f;

    [[nodiscard]] constexpr bool operator==(const PhysicsBodyPose2D& o) const noexcept {
        return valid == o.valid && position.x == o.position.x && position.y == o.position.y
            && rotationRadians == o.rotationRadians;
    }
};
static_assert(std::is_trivially_copyable_v<PhysicsBodyPose2D>,
              "PhysicsBodyPose2D must be trivially copyable — it crosses FFI boundary");

struct PhysicsBodyPose3D {
    bool valid = false;
    Vector3 position{0.0f, 0.0f, 0.0f};
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};

    [[nodiscard]] constexpr bool operator==(const PhysicsBodyPose3D& o) const noexcept {
        return valid == o.valid && position.x == o.position.x && position.y == o.position.y && position.z == o.position.z
            && rotation.x == o.rotation.x && rotation.y == o.rotation.y && rotation.z == o.rotation.z && rotation.w == o.rotation.w;
    }
};
static_assert(std::is_trivially_copyable_v<PhysicsBodyPose3D>,
              "PhysicsBodyPose3D must be trivially copyable — it crosses FFI boundary");

// =============================================================================
// Ray hit results — returned from Rapier raycast queries, FFI-facing
// =============================================================================

struct PhysicsRayHit2D {
    PhysicsCollider2D collider{};
    Vector2 point{0.0f, 0.0f};
    Vector2 normal{0.0f, 0.0f};
    f32 timeOfImpact = 0.0f;

    [[nodiscard]] constexpr bool operator==(const PhysicsRayHit2D& o) const noexcept {
        return collider == o.collider && point.x == o.point.x && point.y == o.point.y
            && normal.x == o.normal.x && normal.y == o.normal.y && timeOfImpact == o.timeOfImpact;
    }
};
static_assert(std::is_trivially_copyable_v<PhysicsRayHit2D>,
              "PhysicsRayHit2D must be trivially copyable — it crosses FFI boundary");
static_assert(sizeof(PhysicsRayHit2D) <= 40,
              "PhysicsRayHit2D must fit within 40 bytes for FFI stack copy");

struct PhysicsRayHit3D {
    PhysicsCollider3D collider{};
    Vector3 point{0.0f, 0.0f, 0.0f};
    Vector3 normal{0.0f, 0.0f, 0.0f};
    f32 timeOfImpact = 0.0f;

    [[nodiscard]] constexpr bool operator==(const PhysicsRayHit3D& o) const noexcept {
        return collider == o.collider && point.x == o.point.x && point.y == o.point.y && point.z == o.point.z
            && normal.x == o.normal.x && normal.y == o.normal.y && normal.z == o.normal.z && timeOfImpact == o.timeOfImpact;
    }
};
static_assert(std::is_trivially_copyable_v<PhysicsRayHit3D>,
              "PhysicsRayHit3D must be trivially copyable — it crosses FFI boundary");
static_assert(sizeof(PhysicsRayHit3D) <= 56,
              "PhysicsRayHit3D must fit within 56 bytes for FFI stack copy");

// =============================================================================
// Contact event — drained from Rapier after each step, FFI-facing
// =============================================================================

struct PhysicsContactEvent {
    PhysicsWorldKind world = PhysicsWorldKind::World2D;
    PhysicsContactPhase phase = PhysicsContactPhase::Started;
    u64 colliderA = 0U;
    u64 colliderB = 0U;

    [[nodiscard]] constexpr bool operator==(const PhysicsContactEvent&) const noexcept = default;
};
static_assert(std::is_trivially_copyable_v<PhysicsContactEvent>,
              "PhysicsContactEvent must be trivially copyable — it crosses FFI boundary");

// =============================================================================
// Compile-time verification bundle — all strong-type invariants
// =============================================================================

namespace detail {

// --- Verify every strong type is trivially copyable ---
static_assert(std::is_trivially_copyable_v<Mass>,              "Mass must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Density>,           "Density must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Friction>,          "Friction must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Restitution>,       "Restitution must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Force2D>,           "Force2D must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Force3D>,           "Force3D must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Impulse2D>,         "Impulse2D must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Impulse3D>,         "Impulse3D must be trivially copyable");
static_assert(std::is_trivially_copyable_v<AngularVelocity2D>, "AngularVelocity2D must be trivially copyable");
static_assert(std::is_trivially_copyable_v<AngularVelocity3D>, "AngularVelocity3D must be trivially copyable");

// --- Verify default values are sensible ---
static_assert(Mass{}.value == 0.0f,          "Default Mass must be zero");
static_assert(Density{}.value == 0.0f,       "Default Density must be zero");
static_assert(Friction{}.value == 0.5f,      "Default Friction must be 0.5");
static_assert(Restitution{}.value == 0.0f,   "Default Restitution must be zero");

// --- Verify explicit construction ---
static_assert(Mass{10.0f}.value == 10.0f,              "Mass explicit construction failed");
static_assert(Density{2.5f}.value == 2.5f,             "Density explicit construction failed");
static_assert(Friction{0.75f}.value == 0.75f,          "Friction explicit construction failed");
static_assert(Restitution{0.25f}.value == 0.25f,       "Restitution explicit construction failed");

// --- Verify Mass/Density clamp negative to zero ---
static_assert(Mass{-5.0f}.value == 0.0f,      "Negative Mass must clamp to zero");
static_assert(Density{-3.0f}.value == 0.0f,   "Negative Density must clamp to zero");

// --- Verify Friction/Restitution bounds clamping ---
static_assert(Friction{-100.0f}.value == 0.0f,   "Friction must clamp below 0.0");
static_assert(Friction{100.0f}.value == 1.0f,    "Friction must clamp above 1.0");
static_assert(Restitution{-100.0f}.value == 0.0f, "Restitution must clamp below 0.0");
static_assert(Restitution{100.0f}.value == 1.0f,  "Restitution must clamp above 1.0");

// --- Verify CollisionGroup logic ---
static_assert(CollisionGroup::all().mask == 0xFFFF, "CollisionGroup::all mask must be 0xFFFF");
static_assert(CollisionGroup::none().mask == 0,     "CollisionGroup::none mask must be 0");
static_assert(sizeof(CollisionGroup) == 4,          "CollisionGroup must pack to 4 bytes");

constexpr bool testCollisionGroupLogic = []() constexpr {
    constexpr CollisionGroup a{1, 0xFFFF};
    constexpr CollisionGroup b{2, 0xFFFF};
    constexpr CollisionGroup c{3, 0};
    return a.collidesWith(b) && b.collidesWith(a)
        && !a.collidesWith(c) && !c.collidesWith(a);
}();
static_assert(testCollisionGroupLogic, "CollisionGroup::collidesWith logic failed");

// --- Verify groupOnly() actually collides with its own group (regression test:
// groupOnly used to build {g, 0}, a mask that collides with nothing). group/mask
// are bitflags here, so the two "different" groups must use disjoint bits
// (0x0001 vs 0x0002) -- values like 5 and 6 share a bit and would collide under
// this bitwise scheme even though they're numerically distinct. ---
constexpr bool testCollisionGroupOnlyLogic = []() constexpr {
    constexpr CollisionGroup a = CollisionGroup::groupOnly(0x0001);
    constexpr CollisionGroup b = CollisionGroup::groupOnly(0x0001);
    constexpr CollisionGroup c = CollisionGroup::groupOnly(0x0002);
    return a.collidesWith(b) && b.collidesWith(a)
        && !a.collidesWith(c) && !c.collidesWith(a);
}();
static_assert(testCollisionGroupOnlyLogic, "CollisionGroup::groupOnly must collide with its own group");

// --- Verify SolverGroup logic ---
static_assert(SolverGroup::all().mask == 0xFFFF, "SolverGroup::all mask must be 0xFFFF");
static_assert(SolverGroup::none().mask == 0,     "SolverGroup::none mask must be 0");
static_assert(sizeof(SolverGroup) == 4,          "SolverGroup must pack to 4 bytes");

constexpr bool testSolverGroupLogic = []() constexpr {
    constexpr SolverGroup a{1, 0xFFFF};
    constexpr SolverGroup b{2, 0xFFFF};
    constexpr SolverGroup c{3, 0};
    return a.interactsWith(b) && b.interactsWith(a)
        && !a.interactsWith(c) && !c.interactsWith(a);
}();
static_assert(testSolverGroupLogic, "SolverGroup::interactsWith logic failed");

// --- Verify groupOnly() actually interacts with its own group (same regression
// class as CollisionGroup::groupOnly above; same disjoint-bit requirement) ---
constexpr bool testSolverGroupOnlyLogic = []() constexpr {
    constexpr SolverGroup a = SolverGroup::groupOnly(0x0001);
    constexpr SolverGroup b = SolverGroup::groupOnly(0x0001);
    constexpr SolverGroup c = SolverGroup::groupOnly(0x0002);
    return a.interactsWith(b) && b.interactsWith(a)
        && !a.interactsWith(c) && !c.interactsWith(a);
}();
static_assert(testSolverGroupOnlyLogic, "SolverGroup::groupOnly must interact with its own group");

// --- Verify handle types are aggregate-initializable from u64 literal ---
static_assert(PhysicsBody2D{42}.value == 42,          "PhysicsBody2D aggregate init failed");
static_assert(PhysicsCollider2D{99}.value == 99,      "PhysicsCollider2D aggregate init failed");
static_assert(PhysicsBody3D{7}.value == 7,            "PhysicsBody3D aggregate init failed");
static_assert(PhysicsCollider3D{13}.value == 13,      "PhysicsCollider3D aggregate init failed");

// --- Verify handle bool conversion ---
static_assert(!static_cast<bool>(PhysicsBody2D{0}),     "PhysicsBody2D{0} must evaluate to false");
static_assert(static_cast<bool>(PhysicsBody2D{1}),      "PhysicsBody2D{1} must evaluate to true");
static_assert(!static_cast<bool>(PhysicsCollider2D{0}), "PhysicsCollider2D{0} must evaluate to false");
static_assert(static_cast<bool>(PhysicsCollider2D{1}),  "PhysicsCollider2D{1} must evaluate to true");
static_assert(!static_cast<bool>(PhysicsBody3D{0}),     "PhysicsBody3D{0} must evaluate to false");
static_assert(static_cast<bool>(PhysicsBody3D{1}),      "PhysicsBody3D{1} must evaluate to true");
static_assert(!static_cast<bool>(PhysicsCollider3D{0}), "PhysicsCollider3D{0} must evaluate to false");
static_assert(static_cast<bool>(PhysicsCollider3D{1}),  "PhysicsCollider3D{1} must evaluate to true");

// --- Verify desc struct comparison ---
// (Skipped: raymath.h operator== for Vector2 is not constexpr on MSVC)

// --- Verify collider desc comparison ---
// (Skipped: raymath.h operator== for Vector2/3 is not constexpr on MSVC)
//
// --- Verify pose struct comparison ---
// (Skipped: raymath.h operator== for Vector2/3 is not constexpr on MSVC)
//
// --- Verify ray hit comparison ---
// (Skipped: raymath.h operator== for Vector2/3 is not constexpr on MSVC)
//
// --- Verify contact event comparison ---
static_assert(PhysicsContactEvent{} == PhysicsContactEvent{},
              "Default PhysicsContactEvent comparison must be reflexive");

} // namespace detail

} // namespace biofuel::engine::physics
