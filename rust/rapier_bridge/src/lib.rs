//! cxx bridge crate: exposes a 2D and a 3D Rapier physics world to C++.
//!
//! Organized by concern rather than one file:
//! - `mod ffi` (bottom of this file) -- the cxx bridge contract (shared
//!   structs + fn signatures). Has to stay literally in this file: cxx's
//!   macro processes the mod's items directly and doesn't expand an
//!   `include!` placed inside it.
//! - `handles` -- opaque u64 handle packing/unpacking.
//! - `convert` -- bridge-struct <-> Rapier math/enum conversions.
//! - `world2d` / `world3d` -- each world's struct + every free function that
//!   operates on it (2D is currently unused by any game-side system, kept at
//!   parity with 3D -- see engine/physics/README.md).
//! - `character_controller` -- the kinematic character controller, split out
//!   from world3d since it's the meatiest, most distinct piece of logic here.
//!
//! Every implementation function is `pub(crate)`/module-private except where
//! cxx's generated glue needs to name it directly (see the curated `use`
//! list below) -- nothing in this crate has external Rust consumers, it's
//! only ever reached through the generated C++ bindings.
mod character_controller;
mod convert;
mod handles;
mod raycast3d;
#[cfg(test)]
mod tests;
mod world2d;
mod world3d;

#[cfg(test)]
use handles::{KIND_DYNAMIC, KIND_FIXED};

use character_controller::move_character_3d;
use raycast3d::raycast_3d;
use world2d::{
    attach_box_2d, attach_capsule_2d, attach_circle_2d, body_exists_2d, body_pose_2d,
    body_poses_2d, clear_contact_events_2d, clear_contact_force_events_2d, collider_exists_2d,
    contact_event_2d, contact_event_count_2d, contact_force_event_2d, contact_force_event_count_2d,
    create_body_2d, last_step_stats_2d, new_world_2d, raycast_2d, remove_body_2d,
    set_body_linear_velocity_2d, set_body_position_2d, set_gravity_2d, step_world_2d,
    RapierWorld2D,
};
use world3d::{
    attach_ball_3d, attach_capsule_3d, attach_cuboid_3d, body_exists_3d, body_pose_3d,
    body_poses_3d, clear_contact_events_3d, clear_contact_force_events_3d, collider_exists_3d,
    contact_event_3d, contact_event_count_3d, contact_force_event_3d, contact_force_event_count_3d,
    create_body_3d, last_step_stats_3d, new_world_3d, remove_body_3d, set_body_linear_velocity_3d,
    set_body_position_3d, set_gravity_3d, step_world_3d, RapierWorld3D,
};

// cxx's #[cxx::bridge] macro processes this mod's items directly and doesn't
// expand an include! placed inside it (confirmed: "unsupported item") -- the
// struct/fn-signature contract below has to stay literally in this block
// rather than living in its own file. Implementations live in handles.rs/
// convert.rs/world2d.rs/world3d.rs/character_controller.rs (see the module
// doc comment above) and are wired in via the curated `use` list above,
// which is what `super::name` in this macro's generated glue resolves to.
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

    // Config for rapier3d::control::KinematicCharacterController -- a
    // stateless per-call config struct, not persistent per-body state (see
    // move_character_3d below). autostep_max_height <= 0.0 disables autostep
    // entirely (it defaults off in Rapier itself -- "a very computationally
    // expensive feature" per Rapier's own doc comment); snap_to_ground <= 0.0
    // disables ground-snapping.
    struct BridgeCharacterControllerDesc {
        up: BridgeVec3,
        offset: f32,
        slide: bool,
        max_slope_climb_angle: f32,
        min_slope_slide_angle: f32,
        snap_to_ground: f32,
        autostep_max_height: f32,
        autostep_min_width: f32,
        autostep_include_dynamic_bodies: bool,
        normal_nudge_factor: f32,
    }

    struct BridgeCharacterMovement {
        valid: bool,
        translation: BridgeVec3,
        grounded: bool,
        is_sliding_down_slope: bool,
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

        fn move_character_3d(
            world: &RapierWorld3D,
            collider: u64,
            exclude_body: u64,
            position: BridgeVec3,
            desired_translation: BridgeVec3,
            dt: f32,
            desc: BridgeCharacterControllerDesc,
        ) -> BridgeCharacterMovement;

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
