//! World, voxel, brick, and local coordinate conversions for the brick-grid voxel storage.
//!
//! Internal plumbing shared by [`crate::storage::chunk`] and the render module — not part of the
//! crate's public API.

use bevy::math::UVec3;

/// Voxels per brick edge. A brick is `BRICK_SIZE^3` voxels.
pub(crate) const BRICK_SIZE: u32 = 8;

/// Returns `true` if `dims` (in voxels) is a valid chunk size: nonzero and an exact multiple of
/// [`BRICK_SIZE`] on every axis.
pub(crate) fn is_valid_chunk_dims(dims: UVec3) -> bool {
    dims.x > 0
        && dims.y > 0
        && dims.z > 0
        && dims.x % BRICK_SIZE == 0
        && dims.y % BRICK_SIZE == 0
        && dims.z % BRICK_SIZE == 0
}

/// The brick-grid dimensions (in bricks) for a chunk of the given voxel dimensions.
///
/// Caller must ensure `dims` is valid (see [`is_valid_chunk_dims`]); this performs no rounding.
pub(crate) fn brick_dims(dims: UVec3) -> UVec3 {
    UVec3::new(dims.x / BRICK_SIZE, dims.y / BRICK_SIZE, dims.z / BRICK_SIZE)
}

/// Returns `true` if `voxel` is within a chunk of the given voxel `dims`.
pub(crate) fn contains_voxel(voxel: UVec3, dims: UVec3) -> bool {
    voxel.x < dims.x && voxel.y < dims.y && voxel.z < dims.z
}

/// Splits a voxel coordinate into its containing brick coordinate and its local coordinate
/// within that brick (each component in `0..BRICK_SIZE`).
pub(crate) fn split_voxel(voxel: UVec3) -> (UVec3, UVec3) {
    let brick = UVec3::new(voxel.x / BRICK_SIZE, voxel.y / BRICK_SIZE, voxel.z / BRICK_SIZE);
    let local = UVec3::new(voxel.x % BRICK_SIZE, voxel.y % BRICK_SIZE, voxel.z % BRICK_SIZE);
    (brick, local)
}

/// Flattens a 3D coordinate into a linear index within a volume of the given dimensions, using
/// x-major, then y, then z ordering: `x + y * dims.x + z * dims.x * dims.y`.
///
/// Used for both voxel-space indices (into a chunk's voxel array) and brick-space indices (into
/// its occupancy array) — the two spaces never mix within one call.
pub(crate) fn flatten(coord: UVec3, dims: UVec3) -> usize {
    (coord.x + coord.y * dims.x + coord.z * dims.x * dims.y) as usize
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn valid_chunk_dims_requires_nonzero_multiple_of_brick_size() {
        assert!(is_valid_chunk_dims(UVec3::splat(8)));
        assert!(is_valid_chunk_dims(UVec3::new(8, 16, 24)));
        assert!(!is_valid_chunk_dims(UVec3::splat(0)));
        assert!(!is_valid_chunk_dims(UVec3::splat(7)));
        assert!(!is_valid_chunk_dims(UVec3::new(8, 9, 8)));
    }

    #[test]
    fn brick_dims_divides_evenly() {
        assert_eq!(brick_dims(UVec3::splat(128)), UVec3::splat(16));
        assert_eq!(brick_dims(UVec3::new(8, 16, 24)), UVec3::new(1, 2, 3));
    }

    #[test]
    fn contains_voxel_respects_all_three_axes() {
        let dims = UVec3::new(4, 5, 6);
        assert!(contains_voxel(UVec3::new(0, 0, 0), dims));
        assert!(contains_voxel(UVec3::new(3, 4, 5), dims));
        assert!(!contains_voxel(UVec3::new(4, 0, 0), dims));
        assert!(!contains_voxel(UVec3::new(0, 5, 0), dims));
        assert!(!contains_voxel(UVec3::new(0, 0, 6), dims));
    }

    #[test]
    fn split_voxel_round_trips_at_brick_boundaries() {
        assert_eq!(split_voxel(UVec3::new(0, 0, 0)), (UVec3::ZERO, UVec3::ZERO));
        assert_eq!(
            split_voxel(UVec3::new(7, 7, 7)),
            (UVec3::ZERO, UVec3::splat(7))
        );
        assert_eq!(
            split_voxel(UVec3::new(8, 8, 8)),
            (UVec3::ONE, UVec3::ZERO)
        );
        assert_eq!(
            split_voxel(UVec3::new(17, 8, 0)),
            (UVec3::new(2, 1, 0), UVec3::new(1, 0, 0))
        );
    }

    #[test]
    fn flatten_is_x_major_then_y_then_z() {
        let dims = UVec3::new(4, 5, 6);
        assert_eq!(flatten(UVec3::new(0, 0, 0), dims), 0);
        assert_eq!(flatten(UVec3::new(1, 0, 0), dims), 1);
        assert_eq!(flatten(UVec3::new(0, 1, 0), dims), 4);
        assert_eq!(flatten(UVec3::new(0, 0, 1), dims), 20);
        assert_eq!(flatten(UVec3::new(3, 4, 5), dims), 4 * 5 * 6 - 1);
    }

    #[test]
    fn flatten_gives_distinct_indices_for_every_coordinate_in_a_small_volume() {
        let dims = UVec3::new(3, 4, 5);
        let total = (dims.x * dims.y * dims.z) as usize;
        let mut seen = vec![false; total];
        for z in 0..dims.z {
            for y in 0..dims.y {
                for x in 0..dims.x {
                    let idx = flatten(UVec3::new(x, y, z), dims);
                    assert!(!seen[idx], "index {idx} produced twice");
                    seen[idx] = true;
                }
            }
        }
        assert!(seen.iter().all(|&s| s));
    }
}
