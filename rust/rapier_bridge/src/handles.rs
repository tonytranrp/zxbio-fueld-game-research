// Opaque-handle packing: turns a Rapier (index, generation) pair into the
// single u64 the C++ side stores, and back. pub(crate) throughout -- these
// are internal plumbing shared by world2d/world3d/character_controller, never
// part of the crate's cxx-facing surface.
use rapier2d::prelude as r2;
use rapier3d::prelude as r3;

pub(crate) const KIND_FIXED: u8 = 0;
pub(crate) const KIND_DYNAMIC: u8 = 1;
pub(crate) const KIND_KINEMATIC_POSITION: u8 = 2;
pub(crate) const KIND_KINEMATIC_VELOCITY: u8 = 3;
pub(crate) const PHASE_STARTED: u8 = 0;
pub(crate) const PHASE_ENDED: u8 = 1;

fn pack_handle(index: u32, generation: u32) -> u64 {
    (((generation as u64) << 32) | index as u64) + 1
}

fn unpack_handle(raw: u64) -> (u32, u32) {
    if raw == 0 {
        return (u32::MAX, u32::MAX);
    }
    let adjusted = raw - 1;
    (adjusted as u32, (adjusted >> 32) as u32)
}

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

pub(crate) fn safe_positive(value: f32, fallback: f32) -> f32 {
    if value.is_finite() && value > 0.0 {
        value
    } else {
        fallback
    }
}
