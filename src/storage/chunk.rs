//! The `VoxelChunk` dense brick-grid voxel store.

use bevy::math::UVec3;

use super::coords::{brick_dims, contains_voxel, flatten, is_valid_chunk_dims, split_voxel, BRICK_SIZE};
use super::mip::OccupancyMip;
use super::voxel::VoxelId;

/// A dense grid of voxels backed by per-brick occupancy counts (plus a coarser mip hierarchy
/// above them) for fast empty-space skipping during ray marching. Chunk dimensions (in voxels)
/// must be a nonzero multiple of the brick size (8) on every axis.
#[derive(Debug, Clone)]
pub struct VoxelChunk {
    dims: UVec3,
    brick_dims: UVec3,
    voxels: Vec<VoxelId>,
    /// Count of non-air voxels in each brick, indexed the same way as `voxels` but over the
    /// brick grid. `0` means the brick is fully empty and can be skipped entirely.
    brick_occupancy: Vec<u16>,
    /// Coarser occupancy levels above the brick grid: `mips[0]` groups 2x2x2 bricks, `mips[1]`
    /// groups 2x2x2 of `mips[0]`'s own cells, and so on until a level would be `1x1x1`. Lets the
    /// ray marcher skip much larger empty regions in one step than brick occupancy alone —
    /// exactly Teardown's own documented technique (a base occupancy grid plus explicit coarser
    /// mips), rather than a full sparse voxel octree/DAG (see the engine's own scale-research
    /// notes for why that ordering was chosen).
    mips: Vec<OccupancyMip>,
}

impl VoxelChunk {
    /// Creates a new, fully-air chunk of the given voxel dimensions.
    ///
    /// # Panics
    /// Panics if `dims` is not a nonzero multiple of the brick size (8) on every axis.
    pub fn new(dims: UVec3) -> Self {
        assert!(
            is_valid_chunk_dims(dims),
            "VoxelChunk dims {dims:?} must be a nonzero multiple of {BRICK_SIZE} on every axis"
        );
        let brick_dims = brick_dims(dims);
        let voxel_count = (dims.x * dims.y * dims.z) as usize;
        let brick_count = (brick_dims.x * brick_dims.y * brick_dims.z) as usize;
        Self {
            dims,
            brick_dims,
            voxels: vec![VoxelId::AIR; voxel_count],
            brick_occupancy: vec![0; brick_count],
            mips: OccupancyMip::hierarchy_for(brick_dims),
        }
    }

    /// The chunk's dimensions, in voxels.
    pub fn dims(&self) -> UVec3 {
        self.dims
    }

    /// The chunk's dimensions, in bricks.
    pub(crate) fn brick_dims(&self) -> UVec3 {
        self.brick_dims
    }

    /// Returns the voxel at `pos`, or [`VoxelId::AIR`] if `pos` is outside the chunk.
    pub fn get(&self, pos: UVec3) -> VoxelId {
        if !contains_voxel(pos, self.dims) {
            return VoxelId::AIR;
        }
        self.voxels[flatten(pos, self.dims)]
    }

    /// Sets the voxel at `pos`. A `pos` outside the chunk is silently ignored, mirroring
    /// [`Self::get`]'s treatment of out-of-bounds coordinates as "always air."
    pub fn set(&mut self, pos: UVec3, id: VoxelId) {
        if !contains_voxel(pos, self.dims) {
            return;
        }
        let idx = flatten(pos, self.dims);
        let old = self.voxels[idx];
        if old == id {
            return;
        }
        self.voxels[idx] = id;

        let (brick, _local) = split_voxel(pos);
        let b_idx = flatten(brick, self.brick_dims);
        match (old.is_air(), id.is_air()) {
            (true, false) => {
                self.brick_occupancy[b_idx] += 1;
                self.update_mips(brick);
            }
            (false, true) => {
                self.brick_occupancy[b_idx] -= 1;
                self.update_mips(brick);
            }
            _ => {}
        }
    }

