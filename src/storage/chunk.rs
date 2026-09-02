//! The `VoxelChunk` dense brick-grid voxel store.

use bevy::math::UVec3;

use super::coords::{brick_dims, contains_voxel, flatten, is_valid_chunk_dims, split_voxel, BRICK_SIZE};
use super::voxel::VoxelId;

/// A dense grid of voxels backed by per-brick occupancy counts for fast empty-space skipping
/// during ray marching. Chunk dimensions (in voxels) must be a nonzero multiple of the brick
/// size (8) on every axis.
#[derive(Debug, Clone)]
pub struct VoxelChunk {
    dims: UVec3,
    brick_dims: UVec3,
    voxels: Vec<VoxelId>,
    /// Count of non-air voxels in each brick, indexed the same way as `voxels` but over the
    /// brick grid. `0` means the brick is fully empty and can be skipped entirely.
    brick_occupancy: Vec<u16>,
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
            (true, false) => self.brick_occupancy[b_idx] += 1,
            (false, true) => self.brick_occupancy[b_idx] -= 1,
            _ => {}
        }
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
}
