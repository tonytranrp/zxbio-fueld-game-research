//! Per-session input state. `winit::application::ApplicationHandler` is
//! event-driven (`WindowEvent::KeyboardInput`/`DeviceEvent::MouseMotion`
//! callbacks) rather than poll-based like raylib's `IsKeyDown`/
//! `GetMouseDelta`, which the C++ movement/camera code this crate ports
//! (`CharacterController3D`, `FirstPersonCamera`) was written against --
//! this resource accumulates both event streams into the same
//! poll-once-per-frame shape those ports expect, so the ported math doesn't
//! need to change to account for the different input model.
#![forbid(unsafe_code)]

use bevy::prelude::Resource;
use std::collections::HashSet;
use winit::keyboard::KeyCode;

#[derive(Resource, Default)]
pub(crate) struct InputState {
    held_keys: HashSet<KeyCode>,
    mouse_delta: (f32, f32),
    // Edge-triggered (consumed on read), not a held-state set like
    // held_keys -- a harvest-trigger click should fire once per physical
    // click, not once per frame the button happens to be down.
    left_click_this_frame: bool,
}

impl InputState {
    pub(crate) fn set_key(&mut self, key: KeyCode, pressed: bool) {
        if pressed {
            self.held_keys.insert(key);
        } else {
            self.held_keys.remove(&key);
        }
    }

    pub(crate) fn set_left_click(&mut self) {
        self.left_click_this_frame = true;
    }

    // Drain-and-return, same reasoning as take_mouse_delta: called once per
    // Update so a click reported between two Updates is consumed exactly
    // once. Read by fuel.rs's own harvest-trigger system.
    pub(crate) fn take_left_click(&mut self) -> bool {
        std::mem::take(&mut self.left_click_this_frame)
    }

    pub(crate) fn accumulate_mouse_delta(&mut self, dx: f32, dy: f32) {
        self.mouse_delta.0 += dx;
        self.mouse_delta.1 += dy;
    }

    // Drain-and-return: called once per Update so a mouse delta reported
    // between two Updates isn't double-counted (left in place) or lost
    // (overwritten instead of accumulated).
    pub(crate) fn take_mouse_delta(&mut self) -> (f32, f32) {
        std::mem::take(&mut self.mouse_delta)
    }

    pub(crate) fn is_down(&self, key: KeyCode) -> bool {
        self.held_keys.contains(&key)
    }
}
