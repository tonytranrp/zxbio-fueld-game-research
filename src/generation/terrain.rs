//! Heightmap-based terrain generation on top of [`PerlinNoise`] — the standard, well-understood
//! pattern (sample 2D noise per column, fill solid up to the resulting height) as the engine's
//! first real content generator, replacing hand-authored/hand-stamped test scenes with something
//! that can actually approach the scale the engine is meant for.

use bevy::math::IVec3;

use super::noise::PerlinNoise;
use crate::storage::{VoxelChunk, VoxelId};

/// Tuning parameters for [`fill_heightmap_terrain`].
#[derive(Debug, Clone, Copy)]
pub struct HeightmapParams {
    /// How quickly the noise field varies across the X/Z plane — smaller values (e.g. `0.01`)
    /// give broad, gentle hills; larger values (e.g. `0.2`) give cramped, busy terrain.
    pub frequency: f32,
    /// How many voxels of height variation the noise field contributes, above/below
    /// `base_height`.
    pub amplitude: f32,
    /// The height (in voxels) the terrain centers on before noise is added.
    pub base_height: f32,
    /// Fractal Brownian motion octaves (see [`PerlinNoise::sample_fbm`]) — `1` is a single flat
    /// noise layer, higher values add progressively finer detail on top of the same broad shape.
    pub octaves: u32,
}

impl Default for HeightmapParams {
    fn default() -> Self {
        Self {
            frequency: 0.03,
            amplitude: 16.0,
            base_height: 32.0,
            octaves: 4,
        }
    }
}

/// Fills `chunk` with heightmap terrain: for each `(x, z)` column, samples `noise` (via
/// [`PerlinNoise::sample_fbm`], using a fixed Y slice as the 2D height source) to get a height,
/// then sets every voxel from `y = 0` up to that height to `material` — everything above stays
/// air. `chunk` is not cleared first; terrain is only ever added, never overwritten with air, so
/// this composes with content already placed (call it before hand-authored detail, not after, if
/// you want the detail to survive).
///
/// `world_origin` is `chunk`'s own position in WORLD voxel-index space — noise is sampled at
/// `world_origin + local_xz`, not at the chunk-local coordinate alone, specifically so multiple
/// adjacent chunks generate seamlessly-continuous terrain rather than each restarting the noise
/// field from its own local `(0, 0)` (see `adjacent_chunks_produce_seamless_terrain_matching_a_
/// single_larger_chunk` for the exact property this guarantees). Pass [`IVec3::ZERO`] for a
/// single standalone chunk with no world position of its own.
pub fn fill_heightmap_terrain(chunk: &mut VoxelChunk, noise: &PerlinNoise, params: HeightmapParams, world_origin: IVec3, material: VoxelId) {
    let dims = chunk.dims();

    for z in 0..dims.z {
        for x in 0..dims.x {
            let world_x = world_origin.x + x as i32;
            let world_z = world_origin.z + z as i32;
            let sample = noise.sample_fbm(world_x as f32 * params.frequency, 0.0, world_z as f32 * params.frequency, params.octaves);
            let height = (sample * params.amplitude + params.base_height).max(0.0);
            let height_voxels = (height.floor() as u32).min(dims.y);

            // fill_column, not a per-voxel set() loop: measured to matter a great deal (see this
            // function's own history / the engine's procedural-generation memory) -- set()'s own
            // brick/mip bookkeeping, repeated once per voxel instead of once per brick, was
            // roughly 80% of this function's total cost before this change.
            chunk.fill_column(x, z, 0, height_voxels, material);
        }
    }
}

#[cfg(test)]
mod tests {
    use bevy::math::UVec3;

    use super::*;

