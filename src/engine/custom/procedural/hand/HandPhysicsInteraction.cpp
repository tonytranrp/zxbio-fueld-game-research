#include "engine/custom/procedural/hand/HandPhysicsInteraction.hpp"

#include "engine/events/physics/PhysicsEventModule.hpp"
#include "engine/runtime/typed/Events.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::hand {

namespace {

using ::biofuel::engine::physics::BallColliderDesc;
using ::biofuel::engine::physics::CuboidColliderDesc;
using ::biofuel::engine::physics::PhysicsBody3D;
using ::biofuel::engine::physics::PhysicsBodyDesc3D;
using ::biofuel::engine::physics::PhysicsBodyKind;
using ::biofuel::engine::physics::PhysicsShapeRole;
using ::biofuel::engine::physics::PhysicsWorld3D;
using ::biofuel::engine::physics::PhysicsWorldKind;

constexpr std::array<usize, 5U> FingertipLandmarks{{4U, 8U, 12U, 16U, 20U}};
constexpr std::array<usize, 5U> PalmLandmarks{{0U, 5U, 9U, 13U, 17U}};

[[nodiscard]] Vector3 hiddenHandPosition(const HandPhysicsSceneDesc& desc) noexcept {
    return Vector3{0.0f, desc.floorY - 8.0f, 0.0f};
}

[[nodiscard]] bool finite(const Vector3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] f32 safeDt(const f32 dt) noexcept {
    return std::clamp(dt, 1.0f / 240.0f, 0.05f);
}

} // namespace

void HandPhysicsInteraction3D::init(const PhysicsWorld3D world, const HandPhysicsSceneDesc& desc) {
    if (m_state.initialized) {
        return;
    }

    m_desc = desc;
    world.setGravity(m_desc.gravity);
    createScene(world);
    m_state.initialized = true;
    updateCubeState(world);
}

void HandPhysicsInteraction3D::shutdown(const PhysicsWorld3D world) noexcept {
    if (!m_state.initialized) {
        return;
    }

    if (m_state.grabbed) {
        releaseGrab(world, Vector3{0.0f, 0.0f, 0.0f});
    }

    for (ShapeRef& fingertip : m_left.fingertips) {
        destroyShape(world, fingertip);
    }
    destroyShape(world, m_left.palm);

    for (ShapeRef& fingertip : m_right.fingertips) {
        destroyShape(world, fingertip);
    }
    destroyShape(world, m_right.palm);

    destroyShape(world, m_cube);
    destroyShape(world, m_touchShelf);
    destroyShape(world, m_ground);
    world.setGravity(Vector3{0.0f, 0.0f, 0.0f});

    m_left = HandInteractor{};
    m_right = HandInteractor{};
    m_grabOffset = {};
    m_previousCubeCenter = {};
    m_hasPreviousCube = false;
    m_state = HandPhysicsSceneState{};
}

void HandPhysicsInteraction3D::resetCube(const PhysicsWorld3D world) noexcept {
    if (!m_cube.body) {
        return;
    }
    if (m_state.grabbed) {
        releaseGrab(world, Vector3{0.0f, 0.0f, 0.0f});
    }
    world.setBodyPosition(m_cube.body, m_desc.cubeSpawn);
    world.setBodyLinearVelocity(m_cube.body, Vector3{0.0f, 0.0f, 0.0f});
    m_previousCubeCenter = m_desc.cubeSpawn;
    m_hasPreviousCube = true;
    updateCubeState(world);
}

void HandPhysicsInteraction3D::update(
    const PhysicsWorld3D world,
    const TrackedRobotHandPose* left,
    const TrackedRobotHandPose* right,
    const f32 dt) noexcept
{
    if (!m_state.initialized) {
        init(world, m_desc);
    }

    syncHand(world, m_left, left);
    syncHand(world, m_right, right);

    m_state.leftPinchDistance = left != nullptr && left->valid ? pinchDistance(*left) : 0.0f;
    m_state.rightPinchDistance = right != nullptr && right->valid ? pinchDistance(*right) : 0.0f;
    updateCubeState(world);

    if (m_state.cubeValid && m_state.cubeCenter.y < m_desc.floorY - 1.0f) {
        resetCube(world);
    }

    recoverCube(world, left, right, dt);
    updateGrab(world, left, right, dt);
    rememberPinch(m_left, left);
    rememberPinch(m_right, right);

    if (m_state.cubeValid) {
        m_previousCubeCenter = m_state.cubeCenter;
        m_hasPreviousCube = true;
    }
}

