//! Demo: interactive editing. Left-click a solid voxel to dig it out; right-click a solid face to
//! place a voxel next to it. This is the full picking-to-edit pipeline this crate provides,
//! exercised end to end for the first time: a world-space mouse ray -> `Camera::viewport_to_world`
//! (stock Bevy) -> converted into the chunk's own local voxel-index space -> `cast_ray` (this
//! crate) -> `VoxelChunk::set` -> `VoxelMaterial::update_from_chunk` (re-upload in place, no
//! despawn/respawn). Also the first place `RayHit::normal` is used by anything other than its own
//! unit test -- it's what makes "place adjacent to the hit face" possible.

// common/mod.rs is a shared grab-bag of scene-builders/diagnostics across four examples now
// (benchmark_raymarch, voxel_scene, voxel_world, voxel_editing), each using a different subset --
// this file only calls log_fps_once_per_second, building its own chunk directly via fill_column
// instead. `#[allow(dead_code)]` at the inclusion site says that's expected for a shared support
// module, without hiding genuine dead code inside common/mod.rs itself if a helper ever stops
// being used by ANY example.
#[path = "common/mod.rs"]
#[allow(dead_code)]
mod common;

use bevy::color::LinearRgba;
use bevy::core_pipeline::prepass::DepthPrepass;
use bevy::dev_tools::fps_overlay::FpsOverlayPlugin;
use bevy::diagnostic::FrameTimeDiagnosticsPlugin;
use bevy::pbr::MeshMaterial3d;
use bevy::prelude::*;
use bevy::render::occlusion_culling::OcclusionCulling;
use bevy::render::storage::ShaderBuffer;
use voxel_engine::{
    cast_ray, spawn_voxel_chunk, RayHit, VoxelChunk, VoxelEnginePlugin, VoxelFlycam,
    VoxelFlycamPlugin, VoxelId, VoxelMaterial, VoxelMaterialInfo, VoxelPalette,
};

const VOXEL_SIZE: f32 = 0.1;
const CHUNK_VOXELS: u32 = 32;
/// World-space position of the chunk's own voxel-index `(0, 0, 0)` corner -- kept as a named
/// constant, not just baked into the spawn `Transform`, because `dig_or_place` needs this SAME
/// value to convert a world-space ray back into the chunk's local voxel-index space (see
/// `spawn_voxel_chunk`'s own doc comment for this corner/scale convention).
const CHUNK_ORIGIN: Vec3 = Vec3::new(-1.6, 0.0, -1.6);

fn main() {
    App::new()
        .add_plugins(DefaultPlugins)
        .add_plugins(FrameTimeDiagnosticsPlugin::default())
        .add_plugins(FpsOverlayPlugin::default())
        .add_plugins((VoxelEnginePlugin, VoxelFlycamPlugin))
        .add_systems(Startup, setup)
        .add_systems(Update, (dig_or_place, common::log_fps_once_per_second))
        .run();
}

/// Holds the CPU-side data a spawned chunk entity was built from, so `dig_or_place` can edit it
/// and re-upload -- `spawn_voxel_chunk` itself only returns the spawned `Entity`, not the chunk/
/// palette it consumed, since most consumers (every other example) never touch a chunk again
/// after spawning it.
#[derive(Component)]
struct EditableChunk {
    chunk: VoxelChunk,
    palette: VoxelPalette,
}

fn setup(
    mut commands: Commands,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<VoxelMaterial>>,
    mut images: ResMut<Assets<Image>>,
    mut buffers: ResMut<Assets<ShaderBuffer>>,
) {
    let mut chunk = VoxelChunk::new(UVec3::splat(CHUNK_VOXELS));
    for x in 0..CHUNK_VOXELS {
        for z in 0..CHUNK_VOXELS {
            chunk.fill_column(x, z, 0, CHUNK_VOXELS, VoxelId::new(1));
        }
    }

    let mut palette = VoxelPalette::new();
    palette.set(
        VoxelId::new(1),
        VoxelMaterialInfo {
            color: LinearRgba::new(0.75, 0.55, 0.35, 1.0),
        },
    );

    let transform = Transform::from_translation(CHUNK_ORIGIN).with_scale(Vec3::splat(VOXEL_SIZE));
    let entity = spawn_voxel_chunk(
        &mut commands,
        &mut meshes,
        &mut materials,
        &mut images,
        &mut buffers,
        &chunk,
        &palette,
        transform,
    );
    commands.entity(entity).insert(EditableChunk { chunk, palette });

    // DepthPrepass + OcclusionCulling: not load-bearing for a single small chunk's own FPS, but
    // added anyway for consistency with every other example -- a consumer copying this file as a
    // starting point should see the engine's real recommended camera setup, not a stripped-down
    // one that happens to not need it yet at this tiny scale.
    commands.spawn((
        Camera3d::default(),
        DepthPrepass,
        OcclusionCulling,
        Transform::from_xyz(0.0, 2.5, 6.0).looking_at(Vec3::new(0.0, 1.0, 0.0), Vec3::Y),
        VoxelFlycam::default(),
    ));
}

