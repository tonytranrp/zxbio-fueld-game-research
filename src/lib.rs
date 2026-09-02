//! A high-performance sparse-voxel rendering engine built on [Bevy](https://bevyengine.org/) — GPU
//! ray marching through a brick-grid voxel structure rather than triangle meshes. Real, measured
//! scale: a single scene with 2.15 billion voxels (1024 procedurally-generated chunks) at ~38fps
//! on a laptop RTX 4070. See this crate's own `README.md` for the full picture — real numbers, the
//! `DepthPrepass`/`OcclusionCulling` performance recommendation (a real, large win this crate
//! can't apply for you, since it never owns your camera), and design notes worth knowing before
//! building on this (chunks must not spatially overlap; there's no `World`/streaming abstraction
//! yet, deliberately).
//!
//! The example below is kept deliberately minimal and is a real, compiled doctest (`no_run`, since
//! actually running it would open a window and block `cargo test` forever) — so it can't silently
//! drift out of sync with the actual API the way a prose-only or README-only example can.
//!
//! ```no_run
//! use bevy::color::LinearRgba;
//! use bevy::prelude::*;
//! use voxel_engine::{
//!     spawn_voxel_chunk, VoxelChunk, VoxelEnginePlugin, VoxelFlycamPlugin, VoxelId, VoxelMaterial,
//!     VoxelMaterialInfo, VoxelPalette,
//! };
//!
//! fn main() {
//!     App::new()
//!         .add_plugins(DefaultPlugins)
//!         .add_plugins((VoxelEnginePlugin, VoxelFlycamPlugin))
//!         .add_systems(Startup, setup)
//!         .run();
//! }
//!
//! fn setup(
//!     mut commands: Commands,
//!     mut meshes: ResMut<Assets<Mesh>>,
//!     mut materials: ResMut<Assets<VoxelMaterial>>,
//!     mut images: ResMut<Assets<Image>>,
//!     mut buffers: ResMut<Assets<bevy::render::storage::ShaderBuffer>>,
//! ) {
//!     let mut chunk = VoxelChunk::new(UVec3::splat(128));
//!     chunk.set(UVec3::new(10, 10, 10), VoxelId::new(1));
//!
//!     let mut palette = VoxelPalette::new();
//!     palette.set(VoxelId::new(1), VoxelMaterialInfo { color: LinearRgba::new(0.8, 0.25, 0.2, 1.0) });
//!
//!     spawn_voxel_chunk(
//!         &mut commands, &mut meshes, &mut materials, &mut images, &mut buffers,
//!         &chunk, &palette, Transform::IDENTITY,
//!     );
//!
//!     commands.spawn(Camera3d::default());
//! }
//! ```

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
