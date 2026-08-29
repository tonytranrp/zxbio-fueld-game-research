// Opaque-handle packing: turns a Rapier (index, generation) pair into the
// single u64 the C++ side stores, and back. pub(crate) throughout -- these
// are internal plumbing shared by world2d/world3d/character_controller, never
// part of the crate's cxx-facing surface. The actual pack/unpack scheme lives
// in biofuel_engine_utils (Engine/Utils/) since game/ needs the identical
// logic for its own entity handles -- this file just adapts it to Rapier's
// own handle types.
use biofuel_engine_utils::handles::{pack_handle, unpack_handle};
use rapier2d::prelude as r2;
use rapier3d::prelude as r3;

pub(crate) const KIND_FIXED: u8 = 0;
pub(crate) const KIND_DYNAMIC: u8 = 1;
pub(crate) const KIND_KINEMATIC_POSITION: u8 = 2;
pub(crate) const KIND_KINEMATIC_VELOCITY: u8 = 3;
pub(crate) const PHASE_STARTED: u8 = 0;
pub(crate) const PHASE_ENDED: u8 = 1;

pub(crate) fn pack_body_2d(handle: r2::RigidBodyHandle) -> u64 {
    let (index, generation) = handle.into_raw_parts();
    pack_handle(index, generation)
}

pub(crate) fn pack_body_3d(handle: r3::RigidBodyHandle) -> u64 {
    let (index, generation) = handle.into_raw_parts();
    pack_handle(index, generation)
}

pub(crate) fn pack_collider_2d(handle: r2::ColliderHandle) -> u64 {
    let (index, generation) = handle.into_raw_parts();
    pack_handle(index, generation)
}

pub(crate) fn pack_collider_3d(handle: r3::ColliderHandle) -> u64 {
    let (index, generation) = handle.into_raw_parts();
    pack_handle(index, generation)
}

pub(crate) fn unpack_body_2d(raw: u64) -> r2::RigidBodyHandle {
    let (index, generation) = unpack_handle(raw);
    r2::RigidBodyHandle::from_raw_parts(index, generation)
}

pub(crate) fn unpack_body_3d(raw: u64) -> r3::RigidBodyHandle {
    let (index, generation) = unpack_handle(raw);
    r3::RigidBodyHandle::from_raw_parts(index, generation)
}

pub(crate) fn unpack_collider_2d(raw: u64) -> r2::ColliderHandle {
    let (index, generation) = unpack_handle(raw);
    r2::ColliderHandle::from_raw_parts(index, generation)
}

pub(crate) fn unpack_collider_3d(raw: u64) -> r3::ColliderHandle {
    let (index, generation) = unpack_handle(raw);
    r3::ColliderHandle::from_raw_parts(index, generation)
}

// Every collider dimension (half-extent, radius, half-height) crossing the
// FFI boundary from C++ passes through this before reaching a Rapier
// ColliderBuilder. Rapier's builders are not guaranteed panic-free on a
// non-finite or non-positive dimension, and this crate's whole design goal
// is zero panics reachable from C++ input -- so a bad value from the C++
// side is silently clamped to a small sane default instead of ever reaching
// Rapier's shape construction.
pub(crate) fn safe_positive(value: f32, fallback: f32) -> f32 {
    if value.is_finite() && value > 0.0 {
        value
    } else {
        fallback
    }
}
