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

// common/mod.rs is a shared grab-bag of scene-builders/diagnostics across four examples now
// (benchmark_raymarch, voxel_scene, voxel_world, voxel_editing), each using a different subset --
// e.g. this file never calls build_sparse_chunk/build_dense_chunk. `#[allow(dead_code)]` at the
// inclusion site
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
const TERRAIN_SEED: u64 = 7;

/// Grid size (per axis) for the demo world -- 4 by default (16 chunks: fast to iterate on, small
/// enough for an unattended debug-mode smoke run), overridable via the `VOXEL_WORLD_GRID_SIZE`
/// environment variable for stress-testing at real scale without duplicating this whole file.
///
/// **Real numbers, all measured release-mode on this engine's own birds-eye test camera with LOD
/// + `DepthPrepass`/`OcclusionCulling` active** (see the engine's own memory/notes for the full
/// writeup):
/// - `4` (16 chunks, 33.5M voxels): ~74fps
/// - `16` (256 chunks, 537M voxels): ~50fps avg (42.0-59.3), generation 5.16ms/chunk
/// - `32` (1024 chunks, **2,147,483,648 voxels — genuinely over 2 BILLION**, an actual real test
///   of this engine's stated "billions of voxels" goal, not just a bigger demo number): ~38fps
///   avg (33.0-45.5), generation 5.70ms/chunk, no crash, no OOM. Run via
///   `VOXEL_WORLD_GRID_SIZE=32 cargo run --release --example voxel_world --features dev_tools`
///   (add `--config profile.release.lto=false --config profile.release.codegen-units=16` too --
///   see the engine's own tooling notes on why this project's normal `lto="fat"` release profile
///   OOMs LLVM linking an example this size).
///
/// Per-chunk generation cost creeps up gently with grid size (3.75ms -> 5.16ms -> 5.70ms from
/// 16 to 256 to 1024 chunks) -- real, worth knowing, but nowhere near a cliff; not investigated
/// further this session (candidate causes: CPU cache effects at larger total data size, `Vec`
/// allocation overhead scaling with grid size -- unconfirmed either way).
fn grid_size() -> i32 {
    std::env::var("VOXEL_WORLD_GRID_SIZE")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(4)
}

/// Which camera to spawn, selected via the `VOXEL_WORLD_CAMERA` environment variable
/// (`"ground"` or anything else / unset for the default `BirdsEye`).
///
/// Every scaling number in `grid_size`'s own doc comment (74/50/38fps at 16/256/1024 chunks) was
/// measured with `BirdsEye` -- a high, outside-the-grid, looking-down camera where most chunks sit
/// outside the view frustum or nearly edge-on to it at any given moment. `Ground` is the case that
/// camera can never exercise: positioned INSIDE the grid at roughly eye height, looking level
/// across the full diagonal so as many chunks as possible are simultaneously in frame with none of
/// them behind the camera. This is the scenario the billion-voxel milestone's own memory flagged
/// as unmeasured beyond the smallest (16-chunk) grid size -- "a ground-level camera with many
/// chunks actually visible... costs meaningfully more per visible chunk" was asserted from a single
/// small-scale data point, never confirmed at the scales this engine actually claims to handle.
///
/// **Real numbers, release mode, same LOD+`DepthPrepass`+`OcclusionCulling` setup as `BirdsEye`**
/// (see the engine's own memory/notes for the full writeup) -- the prior assumption was WRONG, in
/// the favorable direction: `Ground` is consistently *faster* than `BirdsEye` at every scale
/// tested, and the gap widens as the grid grows, not shrinks:
/// - 16 chunks: ~90fps ground (76.2-98.5) vs. ~74fps birds-eye (+21%)
/// - 256 chunks: ~89fps ground (66.3-104.6) vs. ~50fps birds-eye (+78%)
/// - 1024 chunks / 2.15B voxels: ~76fps ground (62.0-88.4) vs. ~38fps birds-eye (+99%, ~2x)
///
/// Best-supported explanation (mechanistic, not just correlational): this scene's terrain has real
/// vertical relief (`amplitude=20` voxel-index units = 2.0m world height either side of the mean,
/// per `common::build_terrain_chunk`'s params) -- from ground level, nearby hills naturally occlude
/// most distant chunks along the sightline, so `OcclusionCulling` (already shipped, see
/// [[voxel-engine-scale-research]]) skips their fragment work entirely. `BirdsEye` looks nearly
/// straight down at comparatively flat-looking terrain from far above, where far less natural
/// self-occlusion exists between chunks -- most of what's in frustum actually gets shaded. Ground
/// level isn't just "not a hidden weakness" the way the milestone memory worried it might be; the
/// two shipped optimizations (occlusion culling + real terrain relief) compound in exactly the
/// camera scenario that matters most for an actual player, not just an overview screenshot.
enum CameraMode {
    BirdsEye,
    Ground,
}

