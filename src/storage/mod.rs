//! Voxel storage: a brick-grid [`VoxelChunk`] with per-brick occupancy tracking for fast
//! empty-space skipping, plus the [`VoxelId`]/[`VoxelPalette`] material model.

mod chunk;
pub(crate) mod coords;
mod voxel;

pub use chunk::VoxelChunk;
pub use voxel::{VoxelId, VoxelMaterialInfo, VoxelPalette};
