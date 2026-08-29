// Bridge-struct <-> Rapier math/enum conversions. pub(crate) throughout --
// internal plumbing shared by world2d/world3d/character_controller. The
// actual numeric conversion goes through biofuel_engine_utils::convert
// (Engine/Utils/) -- game/ needs the identical Vec3/Quat <-> nalgebra
// logic for its own bridge types.
use crate::ffi;
use crate::handles::{KIND_DYNAMIC, KIND_FIXED, KIND_KINEMATIC_POSITION, KIND_KINEMATIC_VELOCITY};
use biofuel_engine_utils::convert as util_convert;
use rapier2d::prelude as r2;
use rapier3d::prelude as r3;

pub(crate) fn vec2(value: ffi::BridgeVec2) -> r2::Vector {
    util_convert::vector2(value.x, value.y)
}

pub(crate) fn vec3(value: ffi::BridgeVec3) -> r3::Vector {
    util_convert::vector3(value.x, value.y, value.z)
}

pub(crate) fn point2(value: &ffi::BridgeVec2) -> r2::Vector {
    util_convert::vector2(value.x, value.y)
}

pub(crate) fn point3(value: &ffi::BridgeVec3) -> r3::Vector {
    util_convert::vector3(value.x, value.y, value.z)
}

pub(crate) fn bridge_vec2(value: r2::Vector) -> ffi::BridgeVec2 {
    let (x, y) = util_convert::xy(value);
    ffi::BridgeVec2 { x, y }
}

pub(crate) fn bridge_vec3(value: r3::Vector) -> ffi::BridgeVec3 {
    let (x, y, z) = util_convert::xyz(value);
    ffi::BridgeVec3 { x, y, z }
}

pub(crate) fn body_type(kind: u8) -> r2::RigidBodyType {
    match kind {
        KIND_FIXED => r2::RigidBodyType::Fixed,
        KIND_DYNAMIC => r2::RigidBodyType::Dynamic,
        KIND_KINEMATIC_POSITION => r2::RigidBodyType::KinematicPositionBased,
        KIND_KINEMATIC_VELOCITY => r2::RigidBodyType::KinematicVelocityBased,
        _ => r2::RigidBodyType::Fixed,
    }
}

pub(crate) fn body_type_3d(kind: u8) -> r3::RigidBodyType {
    match kind {
        KIND_FIXED => r3::RigidBodyType::Fixed,
        KIND_DYNAMIC => r3::RigidBodyType::Dynamic,
        KIND_KINEMATIC_POSITION => r3::RigidBodyType::KinematicPositionBased,
        KIND_KINEMATIC_VELOCITY => r3::RigidBodyType::KinematicVelocityBased,
        _ => r3::RigidBodyType::Fixed,
    }
}
