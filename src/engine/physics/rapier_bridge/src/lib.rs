use rapier2d::prelude as r2;
use rapier3d::prelude as r3;
use std::sync::mpsc::{channel, Receiver};

#[cxx::bridge(namespace = "biofuel::engine::physics::rapier_bridge")]
mod ffi {
    #[derive(Copy, Clone)]
    struct BridgeVec2 {
        x: f32,
        y: f32,
    }

    #[derive(Copy, Clone)]
    struct BridgeVec3 {
        x: f32,
        y: f32,
        z: f32,
    }

    struct BridgeQuat {
        x: f32,
        y: f32,
        z: f32,
        w: f32,
    }

    struct BridgeBodyDesc2D {
        kind: u8,
        position: BridgeVec2,
        linear_velocity: BridgeVec2,
        rotation_radians: f32,
        angular_velocity: f32,
        can_sleep: bool,
    }

    struct BridgeBodyDesc3D {
        kind: u8,
        position: BridgeVec3,
        linear_velocity: BridgeVec3,
        can_sleep: bool,
    }

    struct BridgeBoxColliderDesc2D {
        half_extents: BridgeVec2,
        density: f32,
        sensor: bool,
    }

    struct BridgeCircleColliderDesc {
        radius: f32,
        density: f32,
        sensor: bool,
    }

    struct BridgeCapsuleColliderDesc2D {
        half_height: f32,
        radius: f32,
        density: f32,
        sensor: bool,
    }

    struct BridgeCuboidColliderDesc {
        half_extents: BridgeVec3,
        density: f32,
        sensor: bool,
    }

    struct BridgeBallColliderDesc {
        radius: f32,
        density: f32,
        sensor: bool,
    }

    struct BridgeCapsuleColliderDesc3D {
        half_height: f32,
        radius: f32,
        density: f32,
        sensor: bool,
    }

    struct BridgeBodyPose2D {
        valid: bool,
        position: BridgeVec2,
        rotation_radians: f32,
    }

    struct BridgeBodyPose3D {
        valid: bool,
        position: BridgeVec3,
        rotation: BridgeQuat,
    }

    struct BridgeRayHit2D {
        valid: bool,
        collider: u64,
        point: BridgeVec2,
        normal: BridgeVec2,
        time_of_impact: f32,
    }

    struct BridgeRayHit3D {
        valid: bool,
        collider: u64,
        point: BridgeVec3,
        normal: BridgeVec3,
        time_of_impact: f32,
    }

    #[derive(Copy, Clone)]
    struct BridgeContactEvent {
        valid: bool,
        phase: u8,
        collider_a: u64,
        collider_b: u64,
    }

    #[derive(Copy, Clone)]
    struct BridgeContactForceEvent2D {
        valid: bool,
        collider_a: u64,
        collider_b: u64,
        total_force: BridgeVec2,
        max_force_direction: BridgeVec2,
        max_force_magnitude: f32,
    }

    #[derive(Copy, Clone)]
    struct BridgeContactForceEvent3D {
        valid: bool,
        collider_a: u64,
        collider_b: u64,
        total_force: BridgeVec3,
        max_force_direction: BridgeVec3,
        max_force_magnitude: f32,
    }

    // Per-stage timing from Rapier's own built-in PhysicsPipeline::counters
    // (requires the "profiler" cargo feature, enabled in Cargo.toml -- without
    // it these fields would silently read 0.0 forever).
    #[derive(Copy, Clone)]
    struct BridgeStepStats {
        step_time_ms: f32,
        broad_phase_time_ms: f32,
        narrow_phase_time_ms: f32,
        island_construction_time_ms: f32,
        solver_time_ms: f32,
        velocity_resolution_time_ms: f32,
        ccd_time_ms: f32,
        ncontact_pairs: u32,
        ncontacts: u32,
    }

