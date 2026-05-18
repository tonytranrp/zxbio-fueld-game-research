#include "engine/world/TerrainGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace biofuel::engine::world {

HeightmapData TerrainGenerator::generateHeightmap(
    const i32 width,
    const i32 height,
    const u32 seed) const
{
    HeightmapData data;
    data.width = width;
    data.height = height;
    data.heights.resize(static_cast<usize>(width) * static_cast<usize>(height), 0.0f);

    for (i32 y = 0; y < height; ++y) {
        for (i32 x = 0; x < width; ++x) {
            f32 value = m_config.baseHeight;
            f32 freq = 1.0f;
            f32 amp = m_config.amplitude;

            // Multi-octave sinusoidal noise
            for (u32 octave = 0; octave < m_config.octaves; ++octave) {
                const f32 nx = static_cast<f32>(x) * m_config.frequencyX * freq;
                const f32 nz = static_cast<f32>(y) * m_config.frequencyZ * freq;
                const f32 phaseShift = static_cast<f32>(seed + octave * 131U) * 0.618f;

                value += std::sin(nx + phaseShift) * std::cos(nz + phaseShift * 1.3f) * amp;

                freq *= 2.0f;
                amp *= m_config.persistence;
            }

            data.at(x, y) = std::max(value, 0.0f);
        }
    }

    return data;
}

BiomeMap TerrainGenerator::generateBiomeMap(const HeightmapData& heights) const {
    BiomeMap map;
    map.width = heights.width;
    map.height = heights.height;
    map.cells.resize(heights.size());

    // Find min/max for normalization
    f32 minH = std::numeric_limits<f32>::max();
    f32 maxH = std::numeric_limits<f32>::lowest();
    for (usize i = 0; i < heights.size(); ++i) {
        minH = std::min(minH, heights.heights[i]);
        maxH = std::max(maxH, heights.heights[i]);
    }
    const f32 range = (maxH - minH) > 0.001f ? (maxH - minH) : 1.0f;

    for (i32 y = 0; y < heights.height; ++y) {
        for (i32 x = 0; x < heights.width; ++x) {
            const f32 normalized = (heights.at(x, y) - minH) / range;

            BiomeMaterial mat;
            if (normalized < 0.05f) {
                mat = BiomeMaterial::DeepWater;
            } else if (normalized < 0.15f) {
                mat = BiomeMaterial::ShallowWater;
            } else if (normalized < 0.30f) {
                mat = BiomeMaterial::Sand;
            } else if (normalized < 0.55f) {
                mat = BiomeMaterial::Grass;
            } else if (normalized < 0.70f) {
                mat = BiomeMaterial::Forest;
            } else if (normalized < 0.85f) {
                mat = BiomeMaterial::Dirt;
            } else if (normalized < 0.95f) {
                mat = BiomeMaterial::Stone;
            } else {
                mat = BiomeMaterial::Snow;
            }

            map.at(x, y).material = mat;
        }
    }

    return map;
}

f32 TerrainGenerator::sampleNoise(const i32 x, const i32 z, const u32 seed) const noexcept {
    // Simple deterministic hash for per-cell variation (used for detail)
    const u32 h = static_cast<u32>(x) * 374761393U
                + static_cast<u32>(z) * 668265263U
                + seed * 1274126177U;
    const u32 mixed = (h ^ (h >> 13U)) * 1274126177U;
    return static_cast<f32>(mixed & 0xFFFFU) / 65535.0f;
}

} // namespace biofuel::engine::world