    /// Recomputes the mip hierarchy starting from the brick at `brick_coord`, propagating
    /// upward only as long as each level's own value actually changes — called exactly when
    /// [`Self::set`] just flipped that brick's own occupied/empty status (its count crossed
    /// zero), never on an edit that leaves the brick's status unchanged.
    fn update_mips(&mut self, brick_coord: UVec3) {
        if self.mips.is_empty() {
            return;
        }

        let mut coarse = UVec3::new(brick_coord.x / 2, brick_coord.y / 2, brick_coord.z / 2);
        let occupied = self.bricks_occupied_in_group(coarse);
        if !self.mips[0].set(coarse, occupied) {
            return;
        }

        for level in 1..self.mips.len() {
            let parent = UVec3::new(coarse.x / 2, coarse.y / 2, coarse.z / 2);
            let occupied = mip_group_occupied(&self.mips[level - 1], parent);
            if !self.mips[level].set(parent, occupied) {
                break;
            }
            coarse = parent;
        }
    }

    /// Whether any of the 2x2x2 bricks grouped under mip level 0's `group_coord` is occupied.
    fn bricks_occupied_in_group(&self, group_coord: UVec3) -> bool {
        let base = UVec3::new(group_coord.x * 2, group_coord.y * 2, group_coord.z * 2);
        for dz in 0..2 {
            for dy in 0..2 {
                for dx in 0..2 {
                    if self.brick_occupied(UVec3::new(base.x + dx, base.y + dy, base.z + dz)) {
                        return true;
                    }
                }
            }
        }
        false
    }

    /// Returns `true` if the brick at `brick_pos` (in brick coordinates, not voxel coordinates)
    /// has at least one non-air voxel. Out-of-range brick coordinates are treated as unoccupied.
    pub(crate) fn brick_occupied(&self, brick_pos: UVec3) -> bool {
        if brick_pos.x >= self.brick_dims.x
            || brick_pos.y >= self.brick_dims.y
            || brick_pos.z >= self.brick_dims.z
        {
            return false;
        }
        self.brick_occupancy[flatten(brick_pos, self.brick_dims)] > 0
    }

    /// Raw voxel material IDs, flattened x-major/y/z — the exact layout uploaded to the GPU
    /// material texture.
    pub(crate) fn voxels(&self) -> &[VoxelId] {
        &self.voxels
    }

    /// Raw per-brick occupancy counts, flattened the same way as [`Self::voxels`] but over the
    /// brick grid — `> 0` is exactly the value uploaded to the GPU occupancy texture.
    pub(crate) fn brick_occupancy(&self) -> &[u16] {
        &self.brick_occupancy
    }

    /// Number of mip levels above the brick grid (`0` for a chunk with only one brick per axis —
    /// nothing coarser to gain). Level `0` groups 2x2x2 bricks; level `k` groups 2x2x2 cells of
    /// level `k-1`.
    pub(crate) fn mip_level_count(&self) -> usize {
        self.mips.len()
    }

    /// Dimensions of mip level `level`, in that level's own cells.
    ///
    /// # Panics
    /// Panics if `level >= self.mip_level_count()`.
    pub(crate) fn mip_dims(&self, level: usize) -> UVec3 {
        self.mips[level].dims()
    }

    /// Whether mip level `level`'s cell at `coord` is occupied. Out-of-range `coord` is
    /// unoccupied, mirroring [`Self::get`]/[`Self::brick_occupied`].
    ///
    /// # Panics
    /// Panics if `level >= self.mip_level_count()`.
    pub(crate) fn mip_occupied(&self, level: usize, coord: UVec3) -> bool {
        self.mips[level].get(coord)
    }
}