    extern "Rust" {
        type RapierWorld2D;
        type RapierWorld3D;

        fn new_world_2d() -> Box<RapierWorld2D>;
        fn new_world_3d() -> Box<RapierWorld3D>;

        fn step_world_2d(world: &mut RapierWorld2D, dt: f32);
        fn step_world_3d(world: &mut RapierWorld3D, dt: f32);

        fn set_gravity_2d(world: &mut RapierWorld2D, gravity: BridgeVec2);
        fn set_gravity_3d(world: &mut RapierWorld3D, gravity: BridgeVec3);

        fn create_body_2d(world: &mut RapierWorld2D, desc: BridgeBodyDesc2D) -> u64;
        fn create_body_3d(world: &mut RapierWorld3D, desc: BridgeBodyDesc3D) -> u64;
        fn remove_body_2d(world: &mut RapierWorld2D, body: u64);
        fn remove_body_3d(world: &mut RapierWorld3D, body: u64);
        fn body_exists_2d(world: &RapierWorld2D, body: u64) -> bool;
        fn body_exists_3d(world: &RapierWorld3D, body: u64) -> bool;
        fn body_pose_2d(world: &RapierWorld2D, body: u64) -> BridgeBodyPose2D;
        fn body_pose_3d(world: &RapierWorld3D, body: u64) -> BridgeBodyPose3D;
        fn body_poses_2d(
            world: &RapierWorld2D,
            bodies: &[u64],
            poses: &mut [BridgeBodyPose2D],
        ) -> u64;
        fn body_poses_3d(
            world: &RapierWorld3D,
            bodies: &[u64],
            poses: &mut [BridgeBodyPose3D],
        ) -> u64;
        fn set_body_position_2d(
            world: &mut RapierWorld2D,
            body: u64,
            position: BridgeVec2,
            rotation_radians: f32,
        );
        fn set_body_position_3d(world: &mut RapierWorld3D, body: u64, position: BridgeVec3);
        fn set_body_linear_velocity_2d(world: &mut RapierWorld2D, body: u64, velocity: BridgeVec2);
        fn set_body_linear_velocity_3d(world: &mut RapierWorld3D, body: u64, velocity: BridgeVec3);

        fn attach_box_2d(
            world: &mut RapierWorld2D,
            body: u64,
            desc: BridgeBoxColliderDesc2D,
        ) -> u64;
        fn attach_circle_2d(
            world: &mut RapierWorld2D,
            body: u64,
            desc: BridgeCircleColliderDesc,
        ) -> u64;
        fn attach_capsule_2d(
            world: &mut RapierWorld2D,
            body: u64,
            desc: BridgeCapsuleColliderDesc2D,
        ) -> u64;
        fn attach_cuboid_3d(
            world: &mut RapierWorld3D,
            body: u64,
            desc: BridgeCuboidColliderDesc,
        ) -> u64;
        fn attach_ball_3d(
            world: &mut RapierWorld3D,
            body: u64,
            desc: BridgeBallColliderDesc,
        ) -> u64;
        fn attach_capsule_3d(
            world: &mut RapierWorld3D,
            body: u64,
            desc: BridgeCapsuleColliderDesc3D,
        ) -> u64;
        fn collider_exists_2d(world: &RapierWorld2D, collider: u64) -> bool;
        fn collider_exists_3d(world: &RapierWorld3D, collider: u64) -> bool;

        fn raycast_2d(
            world: &RapierWorld2D,
            origin: BridgeVec2,
            direction: BridgeVec2,
            max_distance: f32,
            solid: bool,
        ) -> BridgeRayHit2D;
        fn raycast_3d(
            world: &RapierWorld3D,
            origin: BridgeVec3,
            direction: BridgeVec3,
            max_distance: f32,
            solid: bool,
        ) -> BridgeRayHit3D;

        fn contact_event_count_2d(world: &RapierWorld2D) -> u64;
        fn contact_event_count_3d(world: &RapierWorld3D) -> u64;
        fn contact_event_2d(world: &RapierWorld2D, index: u64) -> BridgeContactEvent;
        fn contact_event_3d(world: &RapierWorld3D, index: u64) -> BridgeContactEvent;
        fn clear_contact_events_2d(world: &mut RapierWorld2D);
        fn clear_contact_events_3d(world: &mut RapierWorld3D);

        fn contact_force_event_count_2d(world: &RapierWorld2D) -> u64;
        fn contact_force_event_count_3d(world: &RapierWorld3D) -> u64;
        fn contact_force_event_2d(world: &RapierWorld2D, index: u64) -> BridgeContactForceEvent2D;
        fn contact_force_event_3d(world: &RapierWorld3D, index: u64) -> BridgeContactForceEvent3D;
        fn clear_contact_force_events_2d(world: &mut RapierWorld2D);
        fn clear_contact_force_events_3d(world: &mut RapierWorld3D);

        fn last_step_stats_2d(world: &RapierWorld2D) -> BridgeStepStats;
        fn last_step_stats_3d(world: &RapierWorld3D) -> BridgeStepStats;
    }
}

const KIND_FIXED: u8 = 0;
const KIND_DYNAMIC: u8 = 1;
const KIND_KINEMATIC_POSITION: u8 = 2;
const KIND_KINEMATIC_VELOCITY: u8 = 3;
const PHASE_STARTED: u8 = 0;
const PHASE_ENDED: u8 = 1;

fn pack_handle(index: u32, generation: u32) -> u64 {
    (((generation as u64) << 32) | index as u64) + 1
}

fn unpack_handle(raw: u64) -> (u32, u32) {
    if raw == 0 {
        return (u32::MAX, u32::MAX);
    }
    let adjusted = raw - 1;
    (adjusted as u32, (adjusted >> 32) as u32)
}

fn pack_body_2d(handle: r2::RigidBodyHandle) -> u64 {
    let (index, generation) = handle.into_raw_parts();
    pack_handle(index, generation)
}

fn pack_body_3d(handle: r3::RigidBodyHandle) -> u64 {
    let (index, generation) = handle.into_raw_parts();
    pack_handle(index, generation)
}

fn pack_collider_2d(handle: r2::ColliderHandle) -> u64 {
    let (index, generation) = handle.into_raw_parts();
    pack_handle(index, generation)
}

fn pack_collider_3d(handle: r3::ColliderHandle) -> u64 {
    let (index, generation) = handle.into_raw_parts();
    pack_handle(index, generation)
}

fn unpack_body_2d(raw: u64) -> r2::RigidBodyHandle {
    let (index, generation) = unpack_handle(raw);
    r2::RigidBodyHandle::from_raw_parts(index, generation)
}

fn unpack_body_3d(raw: u64) -> r3::RigidBodyHandle {
    let (index, generation) = unpack_handle(raw);
    r3::RigidBodyHandle::from_raw_parts(index, generation)
}

fn unpack_collider_2d(raw: u64) -> r2::ColliderHandle {
    let (index, generation) = unpack_handle(raw);
    r2::ColliderHandle::from_raw_parts(index, generation)
}

fn unpack_collider_3d(raw: u64) -> r3::ColliderHandle {
    let (index, generation) = unpack_handle(raw);
    r3::ColliderHandle::from_raw_parts(index, generation)
}

fn safe_positive(value: f32, fallback: f32) -> f32 {
    if value.is_finite() && value > 0.0 {
        value
    } else {
        fallback
    }
}

fn vec2(value: ffi::BridgeVec2) -> r2::Vector {
    r2::Vector::new(value.x, value.y)
}

