// Kinematic character controller: the sole consumer of Rapier's
// control::KinematicCharacterController in this crate. Split out from
// world3d.rs because it's the meatiest, most distinct single piece of logic
// here, and the one most likely to grow (autostep tuning, slope handling)
// independently of plain body/collider CRUD.
use crate::convert::{bridge_vec3, vec3};
use crate::ffi;
use crate::handles::{unpack_body_3d, unpack_collider_3d};
use crate::world3d::RapierWorld3D;
use rapier3d::control::{CharacterAutostep, CharacterLength, KinematicCharacterController};
use rapier3d::prelude as r3;

pub fn move_character_3d(
    world: &RapierWorld3D,
    collider: u64,
    exclude_body: u64,
    position: ffi::BridgeVec3,
    desired_translation: ffi::BridgeVec3,
    dt: f32,
    desc: ffi::BridgeCharacterControllerDesc,
) -> ffi::BridgeCharacterMovement {
    let invalid = ffi::BridgeCharacterMovement {
        valid: false,
        translation: ffi::BridgeVec3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        grounded: false,
        is_sliding_down_slope: false,
    };

    if !dt.is_finite() || dt <= 0.0 {
        return invalid;
    }
    let pos_vec = vec3(position);
    let desired_vec = vec3(desired_translation);
    if !pos_vec.x.is_finite()
        || !pos_vec.y.is_finite()
        || !pos_vec.z.is_finite()
        || !desired_vec.x.is_finite()
        || !desired_vec.y.is_finite()
        || !desired_vec.z.is_finite()
        || desc.offset <= 0.0
    {
        return invalid;
    }

    let Some(col) = world.colliders.get(unpack_collider_3d(collider)) else {
        return invalid;
    };
    let character_shape = col.shape();
    let character_pos = r3::Pose::from_translation(pos_vec);

    let mut filter = r3::QueryFilter::default();
    if exclude_body != 0 {
        filter = filter.exclude_rigid_body(unpack_body_3d(exclude_body));
    }
    let query = world.broad_phase.as_query_pipeline(
        world.narrow_phase.query_dispatcher(),
        &world.bodies,
        &world.colliders,
        filter,
    );

    let controller = KinematicCharacterController {
        up: r3::Vector::new(desc.up.x, desc.up.y, desc.up.z),
        offset: CharacterLength::Absolute(desc.offset),
        slide: desc.slide,
        autostep: if desc.autostep_max_height > 0.0 {
            Some(CharacterAutostep {
                max_height: CharacterLength::Absolute(desc.autostep_max_height),
                min_width: CharacterLength::Absolute(desc.autostep_min_width),
                include_dynamic_bodies: desc.autostep_include_dynamic_bodies,
            })
        } else {
            None
        },
        max_slope_climb_angle: desc.max_slope_climb_angle,
        min_slope_slide_angle: desc.min_slope_slide_angle,
        snap_to_ground: if desc.snap_to_ground > 0.0 {
            Some(CharacterLength::Absolute(desc.snap_to_ground))
        } else {
            None
        },
        normal_nudge_factor: desc.normal_nudge_factor,
    };

    let movement = controller.move_shape(
        dt,
        &query,
        character_shape,
        &character_pos,
        desired_vec,
        |_collision| {},
    );

    ffi::BridgeCharacterMovement {
        valid: true,
        translation: bridge_vec3(movement.translation),
        grounded: movement.grounded,
        is_sliding_down_slope: movement.is_sliding_down_slope,
    }
}