Vector3 HandPhysicsInteraction3D::palmCenter(const TrackedRobotHandPose& pose) noexcept {
    Vector3 sum{0.0f, 0.0f, 0.0f};
    for (const usize index : PalmLandmarks) {
        sum = Vector3Add(sum, pose.landmarks[index]);
    }
    return Vector3Scale(sum, 1.0f / static_cast<f32>(PalmLandmarks.size()));
}

Vector3 HandPhysicsInteraction3D::pinchPoint(const TrackedRobotHandPose& pose) noexcept {
    return Vector3Scale(Vector3Add(pose.landmarks[4U], pose.landmarks[8U]), 0.5f);
}

f32 HandPhysicsInteraction3D::pinchDistance(const TrackedRobotHandPose& pose) noexcept {
    return Vector3Distance(pose.landmarks[4U], pose.landmarks[8U]);
}

f32 HandPhysicsInteraction3D::pointToBoxDistance(
    const Vector3 point,
    const Vector3 center,
    const Vector3 halfExtents,
    const f32 depthWeight) noexcept
{
    const Vector3 delta{
        std::max(std::fabs(point.x - center.x) - halfExtents.x, 0.0f),
        std::max(std::fabs(point.y - center.y) - halfExtents.y, 0.0f),
        std::max(std::fabs(point.z - center.z) - halfExtents.z, 0.0f) * std::clamp(depthWeight, 0.0f, 1.0f),
    };
    return Vector3Length(delta);
}

f32 HandPhysicsInteraction3D::grabContactDistance(
    const TrackedRobotHandPose& pose,
    const Vector3 center,
    const Vector3 halfExtents,
    const f32 depthWeight) noexcept
{
    f32 best = pointToBoxDistance(pinchPoint(pose), center, halfExtents, depthWeight);
    best = std::min(best, pointToBoxDistance(pose.landmarks[4U], center, halfExtents, depthWeight));
    best = std::min(best, pointToBoxDistance(pose.landmarks[8U], center, halfExtents, depthWeight));
    best = std::min(best, pointToBoxDistance(palmCenter(pose), center, halfExtents, depthWeight) * 1.18f);
    return best;
}

Vector3 HandPhysicsInteraction3D::limitedVelocity(const Vector3 value, const f32 maxSpeed) noexcept {
    const f32 length = Vector3Length(value);
    if (length <= maxSpeed || length <= 0.0001f) {
        return value;
    }
    return Vector3Scale(value, maxSpeed / length);
}

Vector3 HandPhysicsInteraction3D::clampToInteractionVolume(const Vector3 value) const noexcept {
    const Vector3 min{
        m_desc.interactionCenter.x - m_desc.interactionHalfExtents.x + m_desc.cubeHalfExtents.x,
        std::max(
            m_desc.floorY + m_desc.cubeHalfExtents.y,
            m_desc.interactionCenter.y - m_desc.interactionHalfExtents.y + m_desc.cubeHalfExtents.y),
        m_desc.interactionCenter.z - m_desc.interactionHalfExtents.z + m_desc.cubeHalfExtents.z,
    };
    const Vector3 max{
        m_desc.interactionCenter.x + m_desc.interactionHalfExtents.x - m_desc.cubeHalfExtents.x,
        m_desc.interactionCenter.y + m_desc.interactionHalfExtents.y - m_desc.cubeHalfExtents.y,
        m_desc.interactionCenter.z + m_desc.interactionHalfExtents.z - m_desc.cubeHalfExtents.z,
    };
    return Vector3{
        std::clamp(value.x, min.x, std::max(min.x, max.x)),
        std::clamp(value.y, min.y, std::max(min.y, max.y)),
        std::clamp(value.z, min.z, std::max(min.z, max.z)),
    };
}

bool HandPhysicsInteraction3D::outsideInteractionVolume(const Vector3 value, const f32 padding) const noexcept {
    const Vector3 min{
        m_desc.interactionCenter.x - m_desc.interactionHalfExtents.x - padding,
        m_desc.floorY - padding,
        m_desc.interactionCenter.z - m_desc.interactionHalfExtents.z - padding,
    };
    const Vector3 max{
        m_desc.interactionCenter.x + m_desc.interactionHalfExtents.x + padding,
        m_desc.interactionCenter.y + m_desc.interactionHalfExtents.y + padding,
        m_desc.interactionCenter.z + m_desc.interactionHalfExtents.z + padding,
    };
    return value.x < min.x || value.x > max.x
        || value.y < min.y || value.y > max.y
        || value.z < min.z || value.z > max.z;
}