fn vec3(value: ffi::BridgeVec3) -> r3::Vector {
    r3::Vector::new(value.x, value.y, value.z)
}

fn point2(value: &ffi::BridgeVec2) -> r2::Vector {
    r2::Vector::new(value.x, value.y)
}

fn point3(value: &ffi::BridgeVec3) -> r3::Vector {
    r3::Vector::new(value.x, value.y, value.z)
}

fn bridge_vec2(value: r2::Vector) -> ffi::BridgeVec2 {
    ffi::BridgeVec2 {
        x: value.x,
        y: value.y,
    }
}

fn bridge_vec3(value: r3::Vector) -> ffi::BridgeVec3 {
    ffi::BridgeVec3 {
        x: value.x,
        y: value.y,
        z: value.z,
    }
}

fn body_type(kind: u8) -> r2::RigidBodyType {
    match kind {
        KIND_FIXED => r2::RigidBodyType::Fixed,
        KIND_DYNAMIC => r2::RigidBodyType::Dynamic,
        KIND_KINEMATIC_POSITION => r2::RigidBodyType::KinematicPositionBased,
        KIND_KINEMATIC_VELOCITY => r2::RigidBodyType::KinematicVelocityBased,
        _ => r2::RigidBodyType::Fixed,
    }
}

fn body_type_3d(kind: u8) -> r3::RigidBodyType {
    match kind {
        KIND_FIXED => r3::RigidBodyType::Fixed,
        KIND_DYNAMIC => r3::RigidBodyType::Dynamic,
        KIND_KINEMATIC_POSITION => r3::RigidBodyType::KinematicPositionBased,
        KIND_KINEMATIC_VELOCITY => r3::RigidBodyType::KinematicVelocityBased,
        _ => r3::RigidBodyType::Fixed,
    }
}

pub struct RapierWorld2D {
    pipeline: r2::PhysicsPipeline,
    gravity: r2::Vector,
    integration: r2::IntegrationParameters,
    islands: r2::IslandManager,
    broad_phase: r2::BroadPhaseBvh,
    narrow_phase: r2::NarrowPhase,
    bodies: r2::RigidBodySet,
    colliders: r2::ColliderSet,
    impulse_joints: r2::ImpulseJointSet,
    multibody_joints: r2::MultibodyJointSet,
    ccd_solver: r2::CCDSolver,
    event_handler: r2::ChannelEventCollector,
    collision_events: Receiver<r2::CollisionEvent>,
    contact_force_events: Receiver<r2::ContactForceEvent>,
    drained_contacts: Vec<ffi::BridgeContactEvent>,
    drained_contact_forces: Vec<ffi::BridgeContactForceEvent2D>,
}

pub struct RapierWorld3D {
    pipeline: r3::PhysicsPipeline,
    gravity: r3::Vector,
    integration: r3::IntegrationParameters,
    islands: r3::IslandManager,
    broad_phase: r3::BroadPhaseBvh,
    narrow_phase: r3::NarrowPhase,
    bodies: r3::RigidBodySet,
    colliders: r3::ColliderSet,
    impulse_joints: r3::ImpulseJointSet,
    multibody_joints: r3::MultibodyJointSet,
    ccd_solver: r3::CCDSolver,
    event_handler: r3::ChannelEventCollector,
    collision_events: Receiver<r3::CollisionEvent>,
    contact_force_events: Receiver<r3::ContactForceEvent>,
    drained_contacts: Vec<ffi::BridgeContactEvent>,
    drained_contact_forces: Vec<ffi::BridgeContactForceEvent3D>,
}

impl RapierWorld2D {
    fn new() -> Self {
        let (collision_send, collision_events) = channel();
        let (contact_force_send, contact_force_events) = channel();
        Self {
            pipeline: r2::PhysicsPipeline::new(),
            gravity: r2::Vector::new(0.0, 0.0),
            integration: r2::IntegrationParameters::default(),
            islands: r2::IslandManager::new(),
            broad_phase: r2::BroadPhaseBvh::new(),
            narrow_phase: r2::NarrowPhase::new(),
            bodies: r2::RigidBodySet::new(),
            colliders: r2::ColliderSet::new(),
            impulse_joints: r2::ImpulseJointSet::new(),
            multibody_joints: r2::MultibodyJointSet::new(),
            ccd_solver: r2::CCDSolver::new(),
            event_handler: r2::ChannelEventCollector::new(collision_send, contact_force_send),
            collision_events,
            contact_force_events,
            drained_contacts: Vec::with_capacity(32),
            drained_contact_forces: Vec::with_capacity(32),
        }
    }

    fn collider_builder_events(
        builder: r2::ColliderBuilder,
        density: f32,
        sensor: bool,
    ) -> r2::ColliderBuilder {
        builder
            .density(density.max(0.0))
            .sensor(sensor)
            .active_events(r2::ActiveEvents::COLLISION_EVENTS | r2::ActiveEvents::CONTACT_FORCE_EVENTS)
    }

    fn drain_events(&mut self) {
        self.drained_contacts.clear();
        self.drained_contact_forces.clear();
        while let Ok(event) = self.collision_events.try_recv() {
            let (phase, collider_a, collider_b) = match event {
                r2::CollisionEvent::Started(a, b, _) => (PHASE_STARTED, a, b),
                r2::CollisionEvent::Stopped(a, b, _) => (PHASE_ENDED, a, b),
            };
            self.drained_contacts.push(ffi::BridgeContactEvent {
                valid: true,
                phase,
                collider_a: pack_collider_2d(collider_a),
                collider_b: pack_collider_2d(collider_b),
            });
        }
        while let Ok(event) = self.contact_force_events.try_recv() {
            self.drained_contact_forces
                .push(ffi::BridgeContactForceEvent2D {
                    valid: true,
                    collider_a: pack_collider_2d(event.collider1),
                    collider_b: pack_collider_2d(event.collider2),
                    total_force: bridge_vec2(event.total_force),
                    max_force_direction: bridge_vec2(event.max_force_direction),
                    max_force_magnitude: event.max_force_magnitude,
                });
        }
    }
}

