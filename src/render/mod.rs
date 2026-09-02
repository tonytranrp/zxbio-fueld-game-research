//! Custom Bevy `Material`-based voxel rendering: the GPU ray marcher and the CPU-to-GPU upload
//! helpers that feed it.

pub(crate) mod material;
mod spawn;

pub use material::VoxelMaterial;
pub use spawn::spawn_voxel_chunk;