HandPhysicsInteraction3D::ShapeRef HandPhysicsInteraction3D::createCuboid(
    const PhysicsWorld3D world,
    const PhysicsBodyKind bodyKind,
    const Vector3 position,
    const Vector3 halfExtents,
    const PhysicsShapeRole role) const noexcept
{
    ShapeRef shape{.role = role};
    shape.body = world.createBody(PhysicsBodyDesc3D{
        .kind = bodyKind,
        .position = position,
        .linearVelocity = Vector3{0.0f, 0.0f, 0.0f},
        .canSleep = bodyKind == PhysicsBodyKind::Dynamic,
    });
    shape.collider = world.attachCuboid(shape.body, CuboidColliderDesc{
        .halfExtents = halfExtents,
        .density = bodyKind == PhysicsBodyKind::Dynamic ? 1.0f : 0.0f,
        .sensor = false,
    });
    return shape;
}

HandPhysicsInteraction3D::ShapeRef HandPhysicsInteraction3D::createBall(
    const PhysicsWorld3D world,
    const Vector3 position,
    const f32 radius,
    const PhysicsShapeRole role) const noexcept
{
    ShapeRef shape{.role = role};
    shape.body = world.createBody(PhysicsBodyDesc3D{
        .kind = PhysicsBodyKind::KinematicPosition,
        .position = position,
        .linearVelocity = Vector3{0.0f, 0.0f, 0.0f},
        .canSleep = false,
    });
    shape.collider = world.attachBall(shape.body, BallColliderDesc{
        .radius = radius,
        .density = 0.0f,
        .sensor = false,
    });
    return shape;
}

void HandPhysicsInteraction3D::createScene(const PhysicsWorld3D world) {
    const Vector3 groundCenter{
        0.0f,
        m_desc.floorY - m_desc.groundHalfExtents.y,
        0.0f,
    };
    m_ground = createCuboid(
        world,
        PhysicsBodyKind::Fixed,
        groundCenter,
        m_desc.groundHalfExtents,
        PhysicsShapeRole::StaticScene);
    publishCreated(m_ground);

    m_touchShelf = createCuboid(
        world,
        PhysicsBodyKind::Fixed,
        m_desc.touchShelfCenter,
        m_desc.touchShelfHalfExtents,
        PhysicsShapeRole::StaticScene);
    publishCreated(m_touchShelf);

    m_cube = createCuboid(
        world,
        PhysicsBodyKind::Dynamic,
        m_desc.cubeSpawn,
        m_desc.cubeHalfExtents,
        PhysicsShapeRole::DynamicProp);
    publishCreated(m_cube);

    createHand(world, m_left);
    createHand(world, m_right);
    m_state.cubeHalfExtents = m_desc.cubeHalfExtents;
    m_state.touchShelfValid = true;
    m_state.touchShelfCenter = m_desc.touchShelfCenter;
    m_state.touchShelfHalfExtents = m_desc.touchShelfHalfExtents;
}

void HandPhysicsInteraction3D::createHand(const PhysicsWorld3D world, HandInteractor& hand) {
    const Vector3 hidden = hiddenHandPosition(m_desc);
    hand.palm = createBall(
        world,
        hidden,
        m_desc.palmColliderRadius,
        PhysicsShapeRole::KinematicInteractor);
    publishCreated(hand.palm);

    for (ShapeRef& fingertip : hand.fingertips) {
        fingertip = createBall(
            world,
            hidden,
            m_desc.fingertipColliderRadius,
            PhysicsShapeRole::KinematicInteractor);
        publishCreated(fingertip);
    }
}

void HandPhysicsInteraction3D::destroyShape(const PhysicsWorld3D world, ShapeRef& shape) noexcept {
    if (!shape.body) {
        shape = ShapeRef{};
        return;
    }

    publishDestroyed(shape);
    world.removeBody(shape.body);
    shape = ShapeRef{};
}

void HandPhysicsInteraction3D::rememberPinch(HandInteractor& hand, const TrackedRobotHandPose* pose) const noexcept {
    if (pose == nullptr || !pose->valid) {
        hand.hasPreviousPinch = false;
        return;
    }
    hand.previousPinchPoint = pinchPoint(*pose);
    hand.hasPreviousPinch = true;
}

void HandPhysicsInteraction3D::syncHand(
    const PhysicsWorld3D world,
    HandInteractor& hand,
    const TrackedRobotHandPose* pose) noexcept
{
    if (pose == nullptr || !pose->valid) {
        const Vector3 hidden = hiddenHandPosition(m_desc);
        if (hand.palm.body) {
            world.setBodyPosition(hand.palm.body, hidden);
        }
        for (ShapeRef& fingertip : hand.fingertips) {
            if (fingertip.body) {
                world.setBodyPosition(fingertip.body, hidden);
            }
        }
        return;
    }

    const Vector3 palm = palmCenter(*pose);
    if (finite(palm) && hand.palm.body) {
        world.setBodyPosition(hand.palm.body, palm);
    }

    for (usize index = 0U; index < hand.fingertips.size(); ++index) {
        const Vector3 tip = pose->landmarks[FingertipLandmarks[index]];
        if (finite(tip) && hand.fingertips[index].body) {
            world.setBodyPosition(hand.fingertips[index].body, tip);
        }
    }
}

