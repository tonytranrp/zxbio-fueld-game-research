//! Coarser occupancy levels above the brick grid, letting the ray marcher skip much larger empty
//! regions in one step than brick-level occupancy alone. See `VoxelChunk`'s own `mips` field for
//! how a full hierarchy is built and kept up to date as the chunk is edited.

use bevy::math::UVec3;

use super::coords::flatten;

/// One coarser level of the occupancy mip hierarchy. Each cell groups a 2x2x2 block of cells
/// from the level below (bricks, for the first level; the previous mip level, for any level
/// after that) and is `true` if any of them is occupied.
#[derive(Debug, Clone)]
pub(crate) struct OccupancyMip {
    dims: UVec3,
    occupied: Vec<bool>,
}

impl OccupancyMip {
    /// Builds a full, fully-unoccupied mip hierarchy above a brick grid of `brick_dims` — one
    /// level per halving (rounding up on odd sizes) until a level would be `1x1x1` in every
    /// axis, since there's nothing coarser to gain past that.
    pub(crate) fn hierarchy_for(brick_dims: UVec3) -> Vec<OccupancyMip> {
        let mut levels = Vec::new();
        let mut child_dims = brick_dims;
        while child_dims.x > 1 || child_dims.y > 1 || child_dims.z > 1 {
            let dims = UVec3::new(
                child_dims.x.div_ceil(2),
                child_dims.y.div_ceil(2),
                child_dims.z.div_ceil(2),
            );
            let count = (dims.x * dims.y * dims.z) as usize;
            levels.push(OccupancyMip {
                dims,
                occupied: vec![false; count],
            });
            child_dims = dims;
        }
        levels
    }

    pub(crate) fn dims(&self) -> UVec3 {
        self.dims
    }

    /// Returns this level's occupancy at `coord`; out-of-range coordinates are unoccupied.
    pub(crate) fn get(&self, coord: UVec3) -> bool {
        if coord.x >= self.dims.x || coord.y >= self.dims.y || coord.z >= self.dims.z {
            return false;
        }
        self.occupied[flatten(coord, self.dims)]
    }

    /// Sets this level's occupancy at `coord`. Returns `true` if the value actually changed —
    /// callers use this to decide whether to keep propagating up the hierarchy.
    pub(crate) fn set(&mut self, coord: UVec3, value: bool) -> bool {
        let idx = flatten(coord, self.dims);
        if self.occupied[idx] == value {
            return false;
        }
        self.occupied[idx] = value;
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hierarchy_for_stops_once_a_level_would_be_1x1x1() {
        let levels = OccupancyMip::hierarchy_for(UVec3::splat(16));
        let dims: Vec<UVec3> = levels.iter().map(OccupancyMip::dims).collect();
        assert_eq!(
            dims,
            vec![UVec3::splat(8), UVec3::splat(4), UVec3::splat(2), UVec3::splat(1)]
        );
    }

    #[test]
    fn hierarchy_for_a_single_brick_has_no_levels() {
        assert!(OccupancyMip::hierarchy_for(UVec3::splat(1)).is_empty());
    }

    #[test]
    fn hierarchy_rounds_odd_dims_up() {
        // 3 bricks -> ceil(3/2)=2 -> ceil(2/2)=1 -> stop.
        let levels = OccupancyMip::hierarchy_for(UVec3::splat(3));
        let dims: Vec<UVec3> = levels.iter().map(OccupancyMip::dims).collect();
        assert_eq!(dims, vec![UVec3::splat(2), UVec3::splat(1)]);
    }

    #[test]
    fn new_level_is_fully_unoccupied() {
        let levels = OccupancyMip::hierarchy_for(UVec3::splat(8));
        assert!(!levels[0].get(UVec3::ZERO));
        assert!(!levels[0].get(UVec3::new(3, 3, 3)));
    }

    #[test]
    fn get_out_of_range_is_unoccupied_not_a_panic() {
        let levels = OccupancyMip::hierarchy_for(UVec3::splat(8));
        assert!(!levels[0].get(UVec3::splat(1000)));
    }

    #[test]
    fn set_reports_whether_the_value_actually_changed() {
        let mut levels = OccupancyMip::hierarchy_for(UVec3::splat(8));
        assert!(levels[0].set(UVec3::ZERO, true), "false -> true is a real change");
        assert!(!levels[0].set(UVec3::ZERO, true), "true -> true is not a change");
        assert!(levels[0].set(UVec3::ZERO, false), "true -> false is a real change");
    }

    /// `voxel_raymarch.wgsl`'s `mip_dims_at` computes a mip level's dims as a single closed-form
    /// division — `ceil(brick_dims / 2^(level+1))` — rather than `hierarchy_for`'s own iterated
    /// per-level halving, because WGSL has no fixed upper bound on a runtime loop it could use to
    /// replay the iteration at shader-compile time. The two are only equivalent because of a real
    /// number-theory identity, `ceil(ceil(n/a)/b) == ceil(n/(a*b))` for positive integers — worth
    /// verifying directly here, in Rust, where it's actually testable, rather than trusting it
    /// unverified in a shader this project has no way to unit test at all (see this project's own
    /// "cross-check anything with a determinate answer two independent ways" discipline).
    #[test]
    fn closed_form_mip_dims_matches_the_iterative_hierarchy_the_shader_cant_replay() {
        fn div_ceil(n: u32, d: u32) -> u32 {
            n.div_ceil(d)
        }
        fn closed_form_dims_at(level: usize, brick_dims: UVec3) -> UVec3 {
            let divisor = 1u32 << (level as u32 + 1);
            UVec3::new(
                div_ceil(brick_dims.x, divisor),
                div_ceil(brick_dims.y, divisor),
                div_ceil(brick_dims.z, divisor),
            )
        }

        for brick_dims in [
            UVec3::splat(16),
            UVec3::splat(3),
            UVec3::splat(1),
            UVec3::new(8, 16, 24),
            UVec3::new(5, 5, 5),
            UVec3::new(1, 32, 7),
            UVec3::splat(256),
        ] {
            let levels = OccupancyMip::hierarchy_for(brick_dims);
            for (level, mip) in levels.iter().enumerate() {
                assert_eq!(
                    closed_form_dims_at(level, brick_dims),
                    mip.dims(),
                    "brick_dims={brick_dims:?} level={level}: closed form disagrees with the \
                     iterative hierarchy"
                );
            }
        }
    }
}
