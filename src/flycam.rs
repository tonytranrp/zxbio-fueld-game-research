//! [`VoxelFlycamPlugin`] — a simple WASD + mouse-look debug camera, built on Bevy's own stock
//! keyboard/mouse input (`ButtonInput<KeyCode>`, `MouseMotion`) rather than a hand-rolled input
//! layer, since nothing here needs to work around any input-delivery constraint.

use bevy::input::mouse::MouseMotion;
use bevy::math::EulerRot;
use bevy::prelude::*;

/// Marks the entity a [`VoxelFlycamPlugin`] system drives, and holds its yaw/pitch state.
///
/// Add this to a `Camera3d` entity you spawn yourself — the plugin does not spawn a camera for
/// you, since a consumer may want to configure its projection, clear color, or render order.
#[derive(Component)]
pub struct VoxelFlycam {
    pub yaw: f32,
    pub pitch: f32,
    pub move_speed: f32,
    pub look_sensitivity: f32,
}

impl Default for VoxelFlycam {
    fn default() -> Self {
        Self {
            yaw: 0.0,
            pitch: 0.0,
            move_speed: 8.0,
            look_sensitivity: 0.0025,
        }
    }
}

/// Adds WASD (+ Space/Shift for up/down) + mouse-look movement to any entity with a
/// [`VoxelFlycam`] component and a `Transform`.
pub struct VoxelFlycamPlugin;

impl Plugin for VoxelFlycamPlugin {
    fn build(&self, app: &mut App) {
        app.add_systems(Update, (look, movement));
    }
}

fn look(mut motion: MessageReader<MouseMotion>, mut query: Query<(&mut VoxelFlycam, &mut Transform)>) {
    let mut delta = Vec2::ZERO;
    for event in motion.read() {
        delta += event.delta;
    }
    if delta == Vec2::ZERO {
        return;
    }

    for (mut cam, mut transform) in &mut query {
        cam.yaw -= delta.x * cam.look_sensitivity;
        cam.pitch = (cam.pitch - delta.y * cam.look_sensitivity)
            .clamp(-std::f32::consts::FRAC_PI_2 + 0.01, std::f32::consts::FRAC_PI_2 - 0.01);
        transform.rotation = Quat::from_euler(EulerRot::YXZ, cam.yaw, cam.pitch, 0.0);
    }
}

fn movement(keys: Res<ButtonInput<KeyCode>>, time: Res<Time>, mut query: Query<(&VoxelFlycam, &mut Transform)>) {
    let dt = time.delta_secs();
    for (cam, mut transform) in &mut query {
        let forward = *transform.forward();
        let right = *transform.right();
        let mut direction = Vec3::ZERO;
        if keys.pressed(KeyCode::KeyW) {
            direction += forward;
        }
        if keys.pressed(KeyCode::KeyS) {
            direction -= forward;
        }
        if keys.pressed(KeyCode::KeyD) {
            direction += right;
        }
        if keys.pressed(KeyCode::KeyA) {
            direction -= right;
        }
        if keys.pressed(KeyCode::Space) {
            direction += Vec3::Y;
        }
        if keys.pressed(KeyCode::ShiftLeft) {
            direction -= Vec3::Y;
        }

        if direction != Vec3::ZERO {
            transform.translation += direction.normalize() * cam.move_speed * dt;
        }
    }
}