impl RapierWorld3D {
    fn new() -> Self {
        let (collision_send, collision_events) = channel();
        let (contact_force_send, contact_force_events) = channel();
        Self {
            pipeline: r3::PhysicsPipeline::new(),
            gravity: r3::Vector::new(0.0, 0.0, 0.0),
            integration: r3::IntegrationParameters::default(),
            islands: r3::IslandManager::new(),
            broad_phase: r3::BroadPhaseBvh::new(),
            narrow_phase: r3::NarrowPhase::new(),
            bodies: r3::RigidBodySet::new(),
            colliders: r3::ColliderSet::new(),
            impulse_joints: r3::ImpulseJointSet::new(),
            multibody_joints: r3::MultibodyJointSet::new(),
            ccd_solver: r3::CCDSolver::new(),
            event_handler: r3::ChannelEventCollector::new(collision_send, contact_force_send),
            collision_events,
            contact_force_events,
            drained_contacts: Vec::with_capacity(32),
            drained_contact_forces: Vec::with_capacity(32),
        }
    }

    fn collider_builder_events(
        builder: r3::ColliderBuilder,
        density: f32,
        sensor: bool,
    ) -> r3::ColliderBuilder {
        builder
            .density(density.max(0.0))
            .sensor(sensor)
            .active_events(r3::ActiveEvents::COLLISION_EVENTS | r3::ActiveEvents::CONTACT_FORCE_EVENTS)
    }

    fn drain_events(&mut self) {
        self.drained_contacts.clear();
        self.drained_contact_forces.clear();
        while let Ok(event) = self.collision_events.try_recv() {
            let (phase, collider_a, collider_b) = match event {
                r3::CollisionEvent::Started(a, b, _) => (PHASE_STARTED, a, b),
                r3::CollisionEvent::Stopped(a, b, _) => (PHASE_ENDED, a, b),
            };
            self.drained_contacts.push(ffi::BridgeContactEvent {
                valid: true,
                phase,
                collider_a: pack_collider_3d(collider_a),
                collider_b: pack_collider_3d(collider_b),
            });
        }
        while let Ok(event) = self.contact_force_events.try_recv() {
            self.drained_contact_forces
                .push(ffi::BridgeContactForceEvent3D {
                    valid: true,
                    collider_a: pack_collider_3d(event.collider1),
                    collider_b: pack_collider_3d(event.collider2),
                    total_force: bridge_vec3(event.total_force),
                    max_force_direction: bridge_vec3(event.max_force_direction),
                    max_force_magnitude: event.max_force_magnitude,
                });
        }
    }
}

pub fn new_world_2d() -> Box<RapierWorld2D> {
    Box::new(RapierWorld2D::new())
}

pub fn new_world_3d() -> Box<RapierWorld3D> {
    Box::new(RapierWorld3D::new())
}

pub fn step_world_2d(world: &mut RapierWorld2D, dt: f32) {
    if !dt.is_finite() || dt <= 0.0 {
        world.drained_contacts.clear();
        world.drained_contact_forces.clear();
        return;
    }
    world.integration.dt = dt;
    world.pipeline.step(
        world.gravity,
        &world.integration,
        &mut world.islands,
        &mut world.broad_phase,
        &mut world.narrow_phase,
        &mut world.bodies,
        &mut world.colliders,
        &mut world.impulse_joints,
        &mut world.multibody_joints,
        &mut world.ccd_solver,
        &(),
        &world.event_handler,
    );
    world.drain_events();
}

pub fn last_step_stats_2d(world: &RapierWorld2D) -> ffi::BridgeStepStats {
    let counters = &world.pipeline.counters;
    ffi::BridgeStepStats {
        step_time_ms: counters.step_time_ms() as f32,
        broad_phase_time_ms: counters.broad_phase_time_ms() as f32,
        narrow_phase_time_ms: counters.narrow_phase_time_ms() as f32,
        island_construction_time_ms: counters.island_construction_time_ms() as f32,
        solver_time_ms: counters.solver_time_ms() as f32,
        velocity_resolution_time_ms: counters.velocity_resolution_time_ms() as f32,
        ccd_time_ms: counters.ccd_time_ms() as f32,
        ncontact_pairs: counters.cd.ncontact_pairs as u32,
        ncontacts: counters.solver.ncontacts as u32,
    }
}

pub fn step_world_3d(world: &mut RapierWorld3D, dt: f32) {
    if !dt.is_finite() || dt <= 0.0 {
        world.drained_contacts.clear();
        world.drained_contact_forces.clear();
        return;
    }
    world.integration.dt = dt;
    world.pipeline.step(
        world.gravity,
        &world.integration,
        &mut world.islands,
        &mut world.broad_phase,
        &mut world.narrow_phase,
        &mut world.bodies,
        &mut world.colliders,
        &mut world.impulse_joints,
        &mut world.multibody_joints,
        &mut world.ccd_solver,
        &(),
        &world.event_handler,
    );
    world.drain_events();
}