/// Left click digs the hit voxel out (sets it to air); right click places a voxel adjacent to the
/// hit face (`hit.voxel + hit.normal`). Both share the same ray-cast-then-lookup step, so one
/// system handles both rather than duplicating the picking math across two systems.
fn dig_or_place(
    mouse: Res<ButtonInput<MouseButton>>,
    windows: Query<&Window>,
    camera_query: Query<(&Camera, &GlobalTransform)>,
    mut chunks: Query<(&mut EditableChunk, &MeshMaterial3d<VoxelMaterial>)>,
    mut materials: ResMut<Assets<VoxelMaterial>>,
    mut images: ResMut<Assets<Image>>,
    mut buffers: ResMut<Assets<ShaderBuffer>>,
) {
    let digging = mouse.just_pressed(MouseButton::Left);
    let placing = mouse.just_pressed(MouseButton::Right);
    if !digging && !placing {
        return;
    }
    let Ok(window) = windows.single() else { return };
    let Some(cursor) = window.cursor_position() else { return };
    let Ok((camera, camera_transform)) = camera_query.single() else { return };
    let Ok(ray) = camera.viewport_to_world(camera_transform, cursor) else { return };

    let (local_origin, local_dir) =
        world_ray_to_local(ray.origin, ray.direction.as_vec3(), CHUNK_ORIGIN, VOXEL_SIZE);

    for (mut editable, material_handle) in &mut chunks {
        let Some(hit) = cast_ray(&editable.chunk, local_origin, local_dir, 100.0) else {
            continue;
        };

        if digging {
            editable.chunk.set(hit.voxel, VoxelId::AIR);
        } else {
            editable.chunk.set(place_target(&hit), VoxelId::new(1));
        }

        if let Some(mut material) = materials.get_mut(&material_handle.0) {
            material.update_from_chunk(&editable.chunk, &editable.palette, &mut images, &mut buffers);
        }
    }
}

/// Converts a world-space ray into the chunk's own local voxel-index space, where [`cast_ray`]
/// operates -- see its own module doc comment for that convention (`[0, dims]` from a corner,
/// matching `spawn_voxel_chunk`'s placement contract: `chunk_origin` is where local `(0,0,0)`
/// sits in the world, `voxel_size` is the world size of one voxel).
///
/// Position has a translation component, so it needs the full `(world - origin) / voxel_size`.
/// Direction doesn't, and `ray_dir` is assumed already unit length (as `Camera::viewport_to_world`
/// always returns), so no `/ voxel_size` is applied to it: dividing a unit vector by any positive
/// scalar only rescales its magnitude, never the actual direction the ray travels through the
/// grid, so doing so here would be a no-op in every way that matters (which voxel ends up hit).
/// That also means `cast_ray`'s own `max_dist` ends up directly in voxel-count units, not world
/// meters -- confirmed, not just asserted, by `dig_then_recast_hits_the_next_voxel_back_and_
/// place_restores_it` below computing an exact expected `hit.distance` by hand.
fn world_ray_to_local(ray_origin: Vec3, ray_dir: Vec3, chunk_origin: Vec3, voxel_size: f32) -> (Vec3, Vec3) {
    ((ray_origin - chunk_origin) / voxel_size, ray_dir)
}

