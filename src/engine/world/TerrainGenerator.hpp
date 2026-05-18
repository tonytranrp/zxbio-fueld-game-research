#pragma once

#include "engine/core/Types.hpp"
#include <cmath>
#include <vector>

namespace biofuel::engine::world {

// =============================================================================
// HeightmapData — flattened 2D array of height values, row-major (x + y*width)
// =============================================================================
struct HeightmapData {
    std::vector<f32> heights;
    i32 width = 0;
    i32 height = 0;

    [[nodiscard]] f32 at(const i32 x, const i32 y) const noexcept {
        return heights[static_cast<usize>(y * width + x)];
    }

    [[nodiscard]] f32& at(const i32 x, const i32 y) noexcept {
        return heights[static_cast<usize>(y * width + x)];
    }

    [[nodiscard]] bool empty() const noexcept { return heights.empty(); }
    [[nodiscard]] usize size() const noexcept { return heights.size(); }
};

// =============================================================================
// BiomeCell — material classification for a single world tile
// =============================================================================
enum class BiomeMaterial : u8 {
    DeepWater,
    ShallowWater,
    Sand,
    Grass,
    Forest,
    Dirt,
    Stone,
    Snow,
};

struct BiomeCell {
    BiomeMaterial material = BiomeMaterial::Grass;
};

struct BiomeMap {
    std::vector<BiomeCell> cells;
    i32 width = 0;
    i32 height = 0;

    [[nodiscard]] const BiomeCell& at(const i32 x, const i32 y) const noexcept {
        return cells[static_cast<usize>(y * width + x)];
    }

    [[nodiscard]] BiomeCell& at(const i32 x, const i32 y) noexcept {
        return cells[static_cast<usize>(y * width + x)];
    }

    [[nodiscard]] bool empty() const noexcept { return cells.empty(); }
};

// =============================================================================
// TerrainGenerator — procedural heightmap generation using sinusoidal noise.
// FastNoiseLite is planned as a future upgrade; sin/cos provides test data.
// =============================================================================
class TerrainGenerator {
public:
    struct Config {
        f32 frequencyX = 0.15f;
        f32 frequencyZ = 0.15f;
        f32 amplitude = 5.0f;
        f32 baseHeight = 0.5f;
        u32 octaves = 3;
        f32 persistence = 0.5f;
    };

    explicit TerrainGenerator(const Config& config = {}) noexcept
        : m_config(config) {}

    // -------------------------------------------------------------------------
    // Generate a heightmap of [width × height] using layered sin/cos noise.
    // Each octave doubles the frequency and halves the amplitude contribution.
    // -------------------------------------------------------------------------
    [[nodiscard]] HeightmapData generateHeightmap(
        i32 width,
        i32 height,
        u32 seed) const;

    // -------------------------------------------------------------------------
    // Classify each cell into a BiomeMaterial based on height thresholds.
    // -------------------------------------------------------------------------
    [[nodiscard]] BiomeMap generateBiomeMap(const HeightmapData& heights) const;

    [[nodiscard]] const Config& config() const noexcept { return m_config; }
    void setConfig(const Config& config) noexcept { m_config = config; }

private:
    [[nodiscard]] f32 sampleNoise(i32 x, i32 z, u32 seed) const noexcept;

    Config m_config;
};

} // namespace biofuel::engine::world
