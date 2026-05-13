#pragma once

#include "engine/physics/PhysicsTypes.hpp"
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace biofuel::engine::physics {

class PhysicsSystem;

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

    [[nodiscard]] PhysicsCollider2D attachBox(PhysicsBody2D body, const BoxColliderDesc2D& desc) const;
    [[nodiscard]] PhysicsCollider2D attachCircle(PhysicsBody2D body, const CircleColliderDesc& desc) const;
    [[nodiscard]] PhysicsCollider2D attachCapsule(PhysicsBody2D body, const CapsuleColliderDesc2D& desc) const;
    [[nodiscard]] bool colliderExists(PhysicsCollider2D collider) const;

    void setGravity(Vector2 gravity) const;
    [[nodiscard]] std::optional<PhysicsRayHit2D> raycast(Vector2 origin, Vector2 direction, f32 maxDistance, bool solid = true) const;

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

    [[nodiscard]] PhysicsCollider3D attachCuboid(PhysicsBody3D body, const CuboidColliderDesc& desc) const;
    [[nodiscard]] PhysicsCollider3D attachBall(PhysicsBody3D body, const BallColliderDesc& desc) const;
    [[nodiscard]] PhysicsCollider3D attachCapsule(PhysicsBody3D body, const CapsuleColliderDesc3D& desc) const;
    [[nodiscard]] bool colliderExists(PhysicsCollider3D collider) const;

    void setGravity(Vector3 gravity) const;
    [[nodiscard]] std::optional<PhysicsRayHit3D> raycast(Vector3 origin, Vector3 direction, f32 maxDistance, bool solid = true) const;

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

private:
    struct Impl;

    std::unique_ptr<Impl> m_impl;
    std::vector<PhysicsContactEvent> m_contacts;

    void ensureInitialized();
    void drainContacts();
    void publishContact(const PhysicsContactEvent& event) const;

    friend class PhysicsWorld2D;
    friend class PhysicsWorld3D;
};

} // namespace biofuel::engine::physics