pub fn last_step_stats_3d(world: &RapierWorld3D) -> ffi::BridgeStepStats {
    let counters = &world.pipeline.counters;
    ffi::BridgeStepStats {
        step_time_ms: counters.step_time_ms() as f32,
        broad_phase_time_ms: counters.broad_phase_time_ms() as f32,
        narrow_phase_time_ms: counters.narrow_phase_time_ms() as f32,
        island_construction_time_ms: counters.island_construction_time_ms() as f32,
        solver_time_ms: counters.solver_time_ms() as f32,
        velocity_resolution_time_ms: counters.velocity_resolution_time_ms() as f32,
        ccd_time_ms: counters.ccd_time_ms() as f32,
        ncontact_pairs: counters.cd.ncontact_pairs as u32,
        ncontacts: counters.solver.ncontacts as u32,
    }
}

pub fn set_gravity_2d(world: &mut RapierWorld2D, gravity: ffi::BridgeVec2) {
    world.gravity = vec2(gravity);
}

pub fn set_gravity_3d(world: &mut RapierWorld3D, gravity: ffi::BridgeVec3) {
    world.gravity = vec3(gravity);
}

pub fn create_body_2d(world: &mut RapierWorld2D, desc: ffi::BridgeBodyDesc2D) -> u64 {
    let body = r2::RigidBodyBuilder::new(body_type(desc.kind))
        .translation(vec2(desc.position))
        .rotation(desc.rotation_radians)
        .linvel(vec2(desc.linear_velocity))
        .angvel(desc.angular_velocity)
        .can_sleep(desc.can_sleep)
        .build();
    pack_body_2d(world.bodies.insert(body))
}

pub fn create_body_3d(world: &mut RapierWorld3D, desc: ffi::BridgeBodyDesc3D) -> u64 {
    let body = r3::RigidBodyBuilder::new(body_type_3d(desc.kind))
        .translation(vec3(desc.position))
        .linvel(vec3(desc.linear_velocity))
        .can_sleep(desc.can_sleep)
        .build();
    pack_body_3d(world.bodies.insert(body))
}

pub fn remove_body_2d(world: &mut RapierWorld2D, body: u64) {
    let handle = unpack_body_2d(body);
    world.bodies.remove(
        handle,
        &mut world.islands,
        &mut world.colliders,
        &mut world.impulse_joints,
        &mut world.multibody_joints,
        true,
    );
}

pub fn remove_body_3d(world: &mut RapierWorld3D, body: u64) {
    let handle = unpack_body_3d(body);
    world.bodies.remove(
        handle,
        &mut world.islands,
        &mut world.colliders,
        &mut world.impulse_joints,
        &mut world.multibody_joints,
        true,
    );
}

pub fn body_exists_2d(world: &RapierWorld2D, body: u64) -> bool {
    world.bodies.get(unpack_body_2d(body)).is_some()
}

pub fn body_exists_3d(world: &RapierWorld3D, body: u64) -> bool {
    world.bodies.get(unpack_body_3d(body)).is_some()
}

pub fn body_pose_2d(world: &RapierWorld2D, body: u64) -> ffi::BridgeBodyPose2D {
    if let Some(rigid_body) = world.bodies.get(unpack_body_2d(body)) {
        let translation = rigid_body.translation();
        return ffi::BridgeBodyPose2D {
            valid: true,
            position: ffi::BridgeVec2 {
                x: translation.x,
                y: translation.y,
            },
            rotation_radians: rigid_body.rotation().angle(),
        };
    }
    ffi::BridgeBodyPose2D {
        valid: false,
        position: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
        rotation_radians: 0.0,
    }
}

pub fn body_pose_3d(world: &RapierWorld3D, body: u64) -> ffi::BridgeBodyPose3D {
    if let Some(rigid_body) = world.bodies.get(unpack_body_3d(body)) {
        let translation = rigid_body.translation();
        let rotation = rigid_body.rotation();
        return ffi::BridgeBodyPose3D {
            valid: true,
            position: ffi::BridgeVec3 {
                x: translation.x,
                y: translation.y,
                z: translation.z,
            },
            rotation: ffi::BridgeQuat {
                x: rotation.x,
                y: rotation.y,
                z: rotation.z,
                w: rotation.w,
            },
        };
    }
    ffi::BridgeBodyPose3D {
        valid: false,
        position: ffi::BridgeVec3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        rotation: ffi::BridgeQuat {
            x: 0.0,
            y: 0.0,
            z: 0.0,
            w: 1.0,
        },
    }
}

pub fn body_poses_2d(
    world: &RapierWorld2D,
    bodies: &[u64],
    poses: &mut [ffi::BridgeBodyPose2D],
) -> u64 {
    let count = bodies.len().min(poses.len());
    for index in 0..count {
        poses[index] = body_pose_2d(world, bodies[index]);
    }
    count as u64
}

pub fn body_poses_3d(
    world: &RapierWorld3D,
    bodies: &[u64],
    poses: &mut [ffi::BridgeBodyPose3D],
) -> u64 {
    let count = bodies.len().min(poses.len());
    for index in 0..count {
        poses[index] = body_pose_3d(world, bodies[index]);
    }
    count as u64
}

pub fn set_body_position_2d(
    world: &mut RapierWorld2D,
    body: u64,
    position: ffi::BridgeVec2,
    rotation_radians: f32,
) {
    if let Some(rigid_body) = world.bodies.get_mut(unpack_body_2d(body)) {
        rigid_body.set_translation(vec2(position), true);
        rigid_body.set_rotation(r2::Rotation::new(rotation_radians), true);
    }
}

pub fn set_body_position_3d(world: &mut RapierWorld3D, body: u64, position: ffi::BridgeVec3) {
    if let Some(rigid_body) = world.bodies.get_mut(unpack_body_3d(body)) {
        rigid_body.set_translation(vec3(position), true);
    }
}

