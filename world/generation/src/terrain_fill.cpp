#include "world/generation/terrain_fill.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "world/chunk/chunk_coord.hpp"

namespace world::generation {

using world::chunk::ChunkCoord;
using world::chunk::kChunkSize;
using world::chunk::local_index;
using world::chunk::MaterialID;

namespace {

// Group M surface banding (goals 81/93/95's design): materials are a pure function of
// (column surface height, depth below surface, local slope), so they are seam-consistent by
// construction -- every chunk computing a shared column gets identical answers.
constexpr float kBeachBand = 1.75f;    // columns with surface <= sea+band get sand, not grass
constexpr std::int32_t kSoilDepth = 3; // dirt below the surface voxel down to this depth
constexpr float kGrassMaxSlope = 1.9f; // steeper columns read as exposed rock (no grass skin)

} // namespace

void fill_terrain(world::chunk::Chunk& chunk, const HeightmapGenerator& heightmap, const TerrainFillParams& params) {
    const ChunkCoord& coord = chunk.coord();
    const std::int32_t worldXOffset = coord.x * kChunkSize;
    const std::int32_t worldZOffset = coord.z * kChunkSize;
    const std::int32_t worldYBase = coord.y * kChunkSize;
    const std::int32_t worldYTop = worldYBase + kChunkSize - 1;

    // 34x34: one column of margin on every side so per-column SLOPE (central difference against
    // real neighbor columns) is exact at chunk borders too -- a clamped-edge approximation would
    // make grass-vs-rock decisions differ across a chunk seam.
    constexpr std::int32_t kGrid = kChunkSize + 2;
    std::array<float, static_cast<std::size_t>(kGrid) * static_cast<std::size_t>(kGrid)> columnHeights{};
    const HeightmapMinMax minMax = heightmap.generate_column_heights(worldXOffset - 1, worldZOffset - 1, kGrid, kGrid,
                                                                     columnHeights.data());
    const auto heightAt = [&](std::int32_t lx, std::int32_t lz) {
        return columnHeights[static_cast<std::size_t>(lz + 1) * static_cast<std::size_t>(kGrid) +
                             static_cast<std::size_t>(lx + 1)];
    };

    const auto minSurface = static_cast<std::int32_t>(minMax.min);
    const auto maxSurface = static_cast<std::int32_t>(minMax.max);

    // Whole chunk strictly above every column's surface AND above sea level -> pure air, which is
    // already the default a freshly constructed ChunkVoxels starts as -- nothing to do.
    if (worldYBase > maxSurface && worldYBase > params.seaLevel) {
        return;
    }
    // Whole chunk strictly below every column's SOIL band -> pure stone, O(1), no per-voxel loop
    // (§2.4/§4's min/max short-circuit). The extra kSoilDepth+1 margin keeps this consistent with
    // the banded fill below: a chunk whose top voxel could still be grass/dirt must take the real
    // per-voxel path. (minMax covers the margin columns too -- conservative, never wrong.)
    if (worldYTop < minSurface - (kSoilDepth + 1)) {
        chunk.voxels().fill_uniform(MaterialID::Stone);
        return;
    }

    // Straddles the surface/soil band and/or sea level for at least one column: real per-voxel
    // fill. Only non-Air voxels are ever set() -- Air is already every voxel's default.
    for (std::int32_t lz = 0; lz < kChunkSize; ++lz) {
        for (std::int32_t lx = 0; lx < kChunkSize; ++lx) {
            const float surfaceHeightF = heightAt(lx, lz);
            const auto surfaceHeight = static_cast<std::int32_t>(surfaceHeightF);

            // Central-difference slope from the margin-complete grid (seam-exact).
            const float slopeX = std::abs(heightAt(lx + 1, lz) - heightAt(lx - 1, lz)) * 0.5f;
            const float slopeZ = std::abs(heightAt(lx, lz + 1) - heightAt(lx, lz - 1)) * 0.5f;
            const float slope = std::max(slopeX, slopeZ);

            const bool beach = surfaceHeightF <= static_cast<float>(params.seaLevel) + kBeachBand;
            const bool grassy = !beach && slope <= kGrassMaxSlope;

            for (std::int32_t ly = 0; ly < kChunkSize; ++ly) {
                const std::int32_t worldY = worldYBase + ly;
                MaterialID material = MaterialID::Air;
                if (worldY <= surfaceHeight) {
                    const std::int32_t depth = surfaceHeight - worldY;
                    if (depth == 0) {
                        material = beach ? MaterialID::Sand : (grassy ? MaterialID::Grass : MaterialID::Stone);
                    } else if (depth <= kSoilDepth) {
                        material = beach ? MaterialID::Sand : MaterialID::Dirt;
                    } else {
                        material = MaterialID::Stone;
                    }
                } else if (worldY <= params.seaLevel) {
                    material = MaterialID::Water; // water never overrides solid ground (§4)
                }
                if (material != MaterialID::Air) {
                    chunk.voxels().set(local_index(lx, ly, lz), material);
                }
            }
        }
    }
}

} // namespace world::generation
