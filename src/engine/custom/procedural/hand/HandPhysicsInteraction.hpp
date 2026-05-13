#pragma once

#include "engine/core/Types.hpp"
#include "engine/custom/procedural/hand/HandTypes.hpp"
#include "engine/custom/procedural/hand/TrackedRobotHand.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include <array>
#include <raylib.h>

namespace biofuel::engine::custom::procedural::hand {

struct HandPhysicsSceneDesc {
    f32 floorY = -0.37f;
    Vector3 gravity{0.0f, -4.8f, 0.0f};
    Vector3 cubeSpawn{0.0f, -0.070f, -0.180f};
    Vector3 cubeHalfExtents{0.075f, 0.075f, 0.075f};
    Vector3 groundHalfExtents{2.20f, 0.035f, 2.20f};
    Vector3 touchShelfCenter{0.0f, -0.170f, -0.180f};
    Vector3 touchShelfHalfExtents{0.34f, 0.025f, 0.25f};
    Vector3 interactionCenter{0.0f, 0.03f, -0.180f};
    Vector3 interactionHalfExtents{0.58f, 0.47f, 0.42f};
    f32 palmColliderRadius = 0.060f;
    f32 fingertipColliderRadius = 0.033f;
    f32 closedPinchDistance = 0.155f;
    f32 openedPinchDistance = 0.225f;
    f32 grabRadius = 0.175f;
    f32 releaseRadius = 0.430f;
    f32 grabDepthWeight = 0.28f;
    f32 maxGrabSpeed = 3.35f;
    f32 grabFollowResponse = 18.0f;
    f32 releaseVelocityScale = 0.62f;
    f32 cubeRecoverySpeed = 1.55f;
    f32 cubeHardResetDistance = 1.15f;
};

struct HandPhysicsSceneState {
    bool initialized = false;
    bool cubeValid = false;
    bool touchShelfValid = false;
    bool grabbed = false;
    HandSide grabbedBy = HandSide::Left;
    Vector3 cubeCenter{0.0f, 0.0f, 0.0f};
    Vector3 cubeHalfExtents{0.105f, 0.105f, 0.105f};
    Vector3 touchShelfCenter{0.0f, 0.0f, 0.0f};
    Vector3 touchShelfHalfExtents{0.0f, 0.0f, 0.0f};
    Vector3 grabPoint{0.0f, 0.0f, 0.0f};
    f32 leftPinchDistance = 0.0f;
    f32 rightPinchDistance = 0.0f;
};

class HandPhysicsInteraction3D final {
public:
    HandPhysicsInteraction3D() = default;
    HandPhysicsInteraction3D(const HandPhysicsInteraction3D&) = delete;
    HandPhysicsInteraction3D& operator=(const HandPhysicsInteraction3D&) = delete;
    HandPhysicsInteraction3D(HandPhysicsInteraction3D&&) = delete;
    HandPhysicsInteraction3D& operator=(HandPhysicsInteraction3D&&) = delete;

    void init(::biofuel::engine::physics::PhysicsWorld3D world, const HandPhysicsSceneDesc& desc = {});
    void shutdown(::biofuel::engine::physics::PhysicsWorld3D world) noexcept;
    void resetCube(::biofuel::engine::physics::PhysicsWorld3D world) noexcept;
    void update(
        ::biofuel::engine::physics::PhysicsWorld3D world,
        const TrackedRobotHandPose* left,
        const TrackedRobotHandPose* right,
        f32 dt) noexcept;

    [[nodiscard]] const HandPhysicsSceneState& state() const noexcept { return m_state; }
    [[nodiscard]] bool initialized() const noexcept { return m_state.initialized; }

private:
    struct ShapeRef {
        ::biofuel::engine::physics::PhysicsBody3D body{};
        ::biofuel::engine::physics::PhysicsCollider3D collider{};
        ::biofuel::engine::physics::PhysicsShapeRole role =
            ::biofuel::engine::physics::PhysicsShapeRole::Unknown;
    };