pub fn set_body_linear_velocity_2d(
    world: &mut RapierWorld2D,
    body: u64,
    velocity: ffi::BridgeVec2,
) {
    if let Some(rigid_body) = world.bodies.get_mut(unpack_body_2d(body)) {
        rigid_body.set_linvel(vec2(velocity), true);
    }
}

pub fn set_body_linear_velocity_3d(
    world: &mut RapierWorld3D,
    body: u64,
    velocity: ffi::BridgeVec3,
) {
    if let Some(rigid_body) = world.bodies.get_mut(unpack_body_3d(body)) {
        rigid_body.set_linvel(vec3(velocity), true);
    }
}

pub fn attach_box_2d(
    world: &mut RapierWorld2D,
    body: u64,
    desc: ffi::BridgeBoxColliderDesc2D,
) -> u64 {
    let handle = unpack_body_2d(body);
    if !world.bodies.contains(handle) {
        return 0;
    }
    let builder = r2::ColliderBuilder::cuboid(
        safe_positive(desc.half_extents.x, 0.5),
        safe_positive(desc.half_extents.y, 0.5),
    );
    let collider = RapierWorld2D::collider_builder_events(builder, desc.density, desc.sensor);
    pack_collider_2d(
        world
            .colliders
            .insert_with_parent(collider, handle, &mut world.bodies),
    )
}

pub fn attach_circle_2d(
    world: &mut RapierWorld2D,
    body: u64,
    desc: ffi::BridgeCircleColliderDesc,
) -> u64 {
    let handle = unpack_body_2d(body);
    if !world.bodies.contains(handle) {
        return 0;
    }
    let builder = r2::ColliderBuilder::ball(safe_positive(desc.radius, 0.5));
    let collider = RapierWorld2D::collider_builder_events(builder, desc.density, desc.sensor);
    pack_collider_2d(
        world
            .colliders
            .insert_with_parent(collider, handle, &mut world.bodies),
    )
}

pub fn attach_capsule_2d(
    world: &mut RapierWorld2D,
    body: u64,
    desc: ffi::BridgeCapsuleColliderDesc2D,
) -> u64 {
    let handle = unpack_body_2d(body);
    if !world.bodies.contains(handle) {
        return 0;
    }
    let builder = r2::ColliderBuilder::capsule_y(
        safe_positive(desc.half_height, 0.5),
        safe_positive(desc.radius, 0.25),
    );
    let collider = RapierWorld2D::collider_builder_events(builder, desc.density, desc.sensor);
    pack_collider_2d(
        world
            .colliders
            .insert_with_parent(collider, handle, &mut world.bodies),
    )
}

pub fn attach_cuboid_3d(
    world: &mut RapierWorld3D,
    body: u64,
    desc: ffi::BridgeCuboidColliderDesc,
) -> u64 {
    let handle = unpack_body_3d(body);
    if !world.bodies.contains(handle) {
        return 0;
    }
    let builder = r3::ColliderBuilder::cuboid(
        safe_positive(desc.half_extents.x, 0.5),
        safe_positive(desc.half_extents.y, 0.5),
        safe_positive(desc.half_extents.z, 0.5),
    );
    let collider = RapierWorld3D::collider_builder_events(builder, desc.density, desc.sensor);
    pack_collider_3d(
        world
            .colliders
            .insert_with_parent(collider, handle, &mut world.bodies),
    )
}

pub fn attach_ball_3d(
    world: &mut RapierWorld3D,
    body: u64,
    desc: ffi::BridgeBallColliderDesc,
) -> u64 {
    let handle = unpack_body_3d(body);
    if !world.bodies.contains(handle) {
        return 0;
    }
    let builder = r3::ColliderBuilder::ball(safe_positive(desc.radius, 0.5));
    let collider = RapierWorld3D::collider_builder_events(builder, desc.density, desc.sensor);
    pack_collider_3d(
        world
            .colliders
            .insert_with_parent(collider, handle, &mut world.bodies),
    )
}

pub fn attach_capsule_3d(
    world: &mut RapierWorld3D,
    body: u64,
    desc: ffi::BridgeCapsuleColliderDesc3D,
) -> u64 {
    let handle = unpack_body_3d(body);
    if !world.bodies.contains(handle) {
        return 0;
    }
    let builder = r3::ColliderBuilder::capsule_y(
        safe_positive(desc.half_height, 0.5),
        safe_positive(desc.radius, 0.25),
    );
    let collider = RapierWorld3D::collider_builder_events(builder, desc.density, desc.sensor);
    pack_collider_3d(
        world
            .colliders
            .insert_with_parent(collider, handle, &mut world.bodies),
    )
}

pub fn collider_exists_2d(world: &RapierWorld2D, collider: u64) -> bool {
    world.colliders.get(unpack_collider_2d(collider)).is_some()
}

pub fn collider_exists_3d(world: &RapierWorld3D, collider: u64) -> bool {
    world.colliders.get(unpack_collider_3d(collider)).is_some()
}

