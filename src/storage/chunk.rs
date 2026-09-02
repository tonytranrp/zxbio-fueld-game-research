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

    /// Fills every voxel in column `(x, z)` from `y_start` (inclusive) to `y_end` (exclusive)
    /// with `material` — behaviorally identical to calling [`Self::set`] for every voxel in that
    /// range, but far cheaper for a large contiguous fill: brick-occupancy and mip-hierarchy
    /// bookkeeping happen once per BRICK the column passes through, not once per voxel. `x`/`z`
    /// outside the chunk, or an empty/inverted `y` range, are silently no-ops, mirroring
    /// [`Self::set`]'s own treatment of out-of-bounds coordinates.
    ///
    /// This exists because it measurably matters, not speculatively: a benchmark generating
    /// procedural terrain via a sequence of individual `set()` calls found that bookkeeping
    /// alone (not the per-voxel write itself) accounted for roughly 80% of total generation
    /// time, dominated by brick/mip overhead repeated once per voxel instead of once per brick —
    /// see the engine's own procedural-generation memory/commit history for the exact numbers.
    pub fn fill_column(&mut self, x: u32, z: u32, y_start: u32, y_end: u32, material: VoxelId) {
        if x >= self.dims.x || z >= self.dims.z || y_start >= y_end {
            return;
        }
        let y_end = y_end.min(self.dims.y);
        if y_start >= y_end {
            return;
        }

        let mut y = y_start;
        while y < y_end {
            let brick_y = y / BRICK_SIZE;
            let brick_y_voxel_end = (brick_y + 1) * BRICK_SIZE;
            let segment_end = y_end.min(brick_y_voxel_end);

            let brick_coord = UVec3::new(x / BRICK_SIZE, brick_y, z / BRICK_SIZE);

            let mut delta: i32 = 0;
            for yy in y..segment_end {
                let idx = flatten(UVec3::new(x, yy, z), self.dims);
                let old = self.voxels[idx];
                if old != material {
                    self.voxels[idx] = material;
                    match (old.is_air(), material.is_air()) {
                        (true, false) => delta += 1,
                        (false, true) => delta -= 1,
                        _ => {}
                    }
                }
            }

            if delta != 0 {
                // `brick_coord` is already known in-bounds here (derived from `x`/`z`/`y`, all
                // already range-checked above), so this indexes `brick_occupancy` directly
                // rather than going through `brick_occupied()`'s own bounds-check + a second,
                // redundant `flatten` call for the same coordinate computed just above.
                let b_idx = flatten(brick_coord, self.brick_dims);
                let old_count = self.brick_occupancy[b_idx];
                let new_count = (old_count as i32 + delta) as u16;
                self.brick_occupancy[b_idx] = new_count;
                if (old_count > 0) != (new_count > 0) {
                    self.update_mips(brick_coord);
                }
            }

            y = segment_end;
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

    #[test]
    fn fill_column_agrees_with_an_equivalent_sequence_of_set_calls() {
        // The whole point of fill_column is being a faster path to an IDENTICAL result --
        // verify that directly across several columns spanning multiple bricks, rather than
        // trusting the bookkeeping logic by inspection alone.
        let cases: [(u32, u32, u32, u32); 5] = [
            (0, 0, 0, 20),   // spans bricks (0) and (1) in Y, partial in each
            (5, 5, 8, 16),   // exactly one whole brick
            (10, 3, 1, 33),  // spans three bricks, partial at both ends
            (127, 127, 0, 128), // a full-height column at the far corner
            (3, 3, 5, 5),    // empty range (y_start == y_end) -- must be a no-op
        ];

        for (x, z, y_start, y_end) in cases {
            let mut via_fill = VoxelChunk::new(UVec3::splat(128));
            via_fill.fill_column(x, z, y_start, y_end, VoxelId::new(3));

            let mut via_set = VoxelChunk::new(UVec3::splat(128));
            for y in y_start..y_end {
                via_set.set(UVec3::new(x, y, z), VoxelId::new(3));
            }

            for y in 0..128u32 {
                let pos = UVec3::new(x, y, z);
                assert_eq!(
                    via_fill.get(pos),
                    via_set.get(pos),
                    "voxel mismatch at {pos:?} for case ({x},{z},{y_start},{y_end})"
                );
            }

            // Brick occupancy must agree too, not just the raw voxel values -- this is the part
            // fill_column actually optimizes, so it's the part most likely to be silently wrong
            // if the batching logic has a bug the plain voxel comparison above wouldn't catch.
            for by in 0..16u32 {
                let brick = UVec3::new(x / 8, by, z / 8);
                assert_eq!(
                    via_fill.brick_occupied(brick),
                    via_set.brick_occupied(brick),
                    "brick occupancy mismatch at {brick:?} for case ({x},{z},{y_start},{y_end})"
                );
            }
        }
    }

    #[test]
    fn fill_column_out_of_range_x_or_z_is_a_no_op() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        chunk.fill_column(100, 0, 0, 16, VoxelId::new(1));
        chunk.fill_column(0, 100, 0, 16, VoxelId::new(1));
        for y in 0..16u32 {
            assert_eq!(chunk.get(UVec3::new(0, y, 0)), VoxelId::AIR);
        }
    }

    #[test]
    fn fill_column_clamps_y_end_to_chunk_height_instead_of_panicking() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        chunk.fill_column(1, 1, 10, 1000, VoxelId::new(1));
        assert_eq!(chunk.get(UVec3::new(1, 15, 1)), VoxelId::new(1));
        assert_eq!(chunk.get(UVec3::new(1, 10, 1)), VoxelId::new(1));
    }

    #[test]
    fn fill_column_updates_mip_hierarchy_correctly_across_multiple_bricks() {
        let mut chunk = VoxelChunk::new(UVec3::splat(128));
        // A tall column spanning bricks 0 through 3 in Y (32 voxels / 8 = bricks 0,1,2,3).
        chunk.fill_column(10, 10, 0, 32, VoxelId::new(1));

        for by in 0..4u32 {
            assert!(
                chunk.brick_occupied(UVec3::new(1, by, 1)),
                "brick (1,{by},1) should be occupied after the fill"
            );
        }
        for level in 0..chunk.mip_level_count() {
            assert!(
                chunk.mip_occupied(level, UVec3::ZERO),
                "mip level {level} should see the fill too"
            );
        }
    }

    #[test]
    fn fill_column_clearing_back_to_air_correctly_clears_occupancy() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        chunk.fill_column(2, 2, 0, 16, VoxelId::new(1));
        assert!(chunk.brick_occupied(UVec3::ZERO));

        chunk.fill_column(2, 2, 0, 16, VoxelId::AIR);
        assert!(!chunk.brick_occupied(UVec3::ZERO));
        for y in 0..16u32 {
            assert_eq!(chunk.get(UVec3::new(2, y, 2)), VoxelId::AIR);
        }
    }
}
