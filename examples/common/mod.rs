//! Shared test-scene construction for the engine's examples/benchmarks. Not part of the
//! library's own public API -- included directly by each example via `#[path] mod common;`
//! (Cargo's convention for sharing code between examples without it becoming its own example
//! target) so the visual demo and the raw-throughput benchmark measure the identical scenes.

use bevy::math::{IVec3, UVec3};
use voxel_engine::{fill_heightmap_terrain, HeightmapParams, PerlinNoise, VoxelChunk, VoxelId};

pub const CHUNK_VOXELS: u32 = 128;

/// A handful of floating spheres, well separated so most of the volume stays empty air -- the
/// case where the brick/mip occupancy skip should help the most.
pub fn build_sparse_chunk() -> VoxelChunk {
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
pub fn build_dense_chunk() -> VoxelChunk {
    let mut chunk = VoxelChunk::new(UVec3::splat(CHUNK_VOXELS));
    fill_solid(&mut chunk, VoxelId::new(2));
    stamp_sphere(&mut chunk, IVec3::new(64, 64, 64), 40, VoxelId::AIR);
    stamp_sphere(&mut chunk, IVec3::new(20, 20, 20), 15, VoxelId::AIR);
    stamp_sphere(&mut chunk, IVec3::new(108, 108, 108), 15, VoxelId::AIR);
    chunk
}

/// Procedurally generated rolling terrain -- a real, non-toy occupancy profile distinct from
/// both hand-stamped scenes: a mostly-solid lower half (like the dense scene, for rays entering
/// from above) but with genuine empty sky above it (like the sparse scene), and none of it
/// hand-placed. This is the shape "billions of voxels" actually needs to come from -- no one
/// hand-authors that much content.
///
/// `world_origin` positions this chunk in the SAME world-space noise field `fill_heightmap_
/// terrain` samples from -- call this repeatedly with origins offset by `CHUNK_VOXELS` in X/Z
/// to build a multi-chunk world with seamless terrain across chunk boundaries (see that
/// function's own doc comment, and its `adjacent_chunks_produce_seamless_terrain_matching_a_
/// single_larger_chunk` test, for exactly what guarantee this relies on).
pub fn build_terrain_chunk(seed: u64, world_origin: IVec3) -> VoxelChunk {
    let mut chunk = VoxelChunk::new(UVec3::splat(CHUNK_VOXELS));
    let noise = PerlinNoise::new(seed);
    let params = HeightmapParams {
        frequency: 0.025,
        amplitude: 20.0,
        base_height: 40.0,
        octaves: 4,
    };
    fill_heightmap_terrain(&mut chunk, &noise, params, world_origin, VoxelId::new(2));
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
