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
