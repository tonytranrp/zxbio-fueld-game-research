#include "engine/physics/PhysicsSystem.hpp"

#include "biofuel_rapier_bridge_cxx/lib.h"
#include "engine/events/physics/PhysicsEventModule.hpp"
#include "engine/runtime/typed/Events.hpp"
#include <algorithm>
#include <vector>

namespace biofuel::engine::physics {

namespace bridge = ::biofuel::engine::physics::rapier_bridge;

namespace {

[[nodiscard]] constexpr u8 toBridgeKind(const PhysicsBodyKind kind) noexcept {
    switch (kind) {
    case PhysicsBodyKind::Fixed: return 0U;
    case PhysicsBodyKind::Dynamic: return 1U;
    case PhysicsBodyKind::KinematicPosition: return 2U;
    case PhysicsBodyKind::KinematicVelocity: return 3U;
    }
    return 0U;
}

[[nodiscard]] constexpr PhysicsContactPhase contactPhaseFromBridge(const u8 phase) noexcept {
    return phase == 1U ? PhysicsContactPhase::Ended : PhysicsContactPhase::Started;
}

[[nodiscard]] constexpr bridge::BridgeVec2 toBridge(const Vector2 value) noexcept {
    return bridge::BridgeVec2{.x = value.x, .y = value.y};
}

[[nodiscard]] constexpr bridge::BridgeVec3 toBridge(const Vector3 value) noexcept {
    return bridge::BridgeVec3{.x = value.x, .y = value.y, .z = value.z};
}

[[nodiscard]] constexpr Vector2 fromBridge(const bridge::BridgeVec2 value) noexcept {
    return Vector2{value.x, value.y};
}

[[nodiscard]] constexpr Vector3 fromBridge(const bridge::BridgeVec3 value) noexcept {
    return Vector3{value.x, value.y, value.z};
}

[[nodiscard]] constexpr Quaternion fromBridge(const bridge::BridgeQuat value) noexcept {
    return Quaternion{value.x, value.y, value.z, value.w};
}

} // namespace

struct PhysicsSystem::Impl {
    rust::Box<bridge::RapierWorld2D> world2D = bridge::new_world_2d();
    rust::Box<bridge::RapierWorld3D> world3D = bridge::new_world_3d();
};

PhysicsWorld2D::PhysicsWorld2D(PhysicsSystem& system) noexcept
    : m_system(&system)
{
}

PhysicsBody2D PhysicsWorld2D::createBody(const PhysicsBodyDesc2D& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeBodyDesc2D bridgeDesc{
        .kind = toBridgeKind(desc.kind),
        .position = toBridge(desc.position),
        .linear_velocity = toBridge(desc.linearVelocity),
        .rotation_radians = desc.rotationRadians,
        .angular_velocity = desc.angularVelocity,
        .can_sleep = desc.canSleep,
    };
    return PhysicsBody2D{bridge::create_body_2d(*m_system->m_impl->world2D, bridgeDesc)};
}

void PhysicsWorld2D::removeBody(const PhysicsBody2D body) const {
    m_system->ensureInitialized();
    bridge::remove_body_2d(*m_system->m_impl->world2D, body.value);
}

bool PhysicsWorld2D::bodyExists(const PhysicsBody2D body) const {
    m_system->ensureInitialized();
    return bridge::body_exists_2d(*m_system->m_impl->world2D, body.value);
}

PhysicsBodyPose2D PhysicsWorld2D::bodyPose(const PhysicsBody2D body) const {
    m_system->ensureInitialized();
    const bridge::BridgeBodyPose2D pose = bridge::body_pose_2d(*m_system->m_impl->world2D, body.value);
    return PhysicsBodyPose2D{
        .valid = pose.valid,
        .position = fromBridge(pose.position),
        .rotationRadians = pose.rotation_radians,
    };
}

usize PhysicsWorld2D::bodyPoses(
    const std::span<const PhysicsBody2D> bodies,
    const std::span<PhysicsBodyPose2D> poses) const
{
    m_system->ensureInitialized();
    const usize count = std::min(bodies.size(), poses.size());
    if (count == 0U) {
        return 0U;
    }

    std::vector<u64> bridgeBodies(count);
    std::vector<bridge::BridgeBodyPose2D> bridgePoses(count);
    for (usize index = 0U; index < count; ++index) {
        bridgeBodies[index] = bodies[index].value;
    }

    const u64 written = bridge::body_poses_2d(
        *m_system->m_impl->world2D,
        rust::Slice<const u64>{bridgeBodies.data(), count},
        rust::Slice<bridge::BridgeBodyPose2D>{bridgePoses.data(), count});
    const usize safeWritten = std::min<usize>(static_cast<usize>(written), count);
    for (usize index = 0U; index < safeWritten; ++index) {
        poses[index] = PhysicsBodyPose2D{
            .valid = bridgePoses[index].valid,
            .position = fromBridge(bridgePoses[index].position),
            .rotationRadians = bridgePoses[index].rotation_radians,
        };
    }
    return safeWritten;
}

void PhysicsWorld2D::setBodyPosition(const PhysicsBody2D body, const Vector2 position, const f32 rotationRadians) const {
    m_system->ensureInitialized();
    bridge::set_body_position_2d(*m_system->m_impl->world2D, body.value, toBridge(position), rotationRadians);
}

void PhysicsWorld2D::setBodyLinearVelocity(const PhysicsBody2D body, const Vector2 velocity) const {
    m_system->ensureInitialized();
    bridge::set_body_linear_velocity_2d(*m_system->m_impl->world2D, body.value, toBridge(velocity));
}

PhysicsCollider2D PhysicsWorld2D::attachBox(const PhysicsBody2D body, const BoxColliderDesc2D& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeBoxColliderDesc2D bridgeDesc{
        .half_extents = toBridge(desc.halfExtents),
        .density = desc.density,
        .sensor = desc.sensor,
    };
    return PhysicsCollider2D{bridge::attach_box_2d(*m_system->m_impl->world2D, body.value, bridgeDesc)};
}

PhysicsCollider2D PhysicsWorld2D::attachCircle(const PhysicsBody2D body, const CircleColliderDesc& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeCircleColliderDesc bridgeDesc{
        .radius = desc.radius,
        .density = desc.density,
        .sensor = desc.sensor,
    };
    return PhysicsCollider2D{bridge::attach_circle_2d(*m_system->m_impl->world2D, body.value, bridgeDesc)};
}

PhysicsCollider2D PhysicsWorld2D::attachCapsule(const PhysicsBody2D body, const CapsuleColliderDesc2D& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeCapsuleColliderDesc2D bridgeDesc{
        .half_height = desc.halfHeight,
        .radius = desc.radius,
        .density = desc.density,
        .sensor = desc.sensor,
    };
    return PhysicsCollider2D{bridge::attach_capsule_2d(*m_system->m_impl->world2D, body.value, bridgeDesc)};
}

bool PhysicsWorld2D::colliderExists(const PhysicsCollider2D collider) const {
    m_system->ensureInitialized();
    return bridge::collider_exists_2d(*m_system->m_impl->world2D, collider.value);
}

void PhysicsWorld2D::setGravity(const Vector2 gravity) const {
    m_system->ensureInitialized();
    bridge::set_gravity_2d(*m_system->m_impl->world2D, toBridge(gravity));
}

std::optional<PhysicsRayHit2D> PhysicsWorld2D::raycast(
    const Vector2 origin,
    const Vector2 direction,
    const f32 maxDistance,
    const bool solid) const
{
    m_system->ensureInitialized();
    const bridge::BridgeRayHit2D hit =
        bridge::raycast_2d(*m_system->m_impl->world2D, toBridge(origin), toBridge(direction), maxDistance, solid);
    if (!hit.valid) {
        return std::nullopt;
    }
    return PhysicsRayHit2D{
        .collider = PhysicsCollider2D{hit.collider},
        .point = fromBridge(hit.point),
        .normal = fromBridge(hit.normal),
        .timeOfImpact = hit.time_of_impact,
    };
}

PhysicsWorld3D::PhysicsWorld3D(PhysicsSystem& system) noexcept
    : m_system(&system)
{
}

PhysicsBody3D PhysicsWorld3D::createBody(const PhysicsBodyDesc3D& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeBodyDesc3D bridgeDesc{
        .kind = toBridgeKind(desc.kind),
        .position = toBridge(desc.position),
        .linear_velocity = toBridge(desc.linearVelocity),
        .can_sleep = desc.canSleep,
    };
    return PhysicsBody3D{bridge::create_body_3d(*m_system->m_impl->world3D, bridgeDesc)};
}

void PhysicsWorld3D::removeBody(const PhysicsBody3D body) const {
    m_system->ensureInitialized();
    bridge::remove_body_3d(*m_system->m_impl->world3D, body.value);
}

bool PhysicsWorld3D::bodyExists(const PhysicsBody3D body) const {
    m_system->ensureInitialized();
    return bridge::body_exists_3d(*m_system->m_impl->world3D, body.value);
}

PhysicsBodyPose3D PhysicsWorld3D::bodyPose(const PhysicsBody3D body) const {
    m_system->ensureInitialized();
    const bridge::BridgeBodyPose3D pose = bridge::body_pose_3d(*m_system->m_impl->world3D, body.value);
    return PhysicsBodyPose3D{
        .valid = pose.valid,
        .position = fromBridge(pose.position),
        .rotation = fromBridge(pose.rotation),
    };
}

usize PhysicsWorld3D::bodyPoses(
    const std::span<const PhysicsBody3D> bodies,
    const std::span<PhysicsBodyPose3D> poses) const
{
    m_system->ensureInitialized();
    const usize count = std::min(bodies.size(), poses.size());
    if (count == 0U) {
        return 0U;
    }

    std::vector<u64> bridgeBodies(count);
    std::vector<bridge::BridgeBodyPose3D> bridgePoses(count);
    for (usize index = 0U; index < count; ++index) {
        bridgeBodies[index] = bodies[index].value;
    }

    const u64 written = bridge::body_poses_3d(
        *m_system->m_impl->world3D,
        rust::Slice<const u64>{bridgeBodies.data(), count},
        rust::Slice<bridge::BridgeBodyPose3D>{bridgePoses.data(), count});
    const usize safeWritten = std::min<usize>(static_cast<usize>(written), count);
    for (usize index = 0U; index < safeWritten; ++index) {
        poses[index] = PhysicsBodyPose3D{
            .valid = bridgePoses[index].valid,
            .position = fromBridge(bridgePoses[index].position),
            .rotation = fromBridge(bridgePoses[index].rotation),
        };
    }
    return safeWritten;
}

void PhysicsWorld3D::setBodyPosition(const PhysicsBody3D body, const Vector3 position) const {
    m_system->ensureInitialized();
    bridge::set_body_position_3d(*m_system->m_impl->world3D, body.value, toBridge(position));
}

void PhysicsWorld3D::setBodyLinearVelocity(const PhysicsBody3D body, const Vector3 velocity) const {
    m_system->ensureInitialized();
    bridge::set_body_linear_velocity_3d(*m_system->m_impl->world3D, body.value, toBridge(velocity));
}

PhysicsCollider3D PhysicsWorld3D::attachCuboid(const PhysicsBody3D body, const CuboidColliderDesc& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeCuboidColliderDesc bridgeDesc{
        .half_extents = toBridge(desc.halfExtents),
        .density = desc.density,
        .sensor = desc.sensor,
    };
    return PhysicsCollider3D{bridge::attach_cuboid_3d(*m_system->m_impl->world3D, body.value, bridgeDesc)};
}

PhysicsCollider3D PhysicsWorld3D::attachBall(const PhysicsBody3D body, const BallColliderDesc& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeBallColliderDesc bridgeDesc{
        .radius = desc.radius,
        .density = desc.density,
        .sensor = desc.sensor,
    };
    return PhysicsCollider3D{bridge::attach_ball_3d(*m_system->m_impl->world3D, body.value, bridgeDesc)};
}

PhysicsCollider3D PhysicsWorld3D::attachCapsule(const PhysicsBody3D body, const CapsuleColliderDesc3D& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeCapsuleColliderDesc3D bridgeDesc{
        .half_height = desc.halfHeight,
        .radius = desc.radius,
        .density = desc.density,
        .sensor = desc.sensor,
    };
    return PhysicsCollider3D{bridge::attach_capsule_3d(*m_system->m_impl->world3D, body.value, bridgeDesc)};
}

bool PhysicsWorld3D::colliderExists(const PhysicsCollider3D collider) const {
    m_system->ensureInitialized();
    return bridge::collider_exists_3d(*m_system->m_impl->world3D, collider.value);
}

void PhysicsWorld3D::setGravity(const Vector3 gravity) const {
    m_system->ensureInitialized();
    bridge::set_gravity_3d(*m_system->m_impl->world3D, toBridge(gravity));
}

std::optional<PhysicsRayHit3D> PhysicsWorld3D::raycast(
    const Vector3 origin,
    const Vector3 direction,
    const f32 maxDistance,
    const bool solid) const
{
    m_system->ensureInitialized();
    const bridge::BridgeRayHit3D hit =
        bridge::raycast_3d(*m_system->m_impl->world3D, toBridge(origin), toBridge(direction), maxDistance, solid);
    if (!hit.valid) {
        return std::nullopt;
    }
    return PhysicsRayHit3D{
        .collider = PhysicsCollider3D{hit.collider},
        .point = fromBridge(hit.point),
        .normal = fromBridge(hit.normal),
        .timeOfImpact = hit.time_of_impact,
    };
}

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem() noexcept {
    shutdown();
}

void PhysicsSystem::init() {
    if (!m_impl) {
        m_impl = std::make_unique<Impl>();
        m_contacts.reserve(64U);
    }
}

void PhysicsSystem::shutdown() noexcept {
    m_contacts.clear();
    m_impl.reset();
}

void PhysicsSystem::stepFixed(const f32 dt) {
    ensureInitialized();
    m_contacts.clear();
    const f32 safeDt = std::max(dt, 0.0f);
    bridge::step_world_2d(*m_impl->world2D, safeDt);
    bridge::step_world_3d(*m_impl->world3D, safeDt);
    drainContacts();
}

void PhysicsSystem::ensureInitialized() {
    if (!m_impl) {
        init();
    }
}

void PhysicsSystem::drainContacts() {
    const u64 count2D = bridge::contact_event_count_2d(*m_impl->world2D);
    for (u64 index = 0U; index < count2D; ++index) {
        const bridge::BridgeContactEvent contact = bridge::contact_event_2d(*m_impl->world2D, index);
        if (!contact.valid) {
            continue;
        }
        const PhysicsContactEvent event{
            .world = PhysicsWorldKind::World2D,
            .phase = contactPhaseFromBridge(contact.phase),
            .colliderA = contact.collider_a,
            .colliderB = contact.collider_b,
        };
        m_contacts.push_back(event);
        publishContact(event);
    }
    bridge::clear_contact_events_2d(*m_impl->world2D);

    const u64 count3D = bridge::contact_event_count_3d(*m_impl->world3D);
    for (u64 index = 0U; index < count3D; ++index) {
        const bridge::BridgeContactEvent contact = bridge::contact_event_3d(*m_impl->world3D, index);
        if (!contact.valid) {
            continue;
        }
        const PhysicsContactEvent event{
            .world = PhysicsWorldKind::World3D,
            .phase = contactPhaseFromBridge(contact.phase),
            .colliderA = contact.collider_a,
            .colliderB = contact.collider_b,
        };
        m_contacts.push_back(event);
        publishContact(event);
    }
    bridge::clear_contact_events_3d(*m_impl->world3D);
}

void PhysicsSystem::publishContact(const PhysicsContactEvent& event) const {
    if (event.phase == PhysicsContactPhase::Started) {
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::physics::CollisionStarted>({
            .world = event.world,
            .colliderA = event.colliderA,
            .colliderB = event.colliderB,
        });
        return;
    }
    ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::physics::CollisionEnded>({
        .world = event.world,
        .colliderA = event.colliderA,
        .colliderB = event.colliderB,
    });
}

} // namespace biofuel::engine::physics
