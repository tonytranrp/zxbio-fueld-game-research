//! A high-performance sparse-voxel rendering engine built on Bevy.

mod flycam;
mod generation;
mod plugin;
mod raymarch;
mod render;
mod storage;

pub use flycam::{VoxelFlycam, VoxelFlycamPlugin};
pub use generation::{fill_heightmap_terrain, HeightmapParams, PerlinNoise};
pub use plugin::VoxelEnginePlugin;
pub use raymarch::{cast_ray, RayHit};
pub use render::{spawn_voxel_chunk, VoxelMaterial};
pub use storage::{VoxelChunk, VoxelId, VoxelMaterialInfo, VoxelPalette};