pub fn raycast_2d(
    world: &RapierWorld2D,
    origin: ffi::BridgeVec2,
    direction: ffi::BridgeVec2,
    max_distance: f32,
    solid: bool,
) -> ffi::BridgeRayHit2D {
    let dir = vec2(direction);
    let norm = dir.length();
    if !norm.is_finite() || norm <= f32::EPSILON || !max_distance.is_finite() || max_distance <= 0.0
    {
        return ffi::BridgeRayHit2D {
            valid: false,
            collider: 0,
            point: origin,
            normal: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
            time_of_impact: 0.0,
        };
    }
    let unit_dir = dir / norm;
    let ray = r2::Ray::new(point2(&origin), unit_dir);
    let query = world.broad_phase.as_query_pipeline(
        world.narrow_phase.query_dispatcher(),
        &world.bodies,
        &world.colliders,
        r2::QueryFilter::default(),
    );
    if let Some((handle, hit)) = query.cast_ray_and_get_normal(&ray, max_distance, solid) {
        let point = ray.point_at(hit.time_of_impact);
        return ffi::BridgeRayHit2D {
            valid: true,
            collider: pack_collider_2d(handle),
            point: ffi::BridgeVec2 {
                x: point.x,
                y: point.y,
            },
            normal: bridge_vec2(hit.normal),
            time_of_impact: hit.time_of_impact,
        };
    }
    ffi::BridgeRayHit2D {
        valid: false,
        collider: 0,
        point: origin,
        normal: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
        time_of_impact: 0.0,
    }
}

pub fn raycast_3d(
    world: &RapierWorld3D,
    origin: ffi::BridgeVec3,
    direction: ffi::BridgeVec3,
    max_distance: f32,
    solid: bool,
) -> ffi::BridgeRayHit3D {
    let dir = vec3(direction);
    let norm = dir.length();
    if !norm.is_finite() || norm <= f32::EPSILON || !max_distance.is_finite() || max_distance <= 0.0
    {
        return ffi::BridgeRayHit3D {
            valid: false,
            collider: 0,
            point: origin,
            normal: ffi::BridgeVec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            time_of_impact: 0.0,
        };
    }
    let unit_dir = dir / norm;
    let ray = r3::Ray::new(point3(&origin), unit_dir);
    let query = world.broad_phase.as_query_pipeline(
        world.narrow_phase.query_dispatcher(),
        &world.bodies,
        &world.colliders,
        r3::QueryFilter::default(),
    );
    if let Some((handle, hit)) = query.cast_ray_and_get_normal(&ray, max_distance, solid) {
        let point = ray.point_at(hit.time_of_impact);
        return ffi::BridgeRayHit3D {
            valid: true,
            collider: pack_collider_3d(handle),
            point: ffi::BridgeVec3 {
                x: point.x,
                y: point.y,
                z: point.z,
            },
            normal: bridge_vec3(hit.normal),
            time_of_impact: hit.time_of_impact,
        };
    }
    ffi::BridgeRayHit3D {
        valid: false,
        collider: 0,
        point: origin,
        normal: ffi::BridgeVec3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        time_of_impact: 0.0,
    }
}

pub fn contact_event_count_2d(world: &RapierWorld2D) -> u64 {
    world.drained_contacts.len() as u64
}

pub fn contact_event_count_3d(world: &RapierWorld3D) -> u64 {
    world.drained_contacts.len() as u64
}

const INVALID_CONTACT_EVENT: ffi::BridgeContactEvent = ffi::BridgeContactEvent {
    valid: false,
    phase: PHASE_STARTED,
    collider_a: 0,
    collider_b: 0,
};

pub fn contact_event_2d(world: &RapierWorld2D, index: u64) -> ffi::BridgeContactEvent {
    world
        .drained_contacts
        .get(index as usize)
        .copied()
        .unwrap_or(INVALID_CONTACT_EVENT)
}

pub fn contact_event_3d(world: &RapierWorld3D, index: u64) -> ffi::BridgeContactEvent {
    world
        .drained_contacts
        .get(index as usize)
        .copied()
        .unwrap_or(INVALID_CONTACT_EVENT)
}

pub fn clear_contact_events_2d(world: &mut RapierWorld2D) {
    world.drained_contacts.clear();
}

pub fn clear_contact_events_3d(world: &mut RapierWorld3D) {
    world.drained_contacts.clear();
}

pub fn contact_force_event_count_2d(world: &RapierWorld2D) -> u64 {
    world.drained_contact_forces.len() as u64
}

pub fn contact_force_event_count_3d(world: &RapierWorld3D) -> u64 {
    world.drained_contact_forces.len() as u64
}

const INVALID_CONTACT_FORCE_EVENT_2D: ffi::BridgeContactForceEvent2D = ffi::BridgeContactForceEvent2D {
    valid: false,
    collider_a: 0,
    collider_b: 0,
    total_force: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
    max_force_direction: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
    max_force_magnitude: 0.0,
};

const INVALID_CONTACT_FORCE_EVENT_3D: ffi::BridgeContactForceEvent3D = ffi::BridgeContactForceEvent3D {
    valid: false,
    collider_a: 0,
    collider_b: 0,
    total_force: ffi::BridgeVec3 { x: 0.0, y: 0.0, z: 0.0 },
    max_force_direction: ffi::BridgeVec3 { x: 0.0, y: 0.0, z: 0.0 },
    max_force_magnitude: 0.0,
};

pub fn contact_force_event_2d(world: &RapierWorld2D, index: u64) -> ffi::BridgeContactForceEvent2D {
    world
        .drained_contact_forces
        .get(index as usize)
        .copied()
        .unwrap_or(INVALID_CONTACT_FORCE_EVENT_2D)
}

pub fn contact_force_event_3d(world: &RapierWorld3D, index: u64) -> ffi::BridgeContactForceEvent3D {
    world
        .drained_contact_forces
        .get(index as usize)
        .copied()
        .unwrap_or(INVALID_CONTACT_FORCE_EVENT_3D)
}

pub fn clear_contact_force_events_2d(world: &mut RapierWorld2D) {
    world.drained_contact_forces.clear();
}

