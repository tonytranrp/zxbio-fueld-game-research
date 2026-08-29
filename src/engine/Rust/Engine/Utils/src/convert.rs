//! Conversions between plain `f32` components and Rapier's own `Vector`/
//! `Rotation` math types (`rapier2d::prelude`/`rapier3d::prelude` -- these
//! resolve to parry's glam-backed `Vec2`/`Vec3`/`Quat` under the hood, not
//! nalgebra, despite Rapier separately re-exporting raw nalgebra elsewhere
//! for unrelated generic/SIMD code). Deliberately generic over any crate's
//! own cxx bridge-struct shape -- `physics/` and `game/` each declare their
//! own `BridgeVec3`/`BridgeQuat`-style types in their own `#[cxx::bridge]`
//! blocks, and convert through these functions rather than duplicating the
//! construction logic.
use rapier2d::prelude as r2;
use rapier3d::prelude as r3;

pub fn vector2(x: f32, y: f32) -> r2::Vector {
    r2::Vector::new(x, y)
}

pub fn vector3(x: f32, y: f32, z: f32) -> r3::Vector {
    r3::Vector::new(x, y, z)
}

pub fn xy(value: r2::Vector) -> (f32, f32) {
    (value.x, value.y)
}

pub fn xyz(value: r3::Vector) -> (f32, f32, f32) {
    (value.x, value.y, value.z)
}

pub fn rotation_from_xyzw(x: f32, y: f32, z: f32, w: f32) -> r3::Rotation {
    r3::Rotation::from_xyzw(x, y, z, w)
}

pub fn xyzw_from_rotation(value: r3::Rotation) -> (f32, f32, f32, f32) {
    (value.x, value.y, value.z, value.w)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn vector3_round_trips() {
        assert_eq!(xyz(vector3(1.0, 2.0, 3.0)), (1.0, 2.0, 3.0));
    }

    #[test]
    fn vector2_round_trips() {
        assert_eq!(xy(vector2(4.0, 5.0)), (4.0, 5.0));
    }

    #[test]
    fn identity_rotation_round_trips() {
        let identity = r3::Rotation::IDENTITY;
        let (x, y, z, w) = xyzw_from_rotation(identity);
        assert_eq!((x, y, z, w), (0.0, 0.0, 0.0, 1.0));
        assert_eq!(rotation_from_xyzw(x, y, z, w), identity);
    }
}
