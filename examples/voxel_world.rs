//! Demo: a grid of independently-generated terrain chunks stitched into one continuous world via
//! `fill_heightmap_terrain`'s `world_origin` parameter -- the first time that API has been
//! exercised as an actual spawned, rendered scene rather than only a unit test comparing two
//! chunks' raw voxel data. "Billions of voxels" (the standing engine goal) cannot be demonstrated,
//! tested, or even meaningfully discussed with the single hand-placed 128^3 chunk every other
//! example uses -- this is the first step past that: prove multi-chunk generation and placement
//! work end-to-end, and measure what a real grid actually costs, rather than assuming it scales.
//!
//! This is deliberately NOT a `World`/chunk-manager abstraction -- no registry, no
//! streaming/loading/unloading, just a fixed grid built once at startup. Building that
//! abstraction before knowing what it actually needs to solve (what's the real bottleneck at
//! scale -- CPU generation time? GPU upload? draw calls? frame time?) would be optimizing blind;
//! this example exists to gather that data first. See the engine's own memory/notes for what this
//! measurement found and what it implies for the next real step.

// common/mod.rs is a shared grab-bag of scene-builders/diagnostics across three examples now
// (benchmark_raymarch, voxel_scene, voxel_world), each using a different subset -- e.g. this file
// never calls build_sparse_chunk/build_dense_chunk. `#[allow(dead_code)]` at the inclusion site
// says that's expected for a shared support module, without hiding genuine dead code inside
// common/mod.rs itself if a helper ever stops being used by ANY example.
#[path = "common/mod.rs"]
#[allow(dead_code)]
mod common;

use std::time::Instant;

use bevy::color::LinearRgba;
use bevy::core_pipeline::prepass::DepthPrepass;
use bevy::dev_tools::fps_overlay::FpsOverlayPlugin;
use bevy::diagnostic::FrameTimeDiagnosticsPlugin;
use bevy::prelude::*;
use bevy::render::occlusion_culling::OcclusionCulling;
use bevy::render::storage::ShaderBuffer;
use voxel_engine::{
    spawn_voxel_chunk, VoxelEnginePlugin, VoxelFlycam, VoxelFlycamPlugin, VoxelId, VoxelMaterial,
    VoxelMaterialInfo, VoxelPalette,
};

const VOXEL_SIZE: f32 = 0.1;
/// 4x4 = 16 chunks, ~33.6M voxels total -- a real step up from the ~2.1M any single-chunk demo
/// has ever tested, while still small enough to generate+spawn in a few seconds even in an
/// unattended debug-mode smoke run. Not chosen to hit "billions" in one shot -- this iteration is
/// about proving the mechanism and measuring it, which a much larger grid wouldn't do any better,
/// only slower to iterate on.
const GRID_SIZE: i32 = 4;
const TERRAIN_SEED: u64 = 7;

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

/// This chunk's own position in WORLD VOXEL-INDEX space -- what `fill_heightmap_terrain` samples
/// noise relative to, so every chunk in the grid shares one continuous noise field instead of each
/// restarting from its own local origin (see that function's own doc comment).
fn chunk_world_origin(grid_x: i32, grid_z: i32) -> IVec3 {
    IVec3::new(grid_x * common::CHUNK_VOXELS as i32, 0, grid_z * common::CHUNK_VOXELS as i32)
}

/// This chunk's placement in RENDER space (meters), derived from the exact same
/// `chunk_world_origin` a chunk's own generation uses -- deliberately not two independently
/// hand-written offset formulas that happen to agree, since that's exactly the kind of thing that
/// silently drifts out of sync. The only new arithmetic here is the voxel-index-space ->
/// meters conversion (`* VOXEL_SIZE`), which `adjacent_chunk_transforms_are_exactly_one_chunk_
/// width_apart` below checks directly.
fn chunk_render_transform(grid_x: i32, grid_z: i32) -> Transform {
    let origin = chunk_world_origin(grid_x, grid_z);
    Transform::from_xyz(origin.x as f32 * VOXEL_SIZE, origin.y as f32 * VOXEL_SIZE, origin.z as f32 * VOXEL_SIZE)
        .with_scale(Vec3::splat(VOXEL_SIZE))
}