pub fn clear_contact_force_events_3d(world: &mut RapierWorld3D) {
    world.drained_contact_forces.clear();
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn creates_worlds_and_handles_invalid_bodies() {
        let world_2d = new_world_2d();
        let world_3d = new_world_3d();
        assert!(!body_exists_2d(&world_2d, 123));
        assert!(!body_exists_3d(&world_3d, 123));
    }

    #[test]
    fn zero_or_invalid_step_does_not_advance_bodies() {
        let mut world_2d = new_world_2d();
        let body_2d = create_body_2d(
            &mut world_2d,
            ffi::BridgeBodyDesc2D {
                kind: KIND_DYNAMIC,
                position: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
                linear_velocity: ffi::BridgeVec2 { x: 0.0, y: 8.0 },
                rotation_radians: 0.0,
                angular_velocity: 0.0,
                can_sleep: true,
            },
        );
        step_world_2d(&mut world_2d, 0.0);
        assert_eq!(body_pose_2d(&world_2d, body_2d).position.y, 0.0);

        let mut world_3d = new_world_3d();
        let body_3d = create_body_3d(
            &mut world_3d,
            ffi::BridgeBodyDesc3D {
                kind: KIND_DYNAMIC,
                position: ffi::BridgeVec3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                },
                linear_velocity: ffi::BridgeVec3 {
                    x: 0.0,
                    y: 8.0,
                    z: 0.0,
                },
                can_sleep: true,
            },
        );
        step_world_3d(&mut world_3d, f32::NAN);
        assert_eq!(body_pose_3d(&world_3d, body_3d).position.y, 0.0);
    }

    #[test]
    fn raycasts_against_2d_and_3d_colliders() {
        let mut world_2d = new_world_2d();
        let body_2d = create_body_2d(
            &mut world_2d,
            ffi::BridgeBodyDesc2D {
                kind: KIND_FIXED,
                position: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
                linear_velocity: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
                rotation_radians: 0.0,
                angular_velocity: 0.0,
                can_sleep: true,
            },
        );
        attach_box_2d(
            &mut world_2d,
            body_2d,
            ffi::BridgeBoxColliderDesc2D {
                half_extents: ffi::BridgeVec2 { x: 1.0, y: 1.0 },
                density: 1.0,
                sensor: false,
            },
        );
        step_world_2d(&mut world_2d, 1.0 / 60.0);
        assert!(
            raycast_2d(
                &world_2d,
                ffi::BridgeVec2 { x: 0.0, y: 3.0 },
                ffi::BridgeVec2 { x: 0.0, y: -1.0 },
                8.0,
                true,
            )
            .valid
        );
        assert!(
            !raycast_2d(
                &world_2d,
                ffi::BridgeVec2 { x: 0.0, y: 3.0 },
                ffi::BridgeVec2 { x: 0.0, y: -1.0 },
                0.0,
                true,
            )
            .valid
        );

        let mut world_3d = new_world_3d();
        let body_3d = create_body_3d(
            &mut world_3d,
            ffi::BridgeBodyDesc3D {
                kind: KIND_FIXED,
                position: ffi::BridgeVec3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                },
                linear_velocity: ffi::BridgeVec3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                },
                can_sleep: true,
            },
        );
        attach_cuboid_3d(
            &mut world_3d,
            body_3d,
            ffi::BridgeCuboidColliderDesc {
                half_extents: ffi::BridgeVec3 {
                    x: 1.0,
                    y: 1.0,
                    z: 1.0,
                },
                density: 1.0,
                sensor: false,
            },
        );
        step_world_3d(&mut world_3d, 1.0 / 60.0);
        assert!(
            raycast_3d(
                &world_3d,
                ffi::BridgeVec3 {
                    x: 0.0,
                    y: 3.0,
                    z: 0.0,
                },
                ffi::BridgeVec3 {
                    x: 0.0,
                    y: -1.0,
                    z: 0.0,
                },
                8.0,
                true,
            )
            .valid
        );
        assert!(
            !raycast_3d(
                &world_3d,
                ffi::BridgeVec3 {
                    x: 0.0,
                    y: 3.0,
                    z: 0.0,
                },
                ffi::BridgeVec3 {
                    x: 0.0,
                    y: -1.0,
                    z: 0.0,
                },
                f32::NAN,
                true,
            )
            .valid
        );
    }

    #[test]
    fn emits_contact_events() {
        let mut world = new_world_2d();
        set_gravity_2d(&mut world, ffi::BridgeVec2 { x: 0.0, y: -9.8 });

        let ground = create_body_2d(
            &mut world,
            ffi::BridgeBodyDesc2D {
                kind: KIND_FIXED,
                position: ffi::BridgeVec2 { x: 0.0, y: -1.0 },
                linear_velocity: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
                rotation_radians: 0.0,
                angular_velocity: 0.0,
                can_sleep: true,
            },
        );
        attach_box_2d(
            &mut world,
            ground,
            ffi::BridgeBoxColliderDesc2D {
                half_extents: ffi::BridgeVec2 { x: 4.0, y: 0.25 },
                density: 1.0,
                sensor: false,
            },
        );

        let ball = create_body_2d(
            &mut world,
            ffi::BridgeBodyDesc2D {
                kind: KIND_DYNAMIC,
                position: ffi::BridgeVec2 { x: 0.0, y: 1.0 },
                linear_velocity: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
                rotation_radians: 0.0,
                angular_velocity: 0.0,
                can_sleep: true,
            },
        );
        attach_circle_2d(
            &mut world,
            ball,
            ffi::BridgeCircleColliderDesc {
                radius: 0.25,
                density: 1.0,
                sensor: false,
            },
        );

        let mut saw_contact = false;
        for _ in 0..180 {
            step_world_2d(&mut world, 1.0 / 60.0);
            saw_contact = saw_contact || contact_event_count_2d(&world) > 0;
        }

        assert!(saw_contact);
    }
}
