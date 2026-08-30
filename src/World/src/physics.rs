//! Native Rapier physics world for this session's gameplay.
//!
//! `RapierPhysics` (fields, `new()`, `step()`) is a straight port of
//! `Engine/game/src/world.rs`'s own resource of the same shape -- kept as a
//! separate copy rather than a shared dependency because `World/` is
//! deliberately its own standalone Cargo workspace (see `Cargo.toml`'s own
//! doc comment) and this struct is small and stable. `move_character` is the
//! same kinematic-character-controller call `Engine/physics/src/
//! character_controller.rs::move_character_3d` makes for the C++-facing
//! bridge, minus the `ffi::Bridge*` DTO translation that only exists there
//! to cross the cxx boundary -- this crate calls `rapier3d` directly, no FFI
//! needed since it's pure Rust end to end.
#![forbid(unsafe_code)]

use bevy::prelude::Resource;
use rapier3d::control::{CharacterLength, KinematicCharacterController};
use rapier3d::prelude as r3;

#[derive(Resource)]
pub(crate) struct RapierPhysics {
    pipeline: r3::PhysicsPipeline,
    gravity: r3::Vector,
    integration: r3::IntegrationParameters,
    islands: r3::IslandManager,
    broad_phase: r3::BroadPhaseBvh,
    narrow_phase: r3::NarrowPhase,
    pub(crate) bodies: r3::RigidBodySet,
    pub(crate) colliders: r3::ColliderSet,
    impulse_joints: r3::ImpulseJointSet,
    multibody_joints: r3::MultibodyJointSet,
    ccd_solver: r3::CCDSolver,
}

// Result of a single move_character call -- mirrors CharacterMovement3D's
// two fields the C++ CharacterController3D::step() actually reads
// (translation, grounded); is_sliding_down_slope isn't consumed on that
// side either, so it's dropped here too rather than carried for no reader.
pub(crate) struct CharacterMoveResult {
    pub(crate) translation: r3::Vector,
    pub(crate) grounded: bool,
}

impl RapierPhysics {
    pub(crate) fn new() -> Self {
        Self {
            pipeline: r3::PhysicsPipeline::new(),
            gravity: r3::Vector::new(0.0, -9.81, 0.0),
            integration: r3::IntegrationParameters::default(),
            islands: r3::IslandManager::new(),
            broad_phase: r3::BroadPhaseBvh::new(),
            narrow_phase: r3::NarrowPhase::new(),
            bodies: r3::RigidBodySet::new(),
            colliders: r3::ColliderSet::new(),
            impulse_joints: r3::ImpulseJointSet::new(),
            multibody_joints: r3::MultibodyJointSet::new(),
            ccd_solver: r3::CCDSolver::new(),
        }
    }

    pub(crate) fn step(&mut self, dt: f32) {
        if !dt.is_finite() || dt <= 0.0 {
            return;
        }
        self.integration.dt = dt;
        self.pipeline.step(
            self.gravity,
            &self.integration,
            &mut self.islands,
            &mut self.broad_phase,
            &mut self.narrow_phase,
            &mut self.bodies,
            &mut self.colliders,
            &mut self.impulse_joints,
            &mut self.multibody_joints,
            &mut self.ccd_solver,
            &(),
            &(),
        );
    }

    // Same defaults as CharacterControllerDesc3D's C++ struct (see
    // PhysicsTypes.hpp) except snap_to_ground, which the caller passes in
    // explicitly -- CharacterController3D::step() only snaps while already
    // grounded (snapping every tick would glue a jumping/falling character
    // straight back onto the ground), so it can't be a fixed constant here.
    pub(crate) fn move_character(
        &self,
        collider: r3::ColliderHandle,
        exclude_body: r3::RigidBodyHandle,
        position: r3::Vector,
        desired_translation: r3::Vector,
        dt: f32,
        snap_to_ground: f32,
    ) -> Option<CharacterMoveResult> {
        if !dt.is_finite() || dt <= 0.0 {
            return None;
        }
        let col = self.colliders.get(collider)?;
        let shape = col.shape();
        let pose = r3::Pose::from_translation(position);

        let filter = r3::QueryFilter::default().exclude_rigid_body(exclude_body);
        let query = self.broad_phase.as_query_pipeline(
            self.narrow_phase.query_dispatcher(),
            &self.bodies,
            &self.colliders,
            filter,
        );

        let controller = KinematicCharacterController {
            up: r3::Vector::new(0.0, 1.0, 0.0),
            offset: CharacterLength::Absolute(0.01),
            slide: true,
            autostep: None,
            max_slope_climb_angle: 0.785398,
            min_slope_slide_angle: 0.785398,
            snap_to_ground: if snap_to_ground > 0.0 {
                Some(CharacterLength::Absolute(snap_to_ground))
            } else {
                None
            },
            normal_nudge_factor: 1.0e-4,
        };

        let movement = controller.move_shape(dt, &query, shape, &pose, desired_translation, |_collision| {});

        Some(CharacterMoveResult {
            translation: movement.translation,
            grounded: movement.grounded,
        })
    }
}

// Same two properties tests/physics/CharacterControllerSmoke.cpp checks for
// the C++-facing move_character_3d (grounded-detection-via-falling,
// wall-blocking) -- that test exercises the FFI-bridged version; these
// exercise this crate's own move_character directly, in pure Rust, with no
// window/EventLoop/main-thread requirement (unlike session.rs's reentrancy
// behavior, which genuinely can't be `cargo test`-ed -- see that module's
// own note), so `cargo test` covers this new wrapper the same way ctest
// already covers the C++ one.
#[cfg(test)]
mod tests {
    use super::*;

