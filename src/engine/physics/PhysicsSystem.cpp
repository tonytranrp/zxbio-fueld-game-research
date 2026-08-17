#include "engine/physics/PhysicsSystem.hpp"

#include "biofuel_rapier_bridge_cxx/lib.h"
#include "engine/events/physics/PhysicsEventModule.hpp"
#include "engine/runtime/typed/Events.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>
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
    std::unordered_map<u64, CollisionGroup> colliderGroups2D;
    std::unordered_map<u64, CollisionGroup> colliderGroups3D;
    std::unordered_map<u64, std::vector<u64>> bodyColliders2D;
    std::unordered_map<u64, std::vector<u64>> bodyColliders3D;
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
    m_system->purgeBodyColliderGroups(PhysicsWorldKind::World2D, body.value);
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

void PhysicsWorld2D::setBodyType(const PhysicsBody2D body, const PhysicsBodyKind newType) const {
    m_system->ensureInitialized();
    // Not yet bridged to Rapier: this call is currently a silent no-op.
    // TODO(Phase 5): Wire through bridge when set_body_type_2d is exposed.
    // Rapier supports RigidBody::set_body_type() for Fixed↔Dynamic mutation.
    (void)body;
    (void)newType;
}

void PhysicsWorld2D::wakeBody(const PhysicsBody2D body) const {
    m_system->ensureInitialized();
    // Not yet bridged to Rapier: this call is currently a silent no-op.
    // TODO(Phase 5): Wire through bridge when wake_body_2d is exposed.
    (void)body;
}

void PhysicsWorld2D::putBodyToSleep(const PhysicsBody2D body) const {
    m_system->ensureInitialized();
    // Not yet bridged to Rapier: this call is currently a silent no-op.
    // TODO(Phase 5): Wire through bridge when sleep_body_2d is exposed.
    (void)body;
}

std::optional<bool> PhysicsWorld2D::isBodySleeping(const PhysicsBody2D body) const {
    m_system->ensureInitialized();
    // Not yet bridged to Rapier: returns std::nullopt (unknown), not a real
    // sleep state. TODO(Phase 5): Wire through bridge when is_body_sleeping_2d
    // is exposed.
    (void)body;
    return std::nullopt;
}

PhysicsCollider2D PhysicsWorld2D::attachBox(const PhysicsBody2D body, const BoxColliderDesc2D& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeBoxColliderDesc2D bridgeDesc{
        .half_extents = toBridge(desc.halfExtents),
        .density = desc.density,
        .sensor = desc.sensor,
    };
    const u64 handle = bridge::attach_box_2d(*m_system->m_impl->world2D, body.value, bridgeDesc);
    m_system->registerColliderGroup(PhysicsWorldKind::World2D, handle, desc.collisionGroup);
    m_system->trackBodyCollider(PhysicsWorldKind::World2D, body.value, handle);
    return PhysicsCollider2D{handle};
}

PhysicsCollider2D PhysicsWorld2D::attachCircle(const PhysicsBody2D body, const CircleColliderDesc& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeCircleColliderDesc bridgeDesc{
        .radius = desc.radius,
        .density = desc.density,
        .sensor = desc.sensor,
    };
    const u64 handle = bridge::attach_circle_2d(*m_system->m_impl->world2D, body.value, bridgeDesc);
    m_system->registerColliderGroup(PhysicsWorldKind::World2D, handle, desc.collisionGroup);
    m_system->trackBodyCollider(PhysicsWorldKind::World2D, body.value, handle);
    return PhysicsCollider2D{handle};
}