/// Where a "place" click puts its new voxel: adjacent to the hit face, `hit.voxel + hit.normal`.
///
/// `hit.voxel` is unsigned and `hit.normal` a signed +-1 vector, so the componentwise sum can go
/// negative (e.g. placing off the chunk's own `x = 0` face). Each `as u32` below wraps a negative
/// result to a huge value via two's-complement reinterpretation, which `VoxelChunk::set` already
/// treats like any other out-of-range coordinate: a silent no-op, not a panic (see its own doc
/// comment) -- confirmed directly, not just assumed, by `place_target_below_zero_wraps_via_twos_
/// complement_and_set_still_no_ops_safely` below.
fn place_target(hit: &RayHit) -> UVec3 {
    UVec3::new(
        (hit.voxel.x as i32 + hit.normal.x) as u32,
        (hit.voxel.y as i32 + hit.normal.y) as u32,
        (hit.voxel.z as i32 + hit.normal.z) as u32,
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn world_ray_to_local_translates_and_scales_origin_but_leaves_direction_untouched() {
        let chunk_origin = Vec3::new(-1.6, 0.0, -1.6);
        let ray_origin = Vec3::new(0.0, 2.5, 6.0);
        let ray_dir = Vec3::new(0.0, -0.2, -1.0).normalize();

        let (local_origin, local_dir) = world_ray_to_local(ray_origin, ray_dir, chunk_origin, VOXEL_SIZE);

        assert_eq!(local_origin, (ray_origin - chunk_origin) / VOXEL_SIZE);
        assert_eq!(local_dir, ray_dir);
    }

    /// The real end-to-end check: a ray shaped like this example's own camera, straight down -Z
    /// so the entry point is hand-computable exactly, cast against the exact solid chunk `setup`
    /// builds. Chunk origin `(-1.6, 0, -1.6)`, `VOXEL_SIZE = 0.1` -> the chunk occupies world
    /// space `x,z in [-1.6, 1.6]`, `y in [0, 3.2]`. A ray from world `(0, 1.6, 6.0)` toward
    /// `(0, 0, -1)` has local origin `((0,1.6,6.0) - (-1.6,0,-1.6)) / 0.1 = (16, 16, 76)` and
    /// unchanged local direction `(0, 0, -1)` -- it enters the chunk's `[0,32]` box at local
    /// `z = 32` after traveling `76 - 32 = 44` local units, landing in voxel `z = 31` (indices
    /// span `[n, n+1)`), face normal `+Z` (pointing back out at the camera). Digging that voxel
    /// and re-casting from the SAME ray should reach one voxel further, `z = 30`, after `45`
    /// units; placing back from that second hit should exactly restore the first.
    #[test]
    fn dig_then_recast_hits_the_next_voxel_back_and_place_restores_it() {
        let chunk_origin = Vec3::new(-1.6, 0.0, -1.6);
        let mut chunk = VoxelChunk::new(UVec3::splat(CHUNK_VOXELS));
        for x in 0..CHUNK_VOXELS {
            for z in 0..CHUNK_VOXELS {
                chunk.fill_column(x, z, 0, CHUNK_VOXELS, VoxelId::new(1));
            }
        }
        let (local_origin, local_dir) =
            world_ray_to_local(Vec3::new(0.0, 1.6, 6.0), Vec3::new(0.0, 0.0, -1.0), chunk_origin, VOXEL_SIZE);

        let first_hit = cast_ray(&chunk, local_origin, local_dir, 100.0)
            .expect("ray should hit the solid chunk's near face");
        assert_eq!(first_hit.voxel, UVec3::new(16, 16, 31));
        assert_eq!(first_hit.normal, IVec3::new(0, 0, 1));
        assert_eq!(first_hit.distance, 44.0);

        chunk.set(first_hit.voxel, VoxelId::AIR);
        let second_hit = cast_ray(&chunk, local_origin, local_dir, 100.0)
            .expect("ray should now pass through the dug voxel and hit the next one back");
        assert_eq!(second_hit.voxel, UVec3::new(16, 16, 30));
        assert_eq!(second_hit.normal, IVec3::new(0, 0, 1));
        assert_eq!(second_hit.distance, 45.0);

        let restored = place_target(&second_hit);
        assert_eq!(restored, first_hit.voxel);
        chunk.set(restored, VoxelId::new(1));
        assert_eq!(chunk.get(first_hit.voxel), VoxelId::new(1));
    }

    #[test]
    fn place_target_at_the_chunk_boundary_wraps_to_an_out_of_range_coordinate_not_a_panic() {
        let hit = RayHit {
            voxel: UVec3::new(16, 16, 31),
            material: VoxelId::new(1),
            distance: 44.0,
            normal: IVec3::new(0, 0, 1),
        };
        let target = place_target(&hit);
        assert_eq!(target, UVec3::new(16, 16, 32));

        let mut chunk = VoxelChunk::new(UVec3::splat(CHUNK_VOXELS));
        chunk.set(target, VoxelId::new(1)); // must not panic
        assert_eq!(chunk.get(target), VoxelId::AIR); // out-of-range set is a documented no-op
    }

    #[test]
    fn place_target_below_zero_wraps_via_twos_complement_and_set_still_no_ops_safely() {
        let hit = RayHit {
            voxel: UVec3::new(0, 5, 5),
            material: VoxelId::new(1),
            distance: 1.0,
            normal: IVec3::new(-1, 0, 0),
        };
        let target = place_target(&hit);
        assert_eq!(target.x, u32::MAX); // (0i32 + -1) as u32, two's-complement reinterpretation

        let mut chunk = VoxelChunk::new(UVec3::splat(CHUNK_VOXELS));
        chunk.set(target, VoxelId::new(1)); // must not panic
    }
}