/// Whether any of the 2x2x2 cells of `mip` grouped under `group_coord` (in the NEXT level up's
/// own coordinate space) is occupied. Free function, not a method, since it operates on a
/// specific [`OccupancyMip`] rather than on `self` — used both by [`VoxelChunk::update_mips`]
/// (propagating between mip levels) and available for the same computation the marcher performs
/// when checking a mip cell's children directly.
fn mip_group_occupied(mip: &OccupancyMip, group_coord: UVec3) -> bool {
    let base = UVec3::new(group_coord.x * 2, group_coord.y * 2, group_coord.z * 2);
    for dz in 0..2 {
        for dy in 0..2 {
            for dx in 0..2 {
                if mip.get(UVec3::new(base.x + dx, base.y + dy, base.z + dz)) {
                    return true;
                }
            }
        }
    }
    false
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[should_panic]
    fn new_rejects_dims_not_a_multiple_of_brick_size() {
        VoxelChunk::new(UVec3::splat(7));
    }

    #[test]
    fn new_chunk_is_fully_air_and_unoccupied() {
        let chunk = VoxelChunk::new(UVec3::splat(16));
        assert_eq!(chunk.get(UVec3::new(5, 5, 5)), VoxelId::AIR);
        assert!(!chunk.brick_occupied(UVec3::ZERO));
    }

    #[test]
    fn get_out_of_range_returns_air_instead_of_panicking() {
        let chunk = VoxelChunk::new(UVec3::splat(8));
        assert_eq!(chunk.get(UVec3::new(100, 0, 0)), VoxelId::AIR);
        assert_eq!(chunk.get(UVec3::new(0, 0, 1000)), VoxelId::AIR);
    }

    #[test]
    fn set_out_of_range_is_silently_ignored() {
        let mut chunk = VoxelChunk::new(UVec3::splat(8));
        chunk.set(UVec3::new(100, 0, 0), VoxelId::new(1));
        // No panic, and no brick anywhere became spuriously occupied.
        assert!(!chunk.brick_occupied(UVec3::ZERO));
    }

    #[test]
    fn set_then_get_round_trips() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        let pos = UVec3::new(3, 10, 15);
        chunk.set(pos, VoxelId::new(7));
        assert_eq!(chunk.get(pos), VoxelId::new(7));
        // Neighboring voxel is untouched.
        assert_eq!(chunk.get(pos + UVec3::X), VoxelId::AIR);
    }

    #[test]
    fn setting_the_last_solid_voxel_in_a_brick_to_air_correctly_clears_occupancy() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        let a = UVec3::new(1, 1, 1);
        let b = UVec3::new(2, 2, 2);
        // Both voxels live in the same (0,0,0) brick.
        chunk.set(a, VoxelId::new(1));
        chunk.set(b, VoxelId::new(1));
        assert!(chunk.brick_occupied(UVec3::ZERO));

        chunk.set(a, VoxelId::AIR);
        assert!(
            chunk.brick_occupied(UVec3::ZERO),
            "brick still has one solid voxel left"
        );

        chunk.set(b, VoxelId::AIR);
        assert!(
            !chunk.brick_occupied(UVec3::ZERO),
            "brick's last solid voxel was cleared, so it must report unoccupied"
        );
    }

    #[test]
    fn overwriting_a_voxel_with_the_same_id_does_not_change_occupancy_count() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        let pos = UVec3::new(1, 1, 1);
        chunk.set(pos, VoxelId::new(1));
        chunk.set(pos, VoxelId::new(1));
        chunk.set(pos, VoxelId::AIR);
        // If the redundant set had double-incremented, this would still report occupied.
        assert!(!chunk.brick_occupied(UVec3::ZERO));
    }

    #[test]
    fn a_voxel_in_a_second_brick_does_not_affect_the_first_bricks_occupancy() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        chunk.set(UVec3::new(9, 0, 0), VoxelId::new(1)); // brick (1,0,0)
        assert!(chunk.brick_occupied(UVec3::new(1, 0, 0)));
        assert!(!chunk.brick_occupied(UVec3::new(0, 0, 0)));
    }

    #[test]
    fn mip_hierarchy_has_the_expected_level_count_and_dims() {
        // 128 voxels / 8-voxel bricks = 16 bricks/axis -> mip dims 8,4,2,1 -> 4 levels.
        let chunk = VoxelChunk::new(UVec3::splat(128));
        assert_eq!(chunk.mip_level_count(), 4);
        assert_eq!(chunk.mip_dims(0), UVec3::splat(8));
        assert_eq!(chunk.mip_dims(3), UVec3::splat(1));
    }

    #[test]
    fn setting_a_voxel_propagates_occupancy_through_every_mip_level() {
        let mut chunk = VoxelChunk::new(UVec3::splat(128));
        for level in 0..chunk.mip_level_count() {
            assert!(!chunk.mip_occupied(level, UVec3::ZERO), "level {level} should start unoccupied");
        }

        chunk.set(UVec3::new(1, 1, 1), VoxelId::new(1));

        for level in 0..chunk.mip_level_count() {
            assert!(chunk.mip_occupied(level, UVec3::ZERO), "level {level} should now be occupied");
        }
    }

    #[test]
    fn clearing_the_last_voxel_clears_occupancy_through_every_mip_level() {
        let mut chunk = VoxelChunk::new(UVec3::splat(128));
        chunk.set(UVec3::new(1, 1, 1), VoxelId::new(1));
        chunk.set(UVec3::new(1, 1, 1), VoxelId::AIR);

        for level in 0..chunk.mip_level_count() {
            assert!(!chunk.mip_occupied(level, UVec3::ZERO), "level {level} should be unoccupied again");
        }
    }

    #[test]
    fn mip_level_0_distinguishes_near_and_far_octants_but_the_top_level_does_not() {
        // 32 voxels / 8 = 4 bricks/axis -> mip dims 2, 1 -> 2 levels.
        let mut chunk = VoxelChunk::new(UVec3::splat(32));
        assert_eq!(chunk.mip_level_count(), 2);

        // Brick (0,0,0), voxel-space (0..8, 0..8, 0..8).
        chunk.set(UVec3::new(1, 1, 1), VoxelId::new(1));
        assert!(chunk.mip_occupied(0, UVec3::ZERO), "near octant occupied at level 0");
        assert!(
            !chunk.mip_occupied(0, UVec3::splat(1)),
            "far octant not occupied at level 0 yet"
        );
        assert!(chunk.mip_occupied(1, UVec3::ZERO), "the single top-level cell sees it too");

        // Brick (3,3,3), voxel-space (24..32, 24..32, 24..32) -- the far corner.
        chunk.set(UVec3::new(25, 25, 25), VoxelId::new(1));
        assert!(
            chunk.mip_occupied(0, UVec3::splat(1)),
            "far octant now occupied at level 0"
        );
        // Both octants feed the same single top-level cell.
        assert!(chunk.mip_occupied(1, UVec3::ZERO));
    }

    #[test]
    fn a_voxel_edit_that_does_not_flip_the_bricks_status_does_not_disturb_the_mip_hierarchy() {
        let mut chunk = VoxelChunk::new(UVec3::splat(128));
        // Two voxels in the same brick -- the second set keeps the brick occupied throughout,
        // and the third set (clearing the first) leaves it occupied too (the second remains).
        chunk.set(UVec3::new(1, 1, 1), VoxelId::new(1));
        assert!(chunk.mip_occupied(0, UVec3::ZERO));

        chunk.set(UVec3::new(2, 2, 2), VoxelId::new(2));
        assert!(chunk.mip_occupied(0, UVec3::ZERO));

        chunk.set(UVec3::new(1, 1, 1), VoxelId::AIR);
        assert!(
            chunk.mip_occupied(0, UVec3::ZERO),
            "the brick still has voxel (2,2,2), so the mip hierarchy must stay occupied"
        );
    }

    #[test]
    fn mip_out_of_range_is_unoccupied_not_a_panic() {
        let chunk = VoxelChunk::new(UVec3::splat(128));
        assert!(!chunk.mip_occupied(0, UVec3::splat(1000)));
    }
}