void HandPhysicsInteraction3D::updateCubeState(const PhysicsWorld3D world) noexcept {
    m_state.cubeValid = false;
    if (!m_cube.body) {
        return;
    }

    const auto pose = world.bodyPose(m_cube.body);
    if (!pose.valid || !finite(pose.position)) {
        return;
    }

    m_state.cubeValid = true;
    m_state.cubeCenter = pose.position;
    m_state.cubeHalfExtents = m_desc.cubeHalfExtents;
}

void HandPhysicsInteraction3D::recoverCube(
    const PhysicsWorld3D world,
    const TrackedRobotHandPose* left,
    const TrackedRobotHandPose* right,
    const f32 dt) noexcept
{
    if (!m_state.cubeValid || m_state.grabbed) {
        return;
    }

    const bool handVisible = (left != nullptr && left->valid) || (right != nullptr && right->valid);
    const f32 padding = handVisible ? 0.060f : 0.135f;
    if (!outsideInteractionVolume(m_state.cubeCenter, padding)) {
        return;
    }

    const Vector3 target = clampToInteractionVolume(m_state.cubeCenter);
    const Vector3 delta = Vector3Subtract(target, m_state.cubeCenter);
    const f32 distance = Vector3Length(delta);
    if (distance >= m_desc.cubeHardResetDistance || !finite(target)) {
        world.setBodyPosition(m_cube.body, m_desc.cubeSpawn);
        world.setBodyLinearVelocity(m_cube.body, Vector3{0.0f, 0.0f, 0.0f});
        m_state.cubeCenter = m_desc.cubeSpawn;
        return;
    }

    const f32 frameDt = safeDt(dt);
    const Vector3 velocity = limitedVelocity(
        Vector3Scale(delta, 1.0f / frameDt),
        std::max(m_desc.cubeRecoverySpeed, 0.01f));
    world.setBodyLinearVelocity(m_cube.body, velocity);
}

void HandPhysicsInteraction3D::updateGrab(
    const PhysicsWorld3D world,
    const TrackedRobotHandPose* left,
    const TrackedRobotHandPose* right,
    const f32 dt) noexcept
{
    if (!m_state.cubeValid) {
        return;
    }

    const f32 frameDt = safeDt(dt);
    if (m_state.grabbed) {
        const bool leftGrab = m_state.grabbedBy == HandSide::Left;
        const TrackedRobotHandPose* pose = leftGrab ? left : right;
        const HandInteractor& interactor = leftGrab ? m_left : m_right;
        if (pose == nullptr || !pose->valid) {
            releaseGrab(world, Vector3{0.0f, 0.0f, 0.0f});
            return;
        }

        const Vector3 pinch = pinchPoint(*pose);
        const f32 opening = pinchDistance(*pose);
        const f32 cubeDistance = grabContactDistance(
            *pose,
            m_state.cubeCenter,
            m_desc.cubeHalfExtents,
            m_desc.grabDepthWeight);
        Vector3 releaseVelocity{0.0f, 0.0f, 0.0f};
        if (interactor.hasPreviousPinch) {
            releaseVelocity = limitedVelocity(
                Vector3Scale(
                    Vector3Subtract(pinch, interactor.previousPinchPoint),
                    m_desc.releaseVelocityScale / frameDt),
                m_desc.maxGrabSpeed * 1.35f);
        }

        if (opening >= m_desc.openedPinchDistance || cubeDistance >= m_desc.releaseRadius) {
            releaseGrab(world, releaseVelocity);
            return;
        }

        Vector3 target = clampToInteractionVolume(Vector3Add(pinch, m_grabOffset));
        target.y = std::max(target.y, m_desc.floorY + m_desc.cubeHalfExtents.y);
        const Vector3 velocity = limitedVelocity(
            Vector3Scale(
                Vector3Subtract(target, m_state.cubeCenter),
                std::max(m_desc.grabFollowResponse, 0.01f)),
            m_desc.maxGrabSpeed);
        Vector3 next = Vector3Add(m_state.cubeCenter, Vector3Scale(velocity, frameDt));
        if (Vector3Distance(next, target) <= m_desc.maxGrabSpeed * frameDt * 0.35f) {
            next = target;
        }
        next = clampToInteractionVolume(next);
        world.setBodyPosition(m_cube.body, next);
        world.setBodyLinearVelocity(m_cube.body, velocity);
        m_state.cubeCenter = next;
        m_state.grabPoint = pinch;
        return;
    }

    struct Candidate {
        const TrackedRobotHandPose* pose = nullptr;
        HandSide side = HandSide::Left;
        f32 distance = std::numeric_limits<f32>::max();
    };
    Candidate best{};

    const auto consider = [this, &best](const TrackedRobotHandPose* pose, const HandSide side) noexcept {
        if (pose == nullptr || !pose->valid || pinchDistance(*pose) > m_desc.closedPinchDistance) {
            return;
        }
        const f32 distance = grabContactDistance(
            *pose,
            m_state.cubeCenter,
            m_desc.cubeHalfExtents,
            m_desc.grabDepthWeight);
        if (distance <= m_desc.grabRadius && distance < best.distance) {
            best = Candidate{.pose = pose, .side = side, .distance = distance};
        }
    };

    consider(left, HandSide::Left);
    consider(right, HandSide::Right);
    if (best.pose != nullptr) {
        beginGrab(world, best.side, *best.pose);
    }
}