    const DT: f32 = 1.0 / 60.0;
    const CAPSULE_HALF_HEIGHT: f32 = 0.5;
    const CAPSULE_RADIUS: f32 = 0.35;

    fn spawn_capsule(physics: &mut RapierPhysics, position: r3::Vector) -> (r3::RigidBodyHandle, r3::ColliderHandle) {
        let body = r3::RigidBodyBuilder::new(r3::RigidBodyType::KinematicPositionBased)
            .translation(position)
            .build();
        let handle = physics.bodies.insert(body);
        let collider = r3::ColliderBuilder::capsule_y(CAPSULE_HALF_HEIGHT, CAPSULE_RADIUS);
        let collider_handle = physics.colliders.insert_with_parent(collider, handle, &mut physics.bodies);
        (handle, collider_handle)
    }

    #[test]
    fn character_reports_grounded_after_falling_onto_fixed_floor() {
        let mut physics = RapierPhysics::new();
        let floor_body = r3::RigidBodyBuilder::new(r3::RigidBodyType::Fixed)
            .translation(r3::Vector::new(0.0, -0.25, 0.0))
            .build();
        let floor_handle = physics.bodies.insert(floor_body);
        let floor_collider = r3::ColliderBuilder::cuboid(10.0, 0.25, 10.0);
        physics.colliders.insert_with_parent(floor_collider, floor_handle, &mut physics.bodies);

        let (body, collider) = spawn_capsule(&mut physics, r3::Vector::new(0.0, 1.0, 0.0));

        let mut pos = r3::Vector::new(0.0, 1.0, 0.0);
        let mut grounded = false;
        for _ in 0..120 {
            physics.step(DT);
            let Some(result) = physics.move_character(collider, body, pos, r3::Vector::new(0.0, -9.8 * DT, 0.0), DT, 0.2)
            else {
                panic!("move_character returned None for a valid grounded-fall step");
            };
            pos = r3::Vector::new(pos.x + result.translation.x, pos.y + result.translation.y, pos.z + result.translation.z);
            if let Some(rb) = physics.bodies.get_mut(body) {
                rb.set_translation(pos, true);
            }
            grounded = result.grounded;
            if grounded {
                break;
            }
        }
        assert!(grounded, "character never reported grounded after falling onto a fixed floor");
    }

    #[test]
    fn character_is_blocked_by_a_wall_instead_of_tunneling_through() {
        let mut physics = RapierPhysics::new();
        let wall_body = r3::RigidBodyBuilder::new(r3::RigidBodyType::Fixed)
            .translation(r3::Vector::new(3.0, 1.0, 0.0))
            .build();
        let wall_handle = physics.bodies.insert(wall_body);
        let wall_collider = r3::ColliderBuilder::cuboid(0.25, 2.0, 10.0);
        physics.colliders.insert_with_parent(wall_collider, wall_handle, &mut physics.bodies);

        let (body, collider) = spawn_capsule(&mut physics, r3::Vector::new(0.0, 1.0, 0.0));

        let desired_into_wall = r3::Vector::new(1.0, 0.0, 0.0);
        let mut pos = r3::Vector::new(0.0, 1.0, 0.0);
        let mut total_moved_toward_wall = 0.0_f32;
        for _ in 0..300 {
            physics.step(DT);
            let Some(result) = physics.move_character(collider, body, pos, desired_into_wall, DT, 0.0) else {
                panic!("move_character returned None during wall-approach step");
            };
            pos = r3::Vector::new(pos.x + result.translation.x, pos.y + result.translation.y, pos.z + result.translation.z);
            if let Some(rb) = physics.bodies.get_mut(body) {
                rb.set_translation(pos, true);
            }
            total_moved_toward_wall += result.translation.x;
        }
        // Wall face is at x=2.75 (3.0 - 0.25 half-extent); capsule radius 0.35
        // plus a small offset means the center should stop noticeably short
        // of the wall's far side (x=3.25) instead of tunnelling through to
        // 300 * desired_into_wall.x.
        assert!(pos.x < 3.0, "character penetrated through the wall instead of being blocked (x={})", pos.x);
        assert!(total_moved_toward_wall > 1.0, "character did not move toward the wall at all");
    }

    #[test]
    fn move_character_rejects_non_positive_dt() {
        let mut physics = RapierPhysics::new();
        let (body, collider) = spawn_capsule(&mut physics, r3::Vector::new(0.0, 1.0, 0.0));
        let result = physics.move_character(collider, body, r3::Vector::new(0.0, 1.0, 0.0), r3::Vector::new(1.0, 0.0, 0.0), 0.0, 0.0);
        assert!(result.is_none(), "dt <= 0 did not return None");
    }

    #[test]
    fn move_character_rejects_a_nonexistent_collider() {
        let physics = RapierPhysics::new();
        // Default-constructed handles point at nothing in a fresh set --
        // same "invalid sentinel" role PhysicsCollider3D{999999}/
        // PhysicsBody3D{} play in the C++ test this mirrors.
        let bogus_body = r3::RigidBodyHandle::default();
        let bogus_collider = r3::ColliderHandle::default();
        let result = physics.move_character(bogus_collider, bogus_body, r3::Vector::new(0.0, 1.0, 0.0), r3::Vector::new(1.0, 0.0, 0.0), DT, 0.0);
        assert!(result.is_none(), "nonexistent collider handle did not return None");
    }
}
