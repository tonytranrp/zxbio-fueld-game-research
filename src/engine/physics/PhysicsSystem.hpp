#pragma once

#include "engine/physics/PhysicsTypes.hpp"
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace biofuel::engine::physics {

// Forward-declare bridge world types for drainContacts() signature.
// The full bridge header is only pulled in by PhysicsSystem.cpp, keeping
// the Rust FFI types out of every translation unit that includes this header.
namespace rapier_bridge {
struct RapierWorld2D;
struct RapierWorld3D;
} // namespace rapier_bridge

class PhysicsSystem;

struct PhysicsIntegrationConfig {
    i32 solverIterations = 4;
    i32 maxCcdSubsteps = 1;
    f32 erp = 0.8f;

    [[nodiscard]] constexpr bool operator==(const PhysicsIntegrationConfig&) const noexcept = default;
};

class PhysicsWorld2D {
public:
    explicit PhysicsWorld2D(PhysicsSystem& system) noexcept;

    [[nodiscard]] PhysicsBody2D createBody(const PhysicsBodyDesc2D& desc) const;
    void removeBody(PhysicsBody2D body) const;
    [[nodiscard]] bool bodyExists(PhysicsBody2D body) const;
    [[nodiscard]] PhysicsBodyPose2D bodyPose(PhysicsBody2D body) const;
    [[nodiscard]] usize bodyPoses(std::span<const PhysicsBody2D> bodies, std::span<PhysicsBodyPose2D> poses) const;
    void setBodyPosition(PhysicsBody2D body, Vector2 position, f32 rotationRadians) const;
    void setBodyLinearVelocity(PhysicsBody2D body, Vector2 velocity) const;
    void setBodyType(PhysicsBody2D body, PhysicsBodyKind newType) const;

    void wakeBody(PhysicsBody2D body) const;
    void putBodyToSleep(PhysicsBody2D body) const;
    [[nodiscard]] std::optional<bool> isBodySleeping(PhysicsBody2D body) const;

    [[nodiscard]] PhysicsCollider2D attachBox(PhysicsBody2D body, const BoxColliderDesc2D& desc) const;
    [[nodiscard]] PhysicsCollider2D attachCircle(PhysicsBody2D body, const CircleColliderDesc& desc) const;
    [[nodiscard]] PhysicsCollider2D attachCapsule(PhysicsBody2D body, const CapsuleColliderDesc2D& desc) const;
    [[nodiscard]] bool colliderExists(PhysicsCollider2D collider) const;

    void setGravity(Vector2 gravity) const;
    [[nodiscard]] std::optional<PhysicsRayHit2D> raycast(Vector2 origin, Vector2 direction, f32 maxDistance, bool solid = true) const;

    // Joints (stub — not bridged to Rapier yet)
    [[nodiscard]] Joint2D createJoint(const JointDesc2D& desc) const;
    void removeJoint(Joint2D joint) const;
    [[nodiscard]] std::optional<bool> jointExists(Joint2D joint) const;

private:
    PhysicsSystem* m_system = nullptr;
};

class PhysicsWorld3D {
public:
    explicit PhysicsWorld3D(PhysicsSystem& system) noexcept;

    [[nodiscard]] PhysicsBody3D createBody(const PhysicsBodyDesc3D& desc) const;
    void removeBody(PhysicsBody3D body) const;
    [[nodiscard]] bool bodyExists(PhysicsBody3D body) const;
    [[nodiscard]] PhysicsBodyPose3D bodyPose(PhysicsBody3D body) const;
    [[nodiscard]] usize bodyPoses(std::span<const PhysicsBody3D> bodies, std::span<PhysicsBodyPose3D> poses) const;
    void setBodyPosition(PhysicsBody3D body, Vector3 position) const;
    void setBodyLinearVelocity(PhysicsBody3D body, Vector3 velocity) const;
    void setBodyType(PhysicsBody3D body, PhysicsBodyKind newType) const;

    void wakeBody(PhysicsBody3D body) const;
    void putBodyToSleep(PhysicsBody3D body) const;
    [[nodiscard]] std::optional<bool> isBodySleeping(PhysicsBody3D body) const;

    [[nodiscard]] PhysicsCollider3D attachCuboid(PhysicsBody3D body, const CuboidColliderDesc& desc) const;
    [[nodiscard]] PhysicsCollider3D attachBall(PhysicsBody3D body, const BallColliderDesc& desc) const;
    [[nodiscard]] PhysicsCollider3D attachCapsule(PhysicsBody3D body, const CapsuleColliderDesc3D& desc) const;
    [[nodiscard]] bool colliderExists(PhysicsCollider3D collider) const;

    void setGravity(Vector3 gravity) const;
    [[nodiscard]] std::optional<PhysicsRayHit3D> raycast(Vector3 origin, Vector3 direction, f32 maxDistance, bool solid = true) const;

    // Joints (stub — not bridged to Rapier yet)
    [[nodiscard]] Joint3D createJoint(const JointDesc3D& desc) const;
    void removeJoint(Joint3D joint) const;
    [[nodiscard]] std::optional<bool> jointExists(Joint3D joint) const;

private:
    PhysicsSystem* m_system = nullptr;
};

class PhysicsSystem {
public:
    PhysicsSystem();
    ~PhysicsSystem() noexcept;

    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    PhysicsSystem(PhysicsSystem&&) = delete;
    PhysicsSystem& operator=(PhysicsSystem&&) = delete;

    void init();
    void shutdown() noexcept;
    void stepFixed(f32 dt);

    [[nodiscard]] PhysicsWorld2D world2D() noexcept { return PhysicsWorld2D{*this}; }
    [[nodiscard]] PhysicsWorld3D world3D() noexcept { return PhysicsWorld3D{*this}; }
    [[nodiscard]] std::span<const PhysicsContactEvent> recentContacts() const noexcept { return m_contacts; }

    void setFixedTimestep(f32 dt) noexcept;
    [[nodiscard]] f32 fixedTimestep() const noexcept { return m_fixedTimestep; }

    void setMaxSubSteps(i32 n) noexcept;
    [[nodiscard]] i32 maxSubSteps() const noexcept { return m_maxSubSteps; }

    void setSolverIterations(i32 n) noexcept;
    [[nodiscard]] i32 solverIterations() const noexcept { return m_integrationConfig.solverIterations; }

    void setMaxCcdSubsteps(i32 n) noexcept;
    [[nodiscard]] i32 maxCcdSubsteps() const noexcept { return m_integrationConfig.maxCcdSubsteps; }

    void setErp(f32 erp) noexcept;
    [[nodiscard]] f32 erp() const noexcept { return m_integrationConfig.erp; }

    [[nodiscard]] const PhysicsIntegrationConfig& integrationConfig() const noexcept { return m_integrationConfig; }

    void registerColliderGroup(PhysicsWorldKind world, u64 colliderHandle, CollisionGroup group);
    void unregisterColliderGroup(PhysicsWorldKind world, u64 colliderHandle);

private:
    void trackBodyCollider(PhysicsWorldKind world, u64 bodyHandle, u64 colliderHandle);
    void purgeBodyColliderGroups(PhysicsWorldKind world, u64 bodyHandle);
    struct Impl;

    std::unique_ptr<Impl> m_impl;
    std::vector<PhysicsContactEvent> m_contacts;
    PhysicsIntegrationConfig m_integrationConfig{};
    f32 m_fixedTimestep = 1.0f / 60.0f;
    i32 m_maxSubSteps = 4;

    void ensureInitialized();
    void drainContacts(rapier_bridge::RapierWorld2D& world2D, rapier_bridge::RapierWorld3D& world3D);
    void publishContact(const PhysicsContactEvent& event) const;
    [[nodiscard]] bool passesCollisionFilter(PhysicsWorldKind world, u64 colliderA, u64 colliderB) const;

    friend class PhysicsWorld2D;
    friend class PhysicsWorld3D;
};

} // namespace biofuel::engine::physics
