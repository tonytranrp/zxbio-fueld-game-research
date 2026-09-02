//! Milestone 1 demo: two static voxel scenes -- one sparse, one dense -- rendered via the
//! engine's GPU ray marcher, with a flyable debug camera and a live FPS overlay. Comparing the
//! two scenes' FPS is what makes the brick-occupancy skip optimization visible rather than just
//! asserted: the sparse scene should run measurably faster, since most of its bricks are empty
//! and never drop into the fine per-voxel march at all.

use bevy::color::LinearRgba;
use bevy::dev_tools::fps_overlay::FpsOverlayPlugin;
use bevy::diagnostic::FrameTimeDiagnosticsPlugin;
use bevy::math::{IVec3, UVec3};
use bevy::prelude::*;
use bevy::render::storage::ShaderBuffer;
use voxel_engine::{
    spawn_voxel_chunk, VoxelChunk, VoxelEnginePlugin, VoxelFlycam, VoxelFlycamPlugin, VoxelId,
    VoxelMaterial, VoxelMaterialInfo, VoxelPalette,
};

const VOXEL_SIZE: f32 = 0.1;
const CHUNK_VOXELS: u32 = 128;

fn main() {
    App::new()
        .add_plugins(DefaultPlugins)
        .add_plugins(FrameTimeDiagnosticsPlugin::default())
        .add_plugins(FpsOverlayPlugin::default())
        .add_plugins((VoxelEnginePlugin, VoxelFlycamPlugin))
        .add_systems(Startup, setup)
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

    let sparse = build_sparse_chunk();
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

    let dense = build_dense_chunk();
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

    commands.spawn((
        Camera3d::default(),
        Transform::from_xyz(16.0, 20.0, 70.0).looking_at(Vec3::new(16.0, 6.0, 16.0), Vec3::Y),
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

/// A handful of floating spheres, well separated so most of the volume stays empty air -- the
/// case where the brick-occupancy skip should help the most.
fn build_sparse_chunk() -> VoxelChunk {
    let mut chunk = VoxelChunk::new(UVec3::splat(CHUNK_VOXELS));
    stamp_sphere(&mut chunk, IVec3::new(24, 90, 24), 12, VoxelId::new(1));
    stamp_sphere(&mut chunk, IVec3::new(100, 40, 100), 16, VoxelId::new(2));
    stamp_sphere(&mut chunk, IVec3::new(64, 100, 64), 10, VoxelId::new(3));
    stamp_sphere(&mut chunk, IVec3::new(30, 20, 100), 13, VoxelId::new(4));
    stamp_sphere(&mut chunk, IVec3::new(110, 110, 24), 11, VoxelId::new(1));
    chunk
}

/// The whole volume filled solid, then a few pockets carved out -- the opposite occupancy
/// profile from the sparse scene, where nearly every brick along any ray is occupied and the
/// coarse pass can't skip anything.
fn build_dense_chunk() -> VoxelChunk {
    let mut chunk = VoxelChunk::new(UVec3::splat(CHUNK_VOXELS));
    fill_solid(&mut chunk, VoxelId::new(2));
    stamp_sphere(&mut chunk, IVec3::new(64, 64, 64), 40, VoxelId::AIR);
    stamp_sphere(&mut chunk, IVec3::new(20, 20, 20), 15, VoxelId::AIR);
    stamp_sphere(&mut chunk, IVec3::new(108, 108, 108), 15, VoxelId::AIR);
    chunk
}

fn stamp_sphere(chunk: &mut VoxelChunk, center: IVec3, radius: i32, material: VoxelId) {
    let radius_sq = radius * radius;
    for z in (center.z - radius).max(0)..=(center.z + radius) {
        for y in (center.y - radius).max(0)..=(center.y + radius) {
            for x in (center.x - radius).max(0)..=(center.x + radius) {
                let offset = IVec3::new(x, y, z) - center;
                let dist_sq = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
                if dist_sq <= radius_sq {
                    chunk.set(UVec3::new(x as u32, y as u32, z as u32), material);
                }
            }
        }
    }
}

fn fill_solid(chunk: &mut VoxelChunk, material: VoxelId) {
    let dims = chunk.dims();
    for z in 0..dims.z {
        for y in 0..dims.y {
            for x in 0..dims.x {
                chunk.set(UVec3::new(x, y, z), material);
            }
        }
    }
}
