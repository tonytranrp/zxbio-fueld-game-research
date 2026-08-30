//! Kinematic capsule player controller -- a straight port of `engine/
//! character/CharacterController3D.hpp/.cpp`'s own Source-engine-style
//! accelerate/friction ground movement (see that file for the research
//! rationale: reach top speed in ~0.1s via an accelerate-toward-wishspeed
//! model, then a two-regime friction curve that avoids the "ice skating"
//! feel a naive velocity model gets) on top of Rapier's kinematic character
//! controller (`RapierPhysics::move_character`, ported alongside it in
//! `physics.rs`).
#![forbid(unsafe_code)]

use crate::physics::RapierPhysics;
use bevy::prelude::Resource;
use rapier3d::prelude as r3;

pub(crate) struct Config {
    pub(crate) walk_speed: f32,          // m/s
    pub(crate) sprint_speed: f32,        // m/s
    pub(crate) ground_acceleration: f32, // m/s^2 -- reaches walk_speed in ~0.1s
    pub(crate) ground_friction: f32,     // m/s^2 flat deceleration below stop_speed
    pub(crate) stop_speed: f32,          // m/s -- below this, friction is flat, not exponential
    pub(crate) jump_speed: f32,          // m/s, applied once on jump
    pub(crate) gravity: f32,             // m/s^2
    pub(crate) coyote_time_seconds: f32,
    pub(crate) capsule_half_height: f32, // cylinder half-height; total height = 2*(half+radius)
    pub(crate) capsule_radius: f32,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            walk_speed: 4.0,
            sprint_speed: 7.0,
            ground_acceleration: 40.0,
            ground_friction: 10.0,
            stop_speed: 1.0,
            jump_speed: 5.0,
            gravity: 9.8,
            coyote_time_seconds: 0.12,
            capsule_half_height: 0.5,
            capsule_radius: 0.35,
        }
    }
}

// Per-frame movement input. move_axis is local to the look direction (x =
// strafe right, y = forward), typically from WASD, and is NOT required to
// be normalized -- step() clamps its length itself so diagonal movement
// isn't faster than cardinal movement.
pub(crate) struct MoveInput {
    pub(crate) move_axis: (f32, f32),
    pub(crate) yaw_radians: f32,
    pub(crate) sprint: bool,
    pub(crate) jump: bool,
}

#[derive(Resource)]
pub(crate) struct PlayerController {
    body: r3::RigidBodyHandle,
    collider: r3::ColliderHandle,
    position: r3::Vector,
    horizontal_velocity: r3::Vector, // y always 0
    vertical_velocity: f32,
    grounded: bool,
    coyote_timer: f32,
    config: Config,
}

impl PlayerController {
    pub(crate) fn spawn(physics: &mut RapierPhysics, start_position: r3::Vector, config: Config) -> Self {
        let body = r3::RigidBodyBuilder::new(r3::RigidBodyType::KinematicPositionBased)
            .translation(start_position)
            .build();
        let handle = physics.bodies.insert(body);
        let collider = r3::ColliderBuilder::capsule_y(config.capsule_half_height, config.capsule_radius);
        let collider_handle = physics
            .colliders
            .insert_with_parent(collider, handle, &mut physics.bodies);

        Self {
            body: handle,
            collider: collider_handle,
            position: start_position,
            horizontal_velocity: r3::Vector::new(0.0, 0.0, 0.0),
            vertical_velocity: 0.0,
            grounded: false,
            coyote_timer: 0.0,
            config,
        }
    }

    pub(crate) fn position(&self) -> r3::Vector {
        self.position
    }

    pub(crate) fn eye_height(&self) -> f32 {
        self.config.capsule_half_height + self.config.capsule_radius
    }

    // Not read yet -- the C++ original (CharacterController3D::grounded())
    // feeds ExplorationScreen's walk/idle viewmodel-animation switch, which
    // is Phase 4's scope (porting the viewmodel hands), not this phase's.
    // Kept now so that port doesn't need to add this accessor too.
    #[allow(dead_code)]
    pub(crate) fn grounded(&self) -> bool {
        self.grounded
    }

    pub(crate) fn horizontal_speed(&self) -> f32 {
        (self.horizontal_velocity.x * self.horizontal_velocity.x
            + self.horizontal_velocity.z * self.horizontal_velocity.z)
            .sqrt()
    }

