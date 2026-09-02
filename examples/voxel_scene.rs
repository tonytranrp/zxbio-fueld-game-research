//! Demo: three static voxel scenes -- sparse (hand-stamped spheres), dense (solid minus carved
//! pockets), and procedurally generated terrain -- rendered side by side via the engine's GPU
//! ray marcher, with a flyable debug camera and a live FPS overlay. For a headless, GPU-free
//! version of the sparse/dense comparison (raw `cast_ray` throughput rather than on-screen FPS),
//! see `examples/benchmark_raymarch.rs` -- and read that file's own doc comment before assuming
//! "sparse renders faster": the measured result is the opposite, dominated by hit/miss cost
//! rather than by the occupancy-skip optimization alone.

#[path = "common/mod.rs"]
mod common;

use bevy::color::LinearRgba;
use bevy::dev_tools::fps_overlay::FpsOverlayPlugin;
use bevy::diagnostic::FrameTimeDiagnosticsPlugin;
use bevy::prelude::*;
use bevy::render::storage::ShaderBuffer;
use voxel_engine::{
    spawn_voxel_chunk, VoxelEnginePlugin, VoxelFlycam, VoxelFlycamPlugin, VoxelId, VoxelMaterial,
    VoxelMaterialInfo, VoxelPalette,
};

const VOXEL_SIZE: f32 = 0.1;

// **First real FPS measurement** (release build, `opt-level=3` but LTO disabled for this one
// build to work around an LLVM out-of-memory crash under this project's normal `lto="fat"`
// release profile — see the engine's own tooling notes on why that's still a representative
// number for THIS specific measurement: the actual per-frame cost here is GPU-side
// fragment-shader ray marching, not CPU-side Rust code LTO would meaningfully speed up):
// **~163-168 fps**, stable across 14 samples over ~16 seconds, all three demo scenes
// (sparse/dense/terrain) rendered simultaneously, right after the GPU marcher's mip-hierarchy
// port. No prior number exists to compare against — this logging didn't exist before that port —
// so this is a baseline for future work to measure against, not a before/after result. Captured
// via `common::log_fps_once_per_second`, added below.

fn main() {
    App::new()
        .add_plugins(DefaultPlugins)
        .add_plugins(FrameTimeDiagnosticsPlugin::default())
        .add_plugins(FpsOverlayPlugin::default())
        .add_plugins((VoxelEnginePlugin, VoxelFlycamPlugin))
        .add_systems(Startup, setup)
        .add_systems(Update, common::log_fps_once_per_second)
        .run();
}

fn setup(
    mut commands: Commands,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<VoxelMaterial>>,
    mut images: ResMut<Assets<Image>>,
    mut buffers: ResMut<Assets<ShaderBuffer>>,
) {
    let palette = build_palette();

    let sparse = common::build_sparse_chunk();
    spawn_voxel_chunk(
        &mut commands,
        &mut meshes,
        &mut materials,
        &mut images,
        &mut buffers,
        &sparse,
        &palette,
        Transform::from_xyz(0.0, 0.0, 0.0).with_scale(Vec3::splat(VOXEL_SIZE)),
    );

    let dense = common::build_dense_chunk();
    spawn_voxel_chunk(
        &mut commands,
        &mut meshes,
        &mut materials,
        &mut images,
        &mut buffers,
        &dense,
        &palette,
        Transform::from_xyz(20.0, 0.0, 0.0).with_scale(Vec3::splat(VOXEL_SIZE)),
    );

    let terrain = common::build_terrain_chunk(1, IVec3::ZERO);
    spawn_voxel_chunk(
        &mut commands,
        &mut meshes,
        &mut materials,
        &mut images,
        &mut buffers,
        &terrain,
        &palette,
        Transform::from_xyz(40.0, 0.0, 0.0).with_scale(Vec3::splat(VOXEL_SIZE)),
    );

    commands.spawn((
        Camera3d::default(),
        Transform::from_xyz(26.0, 24.0, 90.0).looking_at(Vec3::new(26.0, 6.0, 16.0), Vec3::Y),
        VoxelFlycam::default(),
    ));
}

fn build_palette() -> VoxelPalette {
    let mut palette = VoxelPalette::new();
    palette.set(
        VoxelId::new(1),
        VoxelMaterialInfo {
            color: LinearRgba::new(0.8, 0.25, 0.2, 1.0),
        },
    );
    palette.set(
        VoxelId::new(2),
        VoxelMaterialInfo {
            color: LinearRgba::new(0.3, 0.5, 0.8, 1.0),
        },
    );
    palette.set(
        VoxelId::new(3),
        VoxelMaterialInfo {
            color: LinearRgba::new(0.3, 0.75, 0.3, 1.0),
        },
    );
    palette.set(
        VoxelId::new(4),
        VoxelMaterialInfo {
            color: LinearRgba::new(0.85, 0.7, 0.2, 1.0),
        },
    );
    palette
}
