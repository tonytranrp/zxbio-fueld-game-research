//! Ties a [`VoxelChunk`] + [`VoxelPalette`] + placement together into a rendered entity.

use bevy::asset::Assets;
use bevy::image::Image;
use bevy::math::primitives::Cuboid;
use bevy::math::Vec3;
use bevy::pbr::MeshMaterial3d;
use bevy::prelude::{Commands, Entity, Mesh, Mesh3d, Transform};
use bevy::render::storage::ShaderBuffer;

use crate::storage::{VoxelChunk, VoxelPalette};

use super::material::{build_occupancy_image, build_voxel_image, VoxelMaterial};

/// Uploads `chunk` to the GPU and spawns a rendered entity for it.
///
/// `transform`'s translation places the chunk's own voxel-index origin `(0, 0, 0)` (its corner,
/// not its center); its (uniform) scale becomes the world size of one voxel — e.g.
/// `Transform::from_scale(Vec3::splat(0.1))` renders each voxel as a 0.1m cube. Only
/// `transform.scale.x` is used; non-uniform scale isn't supported by the ray marcher's math.
///
/// # Spawned chunks must not spatially overlap
///
/// This is a real constraint, not a hypothetical — worth stating plainly since violating it
/// produces silently-wrong rendering with no error or warning. [`VoxelMaterial`]'s fragment
/// shader never writes an explicit depth (`@builtin(frag_depth)`); it relies entirely on the
/// bounding [`Cuboid`] mesh's own rasterized geometry depth for all depth-based behavior —
/// standard opaque depth sorting between chunk entities, the depth prepass a consumer adds via
/// `DepthPrepass`, and any `OcclusionCulling` built on top of it (see this crate's own examples
/// for why those two components are worth adding to a real camera).
///
/// For NON-overlapping chunks this is exactly correct, not just an approximation: a ray marched
/// inside one chunk can only ever hit a point within that chunk's own cuboid extent (the DDA is
/// confined to `[0, dims]`), so its true hit depth always falls between that cuboid's own near
/// and far face depth. Two chunks that don't overlap in space therefore have a well-defined,
/// correct relative depth order even though the depth buffer only ever holds cuboid-geometry
/// depth, never the true per-pixel voxel-surface depth.
///
/// That guarantee breaks if two spawned chunks' cuboids DO overlap: the depth buffer would then
/// resolve visibility by whichever cuboid's *geometry* is nearer, which need not match which
/// chunk's *actual ray-marched surface* is nearer at a given pixel — a real, silent rendering bug
/// this function has no way to detect or reject (it has no visibility into other chunks already
/// spawned, and deliberately doesn't own a world/chunk registry that could check — see this
/// crate's own examples for why that abstraction is deferred). If overlapping placement is ever a
/// real need (e.g. a streaming/loading transition blending two chunks), the fragment shader would
/// need to write a real `frag_depth` from the ray-marched hit — not attempted here, since doing so
/// naively would very likely disable hardware early-Z on the MAIN pass the same way an unguarded
/// `discard` already does, undoing a real, measured win this engine's own examples depend on.
#[allow(clippy::too_many_arguments)]
pub fn spawn_voxel_chunk(
    commands: &mut Commands,
    meshes: &mut Assets<Mesh>,
    materials: &mut Assets<VoxelMaterial>,
    images: &mut Assets<Image>,
    buffers: &mut Assets<ShaderBuffer>,
    chunk: &VoxelChunk,
    palette: &VoxelPalette,
    transform: Transform,
) -> Entity {
    let dims = chunk.dims();
    let mesh = meshes.add(Cuboid::new(dims.x as f32, dims.y as f32, dims.z as f32));

    let voxel_data = images.add(build_voxel_image(chunk));
    let brick_occupancy = images.add(build_occupancy_image(chunk));

    let material = materials.add(VoxelMaterial::new(
        chunk,
        palette,
        transform.translation,
        transform.scale.x,
        voxel_data,
        brick_occupancy,
        buffers,
    ));

    // `Cuboid` is centered on its own local origin, but voxel-index space (what the shader
    // marches through) spans `[0, dims]` from a corner — shift the rendered mesh by half its
    // extent so that corner, not the mesh's center, lands at `transform.translation`.
    let half_extent_local = Vec3::new(dims.x as f32, dims.y as f32, dims.z as f32) / 2.0;
    let mesh_transform = transform.with_translation(
        transform.translation + transform.rotation * (half_extent_local * transform.scale.x),
    );

    commands
        .spawn((Mesh3d(mesh), MeshMaterial3d(material), mesh_transform))
        .id()
}