void HandPhysicsInteraction3D::beginGrab(
    const PhysicsWorld3D world,
    const HandSide side,
    const TrackedRobotHandPose& pose) noexcept
{
    const Vector3 pinch = pinchPoint(pose);
    m_state.grabbed = true;
    m_state.grabbedBy = side;
    m_state.grabPoint = pinch;
    m_grabOffset = Vector3Subtract(m_state.cubeCenter, pinch);
    const f32 offsetLength = Vector3Length(m_grabOffset);
    if (offsetLength > m_desc.cubeHalfExtents.x * 1.35f && offsetLength > 0.0001f) {
        m_grabOffset = Vector3Scale(m_grabOffset, (m_desc.cubeHalfExtents.x * 1.35f) / offsetLength);
    }
    world.setBodyLinearVelocity(m_cube.body, Vector3{0.0f, 0.0f, 0.0f});

    const ShapeRef& grabber = side == HandSide::Left ? m_left.palm : m_right.palm;
    ::biofuel::engine::runtime::typed::Events::publish<
        ::biofuel::engine::runtime::typed::physics::ShapeGrabStarted>({
            .world = PhysicsWorldKind::World3D,
            .shapeBody = m_cube.body.value,
            .shapeCollider = m_cube.collider.value,
            .grabberBody = grabber.body.value,
            .point = pinch,
        });
}

void HandPhysicsInteraction3D::releaseGrab(const PhysicsWorld3D world, const Vector3 releaseVelocity) noexcept {
    if (!m_state.grabbed) {
        return;
    }

    const ShapeRef& grabber = m_state.grabbedBy == HandSide::Left ? m_left.palm : m_right.palm;
    const Vector3 safeVelocity = limitedVelocity(releaseVelocity, m_desc.maxGrabSpeed * 1.35f);
    world.setBodyLinearVelocity(m_cube.body, safeVelocity);
    ::biofuel::engine::runtime::typed::Events::publish<
        ::biofuel::engine::runtime::typed::physics::ShapeGrabEnded>({
            .world = PhysicsWorldKind::World3D,
            .shapeBody = m_cube.body.value,
            .shapeCollider = m_cube.collider.value,
            .grabberBody = grabber.body.value,
            .point = m_state.grabPoint,
            .releaseVelocity = safeVelocity,
        });

    m_state.grabbed = false;
    m_grabOffset = Vector3{0.0f, 0.0f, 0.0f};
}

void HandPhysicsInteraction3D::publishCreated(const ShapeRef& shape) const {
    if (!shape.body && !shape.collider) {
        return;
    }
    ::biofuel::engine::runtime::typed::Events::publish<
        ::biofuel::engine::runtime::typed::physics::ShapeCreated>({
            .world = PhysicsWorldKind::World3D,
            .role = shape.role,
            .body = shape.body.value,
            .collider = shape.collider.value,
        });
}

void HandPhysicsInteraction3D::publishDestroyed(const ShapeRef& shape) const {
    if (!shape.body && !shape.collider) {
        return;
    }
    ::biofuel::engine::runtime::typed::Events::publish<
        ::biofuel::engine::runtime::typed::physics::ShapeDestroyed>({
            .world = PhysicsWorldKind::World3D,
            .role = shape.role,
            .body = shape.body.value,
            .collider = shape.collider.value,
        });
}

} // namespace biofuel::engine::custom::procedural::hand
