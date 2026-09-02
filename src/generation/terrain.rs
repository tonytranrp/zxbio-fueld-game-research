//! Heightmap-based terrain generation on top of [`PerlinNoise`] — the standard, well-understood
//! pattern (sample 2D noise per column, fill solid up to the resulting height) as the engine's
//! first real content generator, replacing hand-authored/hand-stamped test scenes with something
//! that can actually approach the scale the engine is meant for.

use bevy::math::UVec3;

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
pub fn fill_heightmap_terrain(chunk: &mut VoxelChunk, noise: &PerlinNoise, params: HeightmapParams, material: VoxelId) {
    let dims = chunk.dims();

    for z in 0..dims.z {
        for x in 0..dims.x {
            let sample = noise.sample_fbm(x as f32 * params.frequency, 0.0, z as f32 * params.frequency, params.octaves);
            let height = (sample * params.amplitude + params.base_height).max(0.0);
            let height_voxels = (height.floor() as u32).min(dims.y);

            for y in 0..height_voxels {
                chunk.set(UVec3::new(x, y, z), material);
            }
        }
    }
}

#[cfg(test)]
mod tests {
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
        fill_heightmap_terrain(&mut chunk, &noise, params, VoxelId::new(1));

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
        fill_heightmap_terrain(&mut chunk, &noise, params, VoxelId::new(1));

        assert_eq!(chunk.get(UVec3::new(5, 31, 5)), VoxelId::new(9));
    }

    #[test]
    fn deterministic_for_the_same_noise_and_params() {
        let params = HeightmapParams::default();
        let mut a = VoxelChunk::new(UVec3::splat(32));
        let mut b = VoxelChunk::new(UVec3::splat(32));
        let noise = PerlinNoise::new(55);

        fill_heightmap_terrain(&mut a, &noise, params, VoxelId::new(1));
        fill_heightmap_terrain(&mut b, &noise, params, VoxelId::new(1));

        for z in 0..32u32 {
            for y in 0..32u32 {
                for x in 0..32u32 {
                    let pos = UVec3::new(x, y, z);
                    assert_eq!(a.get(pos), b.get(pos));
                }
            }
        }
    }
}