    struct HandInteractor {
        ShapeRef palm{};
        std::array<ShapeRef, 5U> fingertips{};
        Vector3 previousPinchPoint{0.0f, 0.0f, 0.0f};
        bool hasPreviousPinch = false;
    };

    ShapeRef m_ground{};
    ShapeRef m_touchShelf{};
    ShapeRef m_cube{};
    HandInteractor m_left{};
    HandInteractor m_right{};
    HandPhysicsSceneDesc m_desc{};
    HandPhysicsSceneState m_state{};
    Vector3 m_grabOffset{0.0f, 0.0f, 0.0f};
    Vector3 m_previousCubeCenter{0.0f, 0.0f, 0.0f};
    bool m_hasPreviousCube = false;

    [[nodiscard]] static Vector3 palmCenter(const TrackedRobotHandPose& pose) noexcept;
    [[nodiscard]] static Vector3 pinchPoint(const TrackedRobotHandPose& pose) noexcept;
    [[nodiscard]] static f32 pinchDistance(const TrackedRobotHandPose& pose) noexcept;
    [[nodiscard]] static f32 pointToBoxDistance(Vector3 point, Vector3 center, Vector3 halfExtents, f32 depthWeight) noexcept;
    [[nodiscard]] static f32 grabContactDistance(
        const TrackedRobotHandPose& pose,
        Vector3 center,
        Vector3 halfExtents,
        f32 depthWeight) noexcept;
    [[nodiscard]] static Vector3 limitedVelocity(Vector3 value, f32 maxSpeed) noexcept;
    [[nodiscard]] Vector3 clampToInteractionVolume(Vector3 value) const noexcept;
    [[nodiscard]] bool outsideInteractionVolume(Vector3 value, f32 padding) const noexcept;

    [[nodiscard]] ShapeRef createCuboid(
        ::biofuel::engine::physics::PhysicsWorld3D world,
        ::biofuel::engine::physics::PhysicsBodyKind bodyKind,
        Vector3 position,
        Vector3 halfExtents,
        ::biofuel::engine::physics::PhysicsShapeRole role) const noexcept;
    [[nodiscard]] ShapeRef createBall(
        ::biofuel::engine::physics::PhysicsWorld3D world,
        Vector3 position,
        f32 radius,
        ::biofuel::engine::physics::PhysicsShapeRole role) const noexcept;
    void createScene(::biofuel::engine::physics::PhysicsWorld3D world);
    void createHand(::biofuel::engine::physics::PhysicsWorld3D world, HandInteractor& hand);
    void destroyShape(::biofuel::engine::physics::PhysicsWorld3D world, ShapeRef& shape) noexcept;
    void rememberPinch(HandInteractor& hand, const TrackedRobotHandPose* pose) const noexcept;
    void syncHand(
        ::biofuel::engine::physics::PhysicsWorld3D world,
        HandInteractor& hand,
        const TrackedRobotHandPose* pose) noexcept;
    void updateCubeState(::biofuel::engine::physics::PhysicsWorld3D world) noexcept;
    void recoverCube(
        ::biofuel::engine::physics::PhysicsWorld3D world,
        const TrackedRobotHandPose* left,
        const TrackedRobotHandPose* right,
        f32 dt) noexcept;
    void updateGrab(
        ::biofuel::engine::physics::PhysicsWorld3D world,
        const TrackedRobotHandPose* left,
        const TrackedRobotHandPose* right,
        f32 dt) noexcept;
    void beginGrab(
        ::biofuel::engine::physics::PhysicsWorld3D world,
        HandSide side,
        const TrackedRobotHandPose& pose) noexcept;
    void releaseGrab(
        ::biofuel::engine::physics::PhysicsWorld3D world,
        Vector3 releaseVelocity) noexcept;
    void publishCreated(const ShapeRef& shape) const;
    void publishDestroyed(const ShapeRef& shape) const;
};

} // namespace biofuel::engine::custom::procedural::hand
