// RapierWorld3D: the 3D physics world plus every cxx-facing free function
// that operates on it, except move_character_3d and raycast_3d (see
// character_controller.rs and raycast3d.rs -- split out to keep this file
// under the project's ~400-line target).
//
// broad_phase/narrow_phase/bodies/colliders are pub(crate), not private:
// both of those files build their own query pipeline from them directly.
use crate::convert::{body_type_3d, bridge_vec3, vec3};
use crate::ffi;
use crate::handles::{
    pack_body_3d, pack_collider_3d, safe_positive, unpack_body_3d, unpack_collider_3d, PHASE_ENDED,
    PHASE_STARTED,
};
use rapier3d::prelude as r3;
use std::sync::mpsc::{channel, Receiver};

pub struct RapierWorld3D {
    pipeline: r3::PhysicsPipeline,
    gravity: r3::Vector,
    integration: r3::IntegrationParameters,
    islands: r3::IslandManager,
    pub(crate) broad_phase: r3::BroadPhaseBvh,
    pub(crate) narrow_phase: r3::NarrowPhase,
    pub(crate) bodies: r3::RigidBodySet,
    pub(crate) colliders: r3::ColliderSet,
    impulse_joints: r3::ImpulseJointSet,
    multibody_joints: r3::MultibodyJointSet,
    ccd_solver: r3::CCDSolver,
    event_handler: r3::ChannelEventCollector,
    collision_events: Receiver<r3::CollisionEvent>,
    contact_force_events: Receiver<r3::ContactForceEvent>,
    drained_contacts: Vec<ffi::BridgeContactEvent>,
    drained_contact_forces: Vec<ffi::BridgeContactForceEvent3D>,
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
            .active_events(
                r3::ActiveEvents::COLLISION_EVENTS | r3::ActiveEvents::CONTACT_FORCE_EVENTS,
            )
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

pub fn new_world_3d() -> Box<RapierWorld3D> {
    Box::new(RapierWorld3D::new())
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

pub fn set_gravity_3d(world: &mut RapierWorld3D, gravity: ffi::BridgeVec3) {
    world.gravity = vec3(gravity);
}

pub fn create_body_3d(world: &mut RapierWorld3D, desc: ffi::BridgeBodyDesc3D) -> u64 {
    let body = r3::RigidBodyBuilder::new(body_type_3d(desc.kind))
        .translation(vec3(desc.position))
        .linvel(vec3(desc.linear_velocity))
        .can_sleep(desc.can_sleep)
        .build();
    pack_body_3d(world.bodies.insert(body))
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

pub fn body_exists_3d(world: &RapierWorld3D, body: u64) -> bool {
    world.bodies.get(unpack_body_3d(body)).is_some()
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

pub fn set_body_position_3d(world: &mut RapierWorld3D, body: u64, position: ffi::BridgeVec3) {
    if let Some(rigid_body) = world.bodies.get_mut(unpack_body_3d(body)) {
        rigid_body.set_translation(vec3(position), true);
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

pub fn collider_exists_3d(world: &RapierWorld3D, collider: u64) -> bool {
    world.colliders.get(unpack_collider_3d(collider)).is_some()
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

pub fn contact_event_3d(world: &RapierWorld3D, index: u64) -> ffi::BridgeContactEvent {
    world
        .drained_contacts
        .get(index as usize)
        .copied()
        .unwrap_or(INVALID_CONTACT_EVENT)
}

pub fn clear_contact_events_3d(world: &mut RapierWorld3D) {
    world.drained_contacts.clear();
}

pub fn contact_force_event_count_3d(world: &RapierWorld3D) -> u64 {
    world.drained_contact_forces.len() as u64
}

const INVALID_CONTACT_FORCE_EVENT_3D: ffi::BridgeContactForceEvent3D =
    ffi::BridgeContactForceEvent3D {
        valid: false,
        collider_a: 0,
        collider_b: 0,
        total_force: ffi::BridgeVec3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        max_force_direction: ffi::BridgeVec3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        max_force_magnitude: 0.0,
    };

pub fn contact_force_event_3d(world: &RapierWorld3D, index: u64) -> ffi::BridgeContactForceEvent3D {
    world
        .drained_contact_forces
        .get(index as usize)
        .copied()
        .unwrap_or(INVALID_CONTACT_FORCE_EVENT_3D)
}

pub fn clear_contact_force_events_3d(world: &mut RapierWorld3D) {
    world.drained_contact_forces.clear();
}