fn camera_mode() -> CameraMode {
    match std::env::var("VOXEL_WORLD_CAMERA").ok().as_deref() {
        Some("ground") => CameraMode::Ground,
        _ => CameraMode::BirdsEye,
    }
}

/// The `Ground` camera's own transform, factored out from `setup` so it can be unit tested (the
/// same reasoning `chunk_render_transform` above already applies: a placement bug here wouldn't
/// fail any existing test, it would silently point the camera at the sky or bury it in terrain --
/// worth checking the arithmetic directly rather than only by eye in a running window).
///
/// Positioned half a chunk width in from the grid's `(0,0)` corner (not exactly ON the corner, so
/// it isn't sitting exactly on a chunk boundary), at a fixed 8m eye height -- clears this scene's
/// own terrain heightmap (`base_height=40` +-`amplitude=20` voxel-index units = 4.0m +-2.0m world
/// height, per `common::build_terrain_chunk`'s params, so terrain tops out around 6m) without
/// floating so high the shot degenerates back toward a birds-eye angle. Looks level (same Y as the
/// camera itself, not angled down) toward the mirrored point half a chunk width in from the far
/// `(grid_size, grid_size)` corner -- the longest sightline the grid offers, maximizing how many
/// chunks fall inside the frustum at once.
///
/// `margin` is capped at a quarter of `grid_extent` -- found by an independent review, not this
/// example's own tests: at `grid_size=1`, an UNCAPPED half-chunk-width margin on both ends of a
/// one-chunk-wide grid makes the camera position and look target bit-identical
/// (`grid_extent - margin == margin` exactly in IEEE-754, since both sides reduce to the same
/// `grid_extent / 2`), producing a zero-length look direction. Bevy's `Transform::looking_at`
/// doesn't panic on that -- it silently falls back to facing world `-Z` (confirmed by reading
/// `bevy_transform`'s own `look_to`) -- so this failed silently, not loudly: the camera would face
/// an arbitrary hardcoded direction instead of "toward the far corner" as documented. Capping at
/// `grid_extent * 0.25` guarantees `margin < grid_extent - margin` strictly for any `grid_size >=
/// 1` (margin can be at most a quarter of the extent, leaving the far side at least three quarters
/// of it) while leaving every grid size this crate has actually measured (>=2, where the
/// half-chunk-width margin is already well under a quarter of the total extent) numerically
/// unchanged -- see `ground_camera_does_not_degenerate_at_the_smallest_real_grid_size` below.
fn ground_camera_transform(grid_size: i32) -> Transform {
    let grid_extent = grid_size as f32 * common::CHUNK_VOXELS as f32 * VOXEL_SIZE;
    let margin = (common::CHUNK_VOXELS as f32 * VOXEL_SIZE * 0.5).min(grid_extent * 0.25);
    let eye_height = 8.0;
    Transform::from_xyz(margin, eye_height, margin)
        .looking_at(Vec3::new(grid_extent - margin, eye_height, grid_extent - margin), Vec3::Y)
}

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
    let grid_size = grid_size();

    // `grid_size <= 0` (an empty or malformed `VOXEL_WORLD_GRID_SIZE`) has no sensible world to
    // build -- found by an independent review, not this example's own tests: the code below used
    // to reach `elapsed / total_chunks` with `total_chunks = 0`, and `Duration`'s own `Div<u32>`
    // panics on a zero divisor (real, reachable via `VOXEL_WORLD_GRID_SIZE=0`, unrelated to the
    // ground-camera fix above -- confirmed via `git show` that this line predates both). A
    // negative `grid_size` doesn't hit that exact panic (`grid_size * grid_size` overflows back
    // to positive, and `0..grid_size` is simply an empty range), but would silently report a
    // wrong, positive `total_chunks` for a world that spawned nothing. Bailing out early with a
    // clear message covers both: no chunks, no camera, no misleading log line, no panic.
    if grid_size <= 0 {
        eprintln!("VOXEL_WORLD_GRID_SIZE={grid_size} -- nothing to spawn, exiting without a world");
        return;
    }

    let start = Instant::now();
    for grid_z in 0..grid_size {
        for grid_x in 0..grid_size {
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
    let total_chunks = (grid_size * grid_size) as u32;
    let total_voxels = total_chunks as u64 * (common::CHUNK_VOXELS as u64).pow(3);
    eprintln!(
        "generated+spawned {total_chunks} chunks ({grid_size}x{grid_size}, {total_voxels} total voxels) in {elapsed:?} ({:?}/chunk avg)",
        elapsed / total_chunks,
    );

    let grid_extent = grid_size as f32 * common::CHUNK_VOXELS as f32 * VOXEL_SIZE;
    let center = grid_extent / 2.0;
    let transform = match camera_mode() {
        CameraMode::BirdsEye => Transform::from_xyz(center, grid_extent * 0.9, center + grid_extent * 1.1)
            .looking_at(Vec3::new(center, 0.0, center), Vec3::Y),
        CameraMode::Ground => ground_camera_transform(grid_size),
    };
    // DepthPrepass + OcclusionCulling: real, measured win (~22fps -> ~79fps with DepthPrepass
    // alone on this exact grid from a ground-level camera, per the engine's own memory/notes) --
    // this shader's fragment() unconditionally `discard`s on a miss and never writes an explicit
    // depth, which disables hardware early-Z without a prepass. See voxel_scene.rs's camera setup
    // for the full reasoning; kept brief here since it's identical for this scene.
    commands.spawn((Camera3d::default(), DepthPrepass, OcclusionCulling, transform, VoxelFlycam::default()));
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

    /// Found by an independent review: an uncapped margin made the camera position and its own
    /// look target bit-identical at `grid_size=1` (both reduce to exactly `grid_extent / 2`),
    /// producing a zero-length look direction that `Transform::looking_at` silently resolves to a
    /// hardcoded `-Z` fallback rather than the documented "toward the far corner." This is the
    /// smallest real grid size this example can actually spawn (`grid_size<=0` now bails out of
    /// `setup` before ever calling this function) -- worth locking in directly, not just trusting
    /// the `margin` cap's own algebra.
    #[test]
    fn ground_camera_does_not_degenerate_at_the_smallest_real_grid_size() {
        let transform = ground_camera_transform(1);
        let forward = *transform.forward();

        assert!(forward.length() > 0.99, "forward should stay unit-length, not collapse toward zero: {forward:?}");
        assert!(forward.x > 0.0 && forward.z > 0.0, "should still look toward the far corner, not the -Z fallback: {forward:?}");
    }

    #[test]
    fn ground_camera_sits_inside_the_grid_at_a_fixed_eye_height_looking_level_toward_the_far_corner() {
        let grid_size = 16;
        let transform = ground_camera_transform(grid_size);
        let grid_extent = grid_size as f32 * common::CHUNK_VOXELS as f32 * VOXEL_SIZE;
        let margin = common::CHUNK_VOXELS as f32 * VOXEL_SIZE * 0.5;

        // Positioned a real half-chunk-width inside the grid's own extent, not at/outside its edge.
        assert_eq!(transform.translation, Vec3::new(margin, 8.0, margin));
        assert!(margin > 0.0 && margin < grid_extent, "camera must sit inside the grid, not on its boundary");

        // forward() must point toward positive X and Z (the far corner), with zero Y component for
        // a level gaze, since the look target shares the camera's own Y exactly.
        let forward = *transform.forward();
        assert!(forward.x > 0.0 && forward.z > 0.0, "camera must look toward the far corner, not away from the grid");
        assert!(forward.y.abs() < 1e-5, "gaze must be level (look target shares the camera's own Y), got {forward:?}");
    }

    /// A first draft of this test asserted the two cameras' `forward()` vectors must differ --
    /// wrong, and caught by actually running it: for a SQUARE grid with an equal margin on both
    /// axes, the diagonal from `(margin, margin)` to `(grid_extent - margin, grid_extent - margin)`
    /// is always exactly 45 degrees regardless of `grid_extent`'s own magnitude, so direction alone
    /// can never distinguish a small grid from a large one here -- a real, intentional consequence
    /// of the symmetric design, not a bug. What actually changes with grid size is the *distance*
    /// to that target, which is what this test checks instead.
    #[test]
    fn ground_camera_targets_a_point_further_away_for_a_larger_grid_while_staying_on_the_same_diagonal() {
        let margin = common::CHUNK_VOXELS as f32 * VOXEL_SIZE * 0.5;
        let target_distance = |grid_size: i32| {
            let grid_extent = grid_size as f32 * common::CHUNK_VOXELS as f32 * VOXEL_SIZE;
            let target = Vec3::new(grid_extent - margin, 8.0, grid_extent - margin);
            ground_camera_transform(grid_size).translation.distance(target)
        };
        assert!(
            target_distance(64) > target_distance(4),
            "a larger grid's far corner should sit further from the same fixed camera position"
        );

        let small = ground_camera_transform(4);
        let large = ground_camera_transform(64);
        assert_eq!(small.translation, large.translation);
        assert_eq!(*small.forward(), *large.forward());
    }
}
