//! First-person yaw/pitch/head-bob state -- a straight port of `engine/
//! character/FirstPersonCamera.hpp/.cpp`'s own math (see that file for the
//! research rationale on bob amplitude/frequency and the pitch clamp range).
//! Deliberately polls no input itself, same as the C++ original: the owning
//! system (see `session.rs`) reads accumulated mouse delta once per frame
//! and calls `add_look_delta`; movement/physics stay entirely separate.
#![forbid(unsafe_code)]

use bevy::math::{Quat, Vec3};
use bevy::prelude::Resource;

// Radians; kept just short of +/-90 degrees to avoid the view flipping
// through the pole -- same constant as FirstPersonCamera::kMaxPitchRadians.
const MAX_PITCH_RADIANS: f32 = 1.55334;

const MOUSE_SENSITIVITY: f32 = 0.0025;

#[derive(Resource, Default)]
pub(crate) struct FirstPersonCamera {
    yaw: f32,
    pitch: f32,
    bob_phase: f32,
}

impl FirstPersonCamera {
    // Raw mouse delta is already a per-frame distance, not a rate -- do not
    // scale by dt (that would double-integrate time and make turn rate
    // framerate-dependent). Yaw/pitch are owned scalars, not derived from
    // position/target vectors, sidestepping gimbal lock entirely.
    pub(crate) fn add_look_delta(&mut self, delta_x: f32, delta_y: f32) {
        self.yaw -= delta_x * MOUSE_SENSITIVITY;
        self.pitch = (self.pitch - delta_y * MOUSE_SENSITIVITY).clamp(-MAX_PITCH_RADIANS, MAX_PITCH_RADIANS);
    }

    // Phase advances with distance traveled, not wall-clock time, so bob
    // frequency naturally scales with movement speed instead of needing a
    // separate speed->frequency curve.
    pub(crate) fn update_bob(&mut self, horizontal_speed: f32, dt: f32) {
        const BOB_CYCLES_PER_METER: f32 = 1.6;
        self.bob_phase += horizontal_speed * dt * BOB_CYCLES_PER_METER * std::f32::consts::TAU;
        if self.bob_phase > std::f32::consts::TAU {
            self.bob_phase %= std::f32::consts::TAU;
        }
    }

    pub(crate) fn yaw(&self) -> f32 {
        self.yaw
    }

    pub(crate) fn forward(&self) -> Vec3 {
        Vec3::new(self.pitch.cos() * self.yaw.sin(), self.pitch.sin(), self.pitch.cos() * self.yaw.cos())
    }

    // Amplitudes are small fractions of eye height by design -- meant to
    // read as "alive," not as a visible wobble.
    pub(crate) fn bob_offset(&self) -> Vec3 {
        const VERTICAL_AMPLITUDE: f32 = 0.015;
        const LATERAL_AMPLITUDE: f32 = 0.008;
        Vec3::new(
            (self.bob_phase * 0.5).cos() * LATERAL_AMPLITUDE,
            (self.bob_phase.sin()).abs() * VERTICAL_AMPLITUDE,
            0.0,
        )
    }

    // The rotation for a Bevy camera Transform whose local -Z (Bevy's own
    // forward convention) points along this yaw/pitch pair's forward()
    // direction. Built via looking_to rather than hand-derived Euler angles:
    // that sidesteps needing forward()'s raylib-convention yaw/pitch signs
    // to independently match whatever handedness Quat::from_euler happens
    // to use -- looking_to is unambiguous for any given direction vector.
    pub(crate) fn rotation(&self) -> Quat {
        bevy::transform::components::Transform::IDENTITY
            .looking_to(self.forward(), Vec3::Y)
            .rotation
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn pitch_clamps_to_the_max_range_even_under_extreme_input() {
        let mut camera = FirstPersonCamera::default();
        // A huge single delta_y should still land exactly at the clamp, not
        // overshoot it -- mirrors FirstPersonCamera::addLookDelta's own
        // std::clamp call.
        camera.add_look_delta(0.0, 1.0e6);
        assert!(
            (-MAX_PITCH_RADIANS..=MAX_PITCH_RADIANS).contains(&camera.pitch),
            "pitch {} exceeded the clamp range",
            camera.pitch
        );
        camera.add_look_delta(0.0, -1.0e6);
        assert!(
            (-MAX_PITCH_RADIANS..=MAX_PITCH_RADIANS).contains(&camera.pitch),
            "pitch {} exceeded the clamp range",
            camera.pitch
        );
    }

    // The one property that actually matters for rotation(): whatever
    // direction forward() reports, applying rotation() to Bevy's own local
    // forward axis (-Z) must reproduce it -- this is what a session.rs bug
    // (a hand-derived Euler-angle rotation that quietly didn't match
    // forward()'s raylib-convention yaw/pitch signs) would have broken
    // silently, since nothing else in this crate cross-checks the two.
    #[test]
    fn rotation_reproduces_forward_for_a_range_of_yaw_and_pitch() {
        for yaw_deg in [-170.0_f32, -90.0, -37.0, 0.0, 42.0, 90.0, 165.0] {
            for pitch_deg in [-80.0_f32, -30.0, 0.0, 25.0, 80.0] {
                let mut camera = FirstPersonCamera::default();
                // Drive yaw/pitch directly to exact values rather than via
                // add_look_delta's sensitivity scaling, so the test isn't
                // coupled to that constant.
                camera.yaw = yaw_deg.to_radians();
                camera.pitch = pitch_deg.to_radians();

                let expected = camera.forward();
                let actual = camera.rotation() * Vec3::NEG_Z;
                assert!(
                    expected.distance(actual) < 1.0e-4,
                    "yaw={yaw_deg} pitch={pitch_deg}: rotation() * -Z = {actual:?}, expected forward() = {expected:?}"
                );
            }
        }
    }

    #[test]
    fn bob_offset_is_zero_at_rest() {
        let camera = FirstPersonCamera::default();
        assert_eq!(camera.bob_offset(), Vec3::new(0.008, 0.0, 0.0), "cos(0)=1, sin(0)=0 at bob_phase=0");
    }
}