    #[test]
    fn fills_a_chunk_with_terrain_that_stays_within_the_configured_height_range() {
        let mut chunk = VoxelChunk::new(UVec3::splat(64));
        let noise = PerlinNoise::new(11);
        let params = HeightmapParams {
            frequency: 0.05,
            amplitude: 10.0,
            base_height: 20.0,
            octaves: 3,
        };
        fill_heightmap_terrain(&mut chunk, &noise, params, IVec3::ZERO, VoxelId::new(1));

        // Every column's solid material should stop somewhere within [base - amplitude - 1,
        // base + amplitude + 1] (a little slack for the fbm normalization not being perfectly
        // exact) -- and never appear above that, never leave a solid voxel floating with air
        // beneath it (terrain fills from y=0 up, so this also catches an off-by-one that skipped
        // the bottom of a column).
        let min_expected = (params.base_height - params.amplitude - 1.0).max(0.0) as u32;
        let max_expected = (params.base_height + params.amplitude + 1.0) as u32;

        for z in 0..64u32 {
            for x in 0..64u32 {
                let mut highest_solid = None;
                for y in 0..64u32 {
                    let solid = !chunk.get(UVec3::new(x, y, z)).is_air();
                    if solid {
                        highest_solid = Some(y);
                        assert!(
                            (0..=y).all(|below| !chunk.get(UVec3::new(x, below, z)).is_air()),
                            "column ({x},{z}) has a solid voxel at y={y} with air beneath it"
                        );
                    }
                }
                if let Some(top) = highest_solid {
                    assert!(
                        top >= min_expected.saturating_sub(1) && top <= max_expected,
                        "column ({x},{z}) height {top} outside expected [{min_expected}, {max_expected}]"
                    );
                }
            }
        }
    }

    #[test]
    fn does_not_clear_existing_content_above_the_generated_height() {
        let mut chunk = VoxelChunk::new(UVec3::splat(32));
        chunk.set(UVec3::new(5, 31, 5), VoxelId::new(9));

        let noise = PerlinNoise::new(1);
        let params = HeightmapParams {
            frequency: 0.05,
            amplitude: 4.0,
            base_height: 4.0,
            octaves: 2,
        };
        fill_heightmap_terrain(&mut chunk, &noise, params, IVec3::ZERO, VoxelId::new(1));

        assert_eq!(chunk.get(UVec3::new(5, 31, 5)), VoxelId::new(9));
    }

    #[test]
    fn deterministic_for_the_same_noise_and_params() {
        let params = HeightmapParams::default();
        let mut a = VoxelChunk::new(UVec3::splat(32));
        let mut b = VoxelChunk::new(UVec3::splat(32));
        let noise = PerlinNoise::new(55);

        fill_heightmap_terrain(&mut a, &noise, params, IVec3::ZERO, VoxelId::new(1));
        fill_heightmap_terrain(&mut b, &noise, params, IVec3::ZERO, VoxelId::new(1));

        for z in 0..32u32 {
            for y in 0..32u32 {
                for x in 0..32u32 {
                    let pos = UVec3::new(x, y, z);
                    assert_eq!(a.get(pos), b.get(pos));
                }
            }
        }
    }

    #[test]
    fn adjacent_chunks_produce_seamless_terrain_matching_a_single_larger_chunk() {
        // The whole point of `world_origin`: two adjacent chunks generated independently must
        // produce EXACTLY what a single larger chunk spanning the same world-space region would
        // -- not just "plausible-looking" terrain, but voxel-for-voxel identical, since that's
        // the actual property multi-chunk worlds depend on (no visible seam/discontinuity at a
        // chunk boundary).
        let noise = PerlinNoise::new(7);
        let params = HeightmapParams::default();

        let mut whole = VoxelChunk::new(UVec3::new(256, 64, 128));
        fill_heightmap_terrain(&mut whole, &noise, params, IVec3::ZERO, VoxelId::new(1));

        let mut left = VoxelChunk::new(UVec3::new(128, 64, 128));
        fill_heightmap_terrain(&mut left, &noise, params, IVec3::ZERO, VoxelId::new(1));

        let mut right = VoxelChunk::new(UVec3::new(128, 64, 128));
        fill_heightmap_terrain(&mut right, &noise, params, IVec3::new(128, 0, 0), VoxelId::new(1));

        for z in 0..128u32 {
            for y in 0..64u32 {
                for x in 0..128u32 {
                    let whole_left = whole.get(UVec3::new(x, y, z));
                    let whole_right = whole.get(UVec3::new(x + 128, y, z));
                    assert_eq!(
                        left.get(UVec3::new(x, y, z)),
                        whole_left,
                        "left chunk diverges from the reference at local ({x},{y},{z})"
                    );
                    assert_eq!(
                        right.get(UVec3::new(x, y, z)),
                        whole_right,
                        "right chunk diverges from the reference at local ({x},{y},{z}) (world x={})",
                        x + 128
                    );
                }
            }
        }
    }

