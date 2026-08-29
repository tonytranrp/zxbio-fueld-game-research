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