fn setup(
    mut commands: Commands,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<VoxelMaterial>>,
    mut images: ResMut<Assets<Image>>,
    mut buffers: ResMut<Assets<ShaderBuffer>>,
) {
    let palette = build_palette();

    let start = Instant::now();
    for grid_z in 0..GRID_SIZE {
        for grid_x in 0..GRID_SIZE {
            let chunk = common::build_terrain_chunk(TERRAIN_SEED, chunk_world_origin(grid_x, grid_z));
            spawn_voxel_chunk(
                &mut commands,
                &mut meshes,
                &mut materials,
                &mut images,
                &mut buffers,
                &chunk,
                &palette,
                chunk_render_transform(grid_x, grid_z),
            );
        }
    }
    let elapsed = start.elapsed();

    // Real, measured number -- CPU generation + GPU-upload-prep time for the whole grid, not a
    // guess. Debug-mode only (no release build this iteration -- see the engine's own tooling
    // notes on the LTO/OOM cost of one), so the absolute number isn't representative of shipped
    // performance, but the PER-CHUNK average is still a meaningful sanity check: it should land
    // close to a single chunk's own already-measured generation cost, not blow up superlinearly.
    let total_chunks = (GRID_SIZE * GRID_SIZE) as u32;
    let total_voxels = total_chunks as u64 * (common::CHUNK_VOXELS as u64).pow(3);
    eprintln!(
        "generated+spawned {total_chunks} chunks ({GRID_SIZE}x{GRID_SIZE}, {total_voxels} total voxels) in {elapsed:?} ({:?}/chunk avg)",
        elapsed / total_chunks,
    );

    let grid_extent = GRID_SIZE as f32 * common::CHUNK_VOXELS as f32 * VOXEL_SIZE;
    let center = grid_extent / 2.0;
    // DepthPrepass + OcclusionCulling: real, measured win (~22fps -> ~79fps with DepthPrepass
    // alone on this exact grid from a ground-level camera, per the engine's own memory/notes) --
    // this shader's fragment() unconditionally `discard`s on a miss and never writes an explicit
    // depth, which disables hardware early-Z without a prepass. See voxel_scene.rs's camera setup
    // for the full reasoning; kept brief here since it's identical for this scene.
    commands.spawn((
        Camera3d::default(),
        DepthPrepass,
        OcclusionCulling,
        Transform::from_xyz(center, grid_extent * 0.9, center + grid_extent * 1.1)
            .looking_at(Vec3::new(center, 0.0, center), Vec3::Y),
        VoxelFlycam::default(),
    ));
}

fn build_palette() -> VoxelPalette {
    let mut palette = VoxelPalette::new();
    palette.set(
        VoxelId::new(2),
        VoxelMaterialInfo {
            color: LinearRgba::new(0.3, 0.5, 0.3, 1.0),
        },
    );
    palette
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn chunk_world_origin_is_the_grid_coord_scaled_by_chunk_voxels() {
        assert_eq!(chunk_world_origin(0, 0), IVec3::ZERO);
        assert_eq!(chunk_world_origin(1, 0), IVec3::new(common::CHUNK_VOXELS as i32, 0, 0));
        assert_eq!(chunk_world_origin(0, 1), IVec3::new(0, 0, common::CHUNK_VOXELS as i32));
        assert_eq!(
            chunk_world_origin(2, 3),
            IVec3::new(2 * common::CHUNK_VOXELS as i32, 0, 3 * common::CHUNK_VOXELS as i32)
        );
        // Negative grid coordinates aren't used by this example's own 0..GRID_SIZE loop, but the
        // function shouldn't silently assume non-negative input either -- worth checking directly
        // rather than trusting it by construction.
        assert_eq!(chunk_world_origin(-1, 0), IVec3::new(-(common::CHUNK_VOXELS as i32), 0, 0));
    }

    #[test]
    fn chunk_render_transform_is_world_origin_scaled_by_voxel_size() {
        let origin = chunk_world_origin(2, 3);
        let transform = chunk_render_transform(2, 3);
        assert_eq!(
            transform.translation,
            Vec3::new(origin.x as f32 * VOXEL_SIZE, origin.y as f32 * VOXEL_SIZE, origin.z as f32 * VOXEL_SIZE)
        );
        assert_eq!(transform.scale, Vec3::splat(VOXEL_SIZE));
    }

    /// The one genuinely new risk this example introduces beyond what's already tested: render
    /// Transform placement, independent of `fill_heightmap_terrain`'s already-verified noise
    /// continuity. A bug here (an off-by-one in the meters-per-chunk offset, say) wouldn't fail
    /// any existing test -- it would silently render chunks with a visible GAP or OVERLAP between
    /// them, exactly the "looks plausible until you check the number" failure class worth a direct
    /// test rather than trusting the arithmetic by eye.
    #[test]
    fn adjacent_chunk_transforms_are_exactly_one_chunk_width_apart() {
        let expected_gap = common::CHUNK_VOXELS as f32 * VOXEL_SIZE;

        let origin = chunk_render_transform(0, 0);
        let next_x = chunk_render_transform(1, 0);
        let next_z = chunk_render_transform(0, 1);

        assert!((next_x.translation.x - origin.translation.x - expected_gap).abs() < 1e-4);
        assert_eq!(next_x.translation.z, origin.translation.z);
        assert!((next_z.translation.z - origin.translation.z - expected_gap).abs() < 1e-4);
        assert_eq!(next_z.translation.x, origin.translation.x);
    }
}