    /// Splits `fill_heightmap_terrain`'s own cost between noise sampling and
    /// `VoxelChunk::set()` bookkeeping, to answer a question worth resolving BEFORE investing in
    /// SIMD-optimized noise (the next queued step per the engine's own SIMD research): for a
    /// 128-voxel chunk, a full terrain fill takes ~16,384 noise samples (one `sample_fbm` per
    /// X/Z column) but potentially ~10x-100x more `set()` calls (one per SOLID VOXEL placed, not
    /// per column -- a column reaching height 60 is 60 individual `set()` calls, each with its
    /// own bounds check, old/new comparison, and -- for the voxel that flips a brick's own
    /// occupied/empty status -- mip-hierarchy propagation). If `set()` overhead turns out to
    /// dominate, SIMD-optimizing the noise function alone would barely move the needle on overall
    /// generation time, and a bulk-fill API on `VoxelChunk` (updating occupancy once per BRICK
    /// touched by a contiguous fill, not once per voxel) would be the better next investment.
    ///
    /// `#[ignore]`d for the same reason as the raymarch ablation benchmark: a timing measurement,
    /// not a correctness check, meaningless in debug builds. Run via `cargo test --release --lib
    /// -- --ignored --nocapture generation_cost_split` (through `rtk proxy`, not plain `cargo
    /// test`, or the printed numbers won't appear -- see the engine's own tooling notes).
    #[test]
    #[ignore = "timing measurement, not a correctness check -- run explicitly, see this test's own doc comment"]
    fn generation_cost_split_between_noise_sampling_and_voxel_placement() {
        let noise = PerlinNoise::new(1);
        let params = HeightmapParams::default();
        let dims = UVec3::splat(128);

        // Warm up.
        for z in 0..dims.z {
            for x in 0..dims.x {
                std::hint::black_box(noise.sample_fbm(
                    x as f32 * params.frequency,
                    0.0,
                    z as f32 * params.frequency,
                    params.octaves,
                ));
            }
        }

        // Pure noise-sampling cost: exactly the same sample_fbm calls fill_heightmap_terrain
        // makes internally, with zero VoxelChunk interaction.
        let mut total_height_voxels: u64 = 0;
        let start = std::time::Instant::now();
        for z in 0..dims.z {
            for x in 0..dims.x {
                let sample = std::hint::black_box(noise.sample_fbm(
                    x as f32 * params.frequency,
                    0.0,
                    z as f32 * params.frequency,
                    params.octaves,
                ));
                let height = (sample * params.amplitude + params.base_height).max(0.0);
                total_height_voxels += (height.floor() as u32).min(dims.y) as u64;
            }
        }
        let noise_only = start.elapsed();

        // Full generation: the same noise sampling, plus VoxelChunk::set() for every solid
        // voxel -- total_height_voxels calls' worth, computed above from the identical noise.
        let start = std::time::Instant::now();
        let mut chunk = VoxelChunk::new(dims);
        fill_heightmap_terrain(&mut chunk, &noise, params, IVec3::ZERO, VoxelId::new(1));
        let full = start.elapsed();

        let implied_set_cost = full.saturating_sub(noise_only);
        println!("chunk: {}^3, {total_height_voxels} solid voxels placed (~{} set() calls)", dims.x, total_height_voxels);
        println!("noise sampling only: {noise_only:?} ({} samples)", dims.x * dims.z * params.octaves);
        println!("full generation:     {full:?}");
        println!(
            "implied set() cost:  {implied_set_cost:?} ({:.1}% of total, ~{:.1} ns/set call)",
            100.0 * implied_set_cost.as_secs_f64() / full.as_secs_f64(),
            implied_set_cost.as_nanos() as f64 / total_height_voxels.max(1) as f64,
        );
    }
}
