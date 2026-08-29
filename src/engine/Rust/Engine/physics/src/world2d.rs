// RapierWorld2D: the 2D physics world plus every cxx-facing free function
// that operates on it. Not currently driven by any game-side system (see
// engine/physics/README.md) but kept at parity with world3d.rs.
use crate::convert::{body_type, bridge_vec2, point2, vec2};
use crate::ffi;
use crate::handles::{
    pack_body_2d, pack_collider_2d, safe_positive, unpack_body_2d, unpack_collider_2d, PHASE_ENDED,
    PHASE_STARTED,
};
use rapier2d::prelude as r2;
use std::sync::mpsc::{channel, Receiver};

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
            .active_events(
                r2::ActiveEvents::COLLISION_EVENTS | r2::ActiveEvents::CONTACT_FORCE_EVENTS,
            )
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

pub fn new_world_2d() -> Box<RapierWorld2D> {
    Box::new(RapierWorld2D::new())
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

pub fn set_gravity_2d(world: &mut RapierWorld2D, gravity: ffi::BridgeVec2) {
    world.gravity = vec2(gravity);
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

pub fn body_exists_2d(world: &RapierWorld2D, body: u64) -> bool {
    world.bodies.get(unpack_body_2d(body)).is_some()
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

pub fn set_body_linear_velocity_2d(
    world: &mut RapierWorld2D,
    body: u64,
    velocity: ffi::BridgeVec2,
) {
    if let Some(rigid_body) = world.bodies.get_mut(unpack_body_2d(body)) {
        rigid_body.set_linvel(vec2(velocity), true);
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

pub fn collider_exists_2d(world: &RapierWorld2D, collider: u64) -> bool {
    world.colliders.get(unpack_collider_2d(collider)).is_some()
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

pub fn contact_event_count_2d(world: &RapierWorld2D) -> u64 {
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

pub fn clear_contact_events_2d(world: &mut RapierWorld2D) {
    world.drained_contacts.clear();
}

pub fn contact_force_event_count_2d(world: &RapierWorld2D) -> u64 {
    world.drained_contact_forces.len() as u64
}

const INVALID_CONTACT_FORCE_EVENT_2D: ffi::BridgeContactForceEvent2D =
    ffi::BridgeContactForceEvent2D {
        valid: false,
        collider_a: 0,
        collider_b: 0,
        total_force: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
        max_force_direction: ffi::BridgeVec2 { x: 0.0, y: 0.0 },
        max_force_magnitude: 0.0,
    };

pub fn contact_force_event_2d(world: &RapierWorld2D, index: u64) -> ffi::BridgeContactForceEvent2D {
    world
        .drained_contact_forces
        .get(index as usize)
        .copied()
        .unwrap_or(INVALID_CONTACT_FORCE_EVENT_2D)
}

pub fn clear_contact_force_events_2d(world: &mut RapierWorld2D) {
    world.drained_contact_forces.clear();
}
