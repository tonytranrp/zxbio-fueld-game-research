//! Integration test exercising the crate's public API only, the way a real external consumer
//! would -- no access to any `pub(crate)` internals.

use bevy::math::{UVec3, Vec3};
use voxel_engine::{cast_ray, VoxelChunk, VoxelId, VoxelMaterialInfo, VoxelPalette};

#[test]
fn author_edit_and_query_a_chunk_through_the_public_api() {
    let mut chunk = VoxelChunk::new(UVec3::splat(32));
    assert_eq!(chunk.dims(), UVec3::splat(32));
    assert_eq!(chunk.get(UVec3::new(5, 5, 5)), VoxelId::AIR);

    chunk.set(UVec3::new(16, 16, 16), VoxelId::new(9));
    assert_eq!(chunk.get(UVec3::new(16, 16, 16)), VoxelId::new(9));

    let hit = cast_ray(&chunk, Vec3::new(16.5, 16.5, -5.0), Vec3::Z, 100.0)
        .expect("expected the ray to hit the voxel just placed");
    assert_eq!(hit.voxel, UVec3::new(16, 16, 16));
    assert_eq!(hit.material, VoxelId::new(9));
}

#[test]
fn palette_maps_material_ids_to_colors_through_the_public_api() {
    let mut palette = VoxelPalette::new();
    let lava = VoxelMaterialInfo {
        color: bevy::color::LinearRgba::new(1.0, 0.3, 0.0, 1.0),
    };
    palette.set(VoxelId::new(3), lava);

    assert_eq!(palette.get(VoxelId::new(3)).color, lava.color);
    // An unset entry still resolves to a real, visible default rather than a missing value.
    assert_eq!(
        palette.get(VoxelId::new(200)).color,
        VoxelMaterialInfo::default().color
    );
}