PhysicsCollider2D PhysicsWorld2D::attachCapsule(const PhysicsBody2D body, const CapsuleColliderDesc2D& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeCapsuleColliderDesc2D bridgeDesc{
        .half_height = desc.halfHeight,
        .radius = desc.radius,
        .density = desc.density,
        .sensor = desc.sensor,
    };
    const u64 handle = bridge::attach_capsule_2d(*m_system->m_impl->world2D, body.value, bridgeDesc);
    m_system->registerColliderGroup(PhysicsWorldKind::World2D, handle, desc.collisionGroup);
    m_system->trackBodyCollider(PhysicsWorldKind::World2D, body.value, handle);
    return PhysicsCollider2D{handle};
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

Joint2D PhysicsWorld2D::createJoint(const JointDesc2D& /*desc*/) const {
    // Not yet bridged to Rapier: returns an inert handle {0} and performs no
    // simulation work. Callers must not treat the handle as a live joint.
    return Joint2D{0U};
}

void PhysicsWorld2D::removeJoint(const Joint2D /*joint*/) const {
    // Not yet bridged to Rapier: this call is currently a silent no-op.
}

std::optional<bool> PhysicsWorld2D::jointExists(const Joint2D joint) const {
    // Not yet bridged to Rapier: returns std::nullopt (unknown), not a real
    // existence check.
    (void)joint;
    return std::nullopt;
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
    m_system->purgeBodyColliderGroups(PhysicsWorldKind::World3D, body.value);
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

void PhysicsWorld3D::setBodyType(const PhysicsBody3D body, const PhysicsBodyKind newType) const {
    m_system->ensureInitialized();
    // Not yet bridged to Rapier: this call is currently a silent no-op.
    // TODO(Phase 5): Wire through bridge when set_body_type_3d is exposed.
    // Rapier supports RigidBody::set_body_type() for Fixed↔Dynamic mutation.
    (void)body;
    (void)newType;
}

void PhysicsWorld3D::wakeBody(const PhysicsBody3D body) const {
    m_system->ensureInitialized();
    // Not yet bridged to Rapier: this call is currently a silent no-op.
    // TODO(Phase 5): Wire through bridge when wake_body_3d is exposed.
    (void)body;
}

void PhysicsWorld3D::putBodyToSleep(const PhysicsBody3D body) const {
    m_system->ensureInitialized();
    // Not yet bridged to Rapier: this call is currently a silent no-op.
    // TODO(Phase 5): Wire through bridge when sleep_body_3d is exposed.
    (void)body;
}

std::optional<bool> PhysicsWorld3D::isBodySleeping(const PhysicsBody3D body) const {
    m_system->ensureInitialized();
    // Not yet bridged to Rapier: returns std::nullopt (unknown), not a real
    // sleep state. TODO(Phase 5): Wire through bridge when is_body_sleeping_3d
    // is exposed.
    (void)body;
    return std::nullopt;
}

PhysicsCollider3D PhysicsWorld3D::attachCuboid(const PhysicsBody3D body, const CuboidColliderDesc& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeCuboidColliderDesc bridgeDesc{
        .half_extents = toBridge(desc.halfExtents),
        .density = desc.density,
        .sensor = desc.sensor,
    };
    const u64 handle = bridge::attach_cuboid_3d(*m_system->m_impl->world3D, body.value, bridgeDesc);
    m_system->registerColliderGroup(PhysicsWorldKind::World3D, handle, desc.collisionGroup);
    m_system->trackBodyCollider(PhysicsWorldKind::World3D, body.value, handle);
    return PhysicsCollider3D{handle};
}

PhysicsCollider3D PhysicsWorld3D::attachBall(const PhysicsBody3D body, const BallColliderDesc& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeBallColliderDesc bridgeDesc{
        .radius = desc.radius,
        .density = desc.density,
        .sensor = desc.sensor,
    };
    const u64 handle = bridge::attach_ball_3d(*m_system->m_impl->world3D, body.value, bridgeDesc);
    m_system->registerColliderGroup(PhysicsWorldKind::World3D, handle, desc.collisionGroup);
    m_system->trackBodyCollider(PhysicsWorldKind::World3D, body.value, handle);
    return PhysicsCollider3D{handle};
}

PhysicsCollider3D PhysicsWorld3D::attachCapsule(const PhysicsBody3D body, const CapsuleColliderDesc3D& desc) const {
    m_system->ensureInitialized();
    const bridge::BridgeCapsuleColliderDesc3D bridgeDesc{
        .half_height = desc.halfHeight,
        .radius = desc.radius,
        .density = desc.density,
        .sensor = desc.sensor,
    };
    const u64 handle = bridge::attach_capsule_3d(*m_system->m_impl->world3D, body.value, bridgeDesc);
    m_system->registerColliderGroup(PhysicsWorldKind::World3D, handle, desc.collisionGroup);
    m_system->trackBodyCollider(PhysicsWorldKind::World3D, body.value, handle);
    return PhysicsCollider3D{handle};
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

Joint3D PhysicsWorld3D::createJoint(const JointDesc3D& /*desc*/) const {
    // Not yet bridged to Rapier: returns an inert handle {0} and performs no
    // simulation work. Callers must not treat the handle as a live joint.
    return Joint3D{0U};
}

void PhysicsWorld3D::removeJoint(const Joint3D /*joint*/) const {
    // Not yet bridged to Rapier: this call is currently a silent no-op.
}

std::optional<bool> PhysicsWorld3D::jointExists(const Joint3D joint) const {
    // Not yet bridged to Rapier: returns std::nullopt (unknown), not a real
    // existence check.
    (void)joint;
    return std::nullopt;
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

void PhysicsSystem::setFixedTimestep(const f32 dt) noexcept {
    m_fixedTimestep = dt > 0.0f ? dt : (1.0f / 60.0f);
}

void PhysicsSystem::setMaxSubSteps(const i32 n) noexcept {
    m_maxSubSteps = n < 1 ? 1 : (n > 16 ? 16 : n);
}

void PhysicsSystem::setSolverIterations(const i32 n) noexcept {
    m_integrationConfig.solverIterations = n < 1 ? 1 : (n > 128 ? 128 : n);
}

void PhysicsSystem::setMaxCcdSubsteps(const i32 n) noexcept {
    m_integrationConfig.maxCcdSubsteps = n < 1 ? 1 : (n > 32 ? 32 : n);
}

void PhysicsSystem::setErp(const f32 erp) noexcept {
    m_integrationConfig.erp = std::clamp(erp, 0.0f, 1.0f);
}

void PhysicsSystem::stepFixed(const f32 dt) {
    ensureInitialized();
    m_contacts.clear();

    const f32 safeDt = std::max(dt, 0.0f);
    if (safeDt <= 0.0f) {
        return;
    }

    // Cache world pointers once — the hot sub-step loop and drainContacts()
    // dereference these repeatedly.  Lifting them into locals avoids re-loading
    // m_impl->world2D/world3D through the unique_ptr indirection on every
    // iteration and inside drainContacts.
    auto& world2D = *m_impl->world2D;
    auto& world3D = *m_impl->world3D;

    const f32 timestep = m_fixedTimestep > 0.0f ? m_fixedTimestep : (1.0f / 60.0f);

    if (safeDt <= timestep) {
        bridge::step_world_2d(world2D, safeDt);
        bridge::step_world_3d(world3D, safeDt);
        drainContacts(world2D, world3D);
        return;
    }

    i32 subSteps = static_cast<i32>(std::ceil(safeDt / timestep));
    if (subSteps > m_maxSubSteps) {
        subSteps = m_maxSubSteps;
    }
    if (subSteps < 1) {
        subSteps = 1;
    }

    const f32 subDt = safeDt / static_cast<f32>(subSteps);
    for (i32 i = 0; i < subSteps; ++i) {
        bridge::step_world_2d(world2D, subDt);
        bridge::step_world_3d(world3D, subDt);
    }
    drainContacts(world2D, world3D);
}

void PhysicsSystem::ensureInitialized() {
    if (!m_impl) {
        init();
    }
}

void PhysicsSystem::drainContacts(
    bridge::RapierWorld2D& world2D,
    bridge::RapierWorld3D& world3D)
{
    const u64 count2D = bridge::contact_event_count_2d(world2D);
    const u64 count3D = bridge::contact_event_count_3d(world3D);

    // Single reserve up-front — avoids incremental reallocations as contacts
    // are pushed in the loops below.
    m_contacts.reserve(m_contacts.size() + count2D + count3D);

    for (u64 index = 0U; index < count2D; ++index) {
        const bridge::BridgeContactEvent contact = bridge::contact_event_2d(world2D, index);
        if (!contact.valid) {
            continue;
        }
        if (!passesCollisionFilter(PhysicsWorldKind::World2D, contact.collider_a, contact.collider_b)) {
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
    bridge::clear_contact_events_2d(world2D);

    for (u64 index = 0U; index < count3D; ++index) {
        const bridge::BridgeContactEvent contact = bridge::contact_event_3d(world3D, index);
        if (!contact.valid) {
            continue;
        }
        if (!passesCollisionFilter(PhysicsWorldKind::World3D, contact.collider_a, contact.collider_b)) {
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
    bridge::clear_contact_events_3d(world3D);
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

void PhysicsSystem::registerColliderGroup(
    const PhysicsWorldKind world,
    const u64 colliderHandle,
    const CollisionGroup group)
{
    ensureInitialized();
    if (colliderHandle == 0U) {
        return;
    }
    if (world == PhysicsWorldKind::World2D) {
        m_impl->colliderGroups2D[colliderHandle] = group;
    } else {
        m_impl->colliderGroups3D[colliderHandle] = group;
    }
}

void PhysicsSystem::unregisterColliderGroup(
    const PhysicsWorldKind world,
    const u64 colliderHandle)
{
    if (!m_impl) {
        return;
    }
    if (world == PhysicsWorldKind::World2D) {
        m_impl->colliderGroups2D.erase(colliderHandle);
    } else {
        m_impl->colliderGroups3D.erase(colliderHandle);
    }
}

void PhysicsSystem::trackBodyCollider(
    const PhysicsWorldKind world,
    const u64 bodyHandle,
    const u64 colliderHandle)
{
    if (colliderHandle == 0U) {
        return;
    }
    auto& bodyColliders = (world == PhysicsWorldKind::World2D)
                              ? m_impl->bodyColliders2D
                              : m_impl->bodyColliders3D;
    bodyColliders[bodyHandle].push_back(colliderHandle);
}

void PhysicsSystem::purgeBodyColliderGroups(
    const PhysicsWorldKind world,
    const u64 bodyHandle)
{
    if (!m_impl) {
        return;
    }
    auto& bodyColliders = (world == PhysicsWorldKind::World2D)
                              ? m_impl->bodyColliders2D
                              : m_impl->bodyColliders3D;
    auto& groups = (world == PhysicsWorldKind::World2D)
                       ? m_impl->colliderGroups2D
                       : m_impl->colliderGroups3D;

    const auto it = bodyColliders.find(bodyHandle);
    if (it == bodyColliders.end()) {
        return;
    }
    for (const u64 colliderHandle : it->second) {
        groups.erase(colliderHandle);
    }
    bodyColliders.erase(it);
}

bool PhysicsSystem::passesCollisionFilter(
    const PhysicsWorldKind world,
    const u64 colliderA,
    const u64 colliderB) const
{
    const auto& groups = (world == PhysicsWorldKind::World2D)
                             ? m_impl->colliderGroups2D
                             : m_impl->colliderGroups3D;

    const auto itA = groups.find(colliderA);
    const auto itB = groups.find(colliderB);

    if (itA == groups.end() || itB == groups.end()) {
        return true;
    }

    const CollisionGroup& groupA = itA->second;
    const CollisionGroup& groupB = itB->second;

    return groupA.collidesWith(groupB);
}

} // namespace biofuel::engine::physics