    pub(crate) fn step(&mut self, physics: &RapierPhysics, input: &MoveInput, dt: f32) {
        // 1. Wish direction in world space from local input (x=strafe, y=forward)
        // rotated by camera yaw -- matches FirstPersonCamera's right()/
        // flatForward() convention (right = {-cosYaw, 0, sinYaw},
        // flatForward = {sinYaw, 0, cosYaw}).
        let (mut ax, mut ay) = input.move_axis;
        let axis_len_sq = ax * ax + ay * ay;
        if axis_len_sq > 1.0 {
            let axis_len = axis_len_sq.sqrt();
            ax /= axis_len;
            ay /= axis_len;
        }
        let axis_len = (ax * ax + ay * ay).sqrt();
        let cos_yaw = input.yaw_radians.cos();
        let sin_yaw = input.yaw_radians.sin();
        let wish_dir_raw = r3::Vector::new(-cos_yaw * ax + sin_yaw * ay, 0.0, sin_yaw * ax + cos_yaw * ay);
        let wish_dir = if axis_len > 1.0e-5 {
            r3::Vector::new(wish_dir_raw.x / axis_len, 0.0, wish_dir_raw.z / axis_len)
        } else {
            r3::Vector::new(0.0, 0.0, 0.0)
        };
        let wish_speed = (if input.sprint { self.config.sprint_speed } else { self.config.walk_speed }) * axis_len;

        // 2. Ground friction -- two-regime model (research baseline: Source
        // engine's PM_Friction): exponential decay above stop_speed, a flat
        // deceleration below it. The flat floor is what actually kills the
        // "ice skating" feel a pure-exponential decay leaves forever
        // approaching zero.
        let current_speed = self.horizontal_speed();
        if current_speed > 1.0e-4 {
            let control = if current_speed < self.config.stop_speed { self.config.stop_speed } else { current_speed };
            let drop = control * self.config.ground_friction * dt;
            let new_speed = (current_speed - drop).max(0.0);
            let scale = new_speed / current_speed;
            self.horizontal_velocity.x *= scale;
            self.horizontal_velocity.z *= scale;
        }

        // 3. Accelerate toward wish_dir*wish_speed, capped so it never
        // overshoots (research baseline: reaches wish_speed in ~0.1s
        // regardless of what that speed is, since the increment itself
        // scales with wish_speed).
        if wish_speed > 0.0 {
            let current_speed_in_wish_dir =
                self.horizontal_velocity.x * wish_dir.x + self.horizontal_velocity.z * wish_dir.z;
            let add_speed = wish_speed - current_speed_in_wish_dir;
            if add_speed > 0.0 {
                let accel_speed = (self.config.ground_acceleration * dt * wish_speed).min(add_speed);
                self.horizontal_velocity.x += wish_dir.x * accel_speed;
                self.horizontal_velocity.z += wish_dir.z * accel_speed;
            }
        }

        // 4. Vertical: gravity + jump, with coyote time. Reads last step's
        // grounded (this step's move hasn't resolved yet) -- updated at the
        // end of this function for the *next* call to read.
        if self.grounded {
            self.coyote_timer = self.config.coyote_time_seconds;
            if !input.jump {
                self.vertical_velocity = 0.0;
            }
        } else {
            self.coyote_timer = (self.coyote_timer - dt).max(0.0);
        }
        if input.jump && (self.grounded || self.coyote_timer > 0.0) {
            self.vertical_velocity = self.config.jump_speed;
            self.grounded = false;
            self.coyote_timer = 0.0;
        } else if !self.grounded {
            self.vertical_velocity -= self.config.gravity * dt;
        }

        // 5. Resolve the move through the kinematic character controller.
        let desired_translation = r3::Vector::new(
            self.horizontal_velocity.x * dt,
            self.vertical_velocity * dt,
            self.horizontal_velocity.z * dt,
        );
        // Only snap while already grounded -- snapping every tick would glue
        // a jumping/falling character straight back onto the ground.
        let snap_to_ground = if self.grounded { 0.2 } else { 0.0 };
        if let Some(result) =
            physics.move_character(self.collider, self.body, self.position, desired_translation, dt, snap_to_ground)
        {
            self.position.x += result.translation.x;
            self.position.y += result.translation.y;
            self.position.z += result.translation.z;
            self.grounded = result.grounded;
        }
    }

    // Kinematic bodies move by having their position set directly, then
    // Rapier resolves collisions against it next step() -- same
    // `set_translation(_, wake=true)` call
    // `Engine/physics/src/world3d.rs::set_body_position_3d` makes for the
    // C++-facing PhysicsWorld3D::setBodyPosition, which the real (proven,
    // shipped) ExplorationScreen/CharacterController3D path already relies
    // on. Separate from step() so the caller (session.rs's Update system)
    // controls exactly when this write happens relative to physics.step().
    pub(crate) fn sync_body_position(&self, physics: &mut RapierPhysics) {
        if let Some(body) = physics.bodies.get_mut(self.body) {
            body.set_translation(self.position, true);
        }
    }
}
