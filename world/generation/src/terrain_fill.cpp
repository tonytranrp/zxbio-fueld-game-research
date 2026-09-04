#include "world/generation/terrain_fill.hpp"

#include <array>

#include "world/chunk/chunk_coord.hpp"

namespace world::generation {

using world::chunk::ChunkCoord;
using world::chunk::kChunkSize;
using world::chunk::local_index;
using world::chunk::MaterialID;

void fill_terrain(world::chunk::Chunk& chunk, const HeightmapGenerator& heightmap, const TerrainFillParams& params) {
    const ChunkCoord& coord = chunk.coord();
    const std::int32_t worldXOffset = coord.x * kChunkSize;
    const std::int32_t worldZOffset = coord.z * kChunkSize;
    const std::int32_t worldYBase = coord.y * kChunkSize;
    const std::int32_t worldYTop = worldYBase + kChunkSize - 1;

    std::array<float, static_cast<std::size_t>(kChunkSize) * static_cast<std::size_t>(kChunkSize)> columnHeights{};
    const HeightmapMinMax minMax =
        heightmap.generate_column_heights(worldXOffset, worldZOffset, kChunkSize, kChunkSize, columnHeights.data());

    const auto minSurface = static_cast<std::int32_t>(minMax.min);
    const auto maxSurface = static_cast<std::int32_t>(minMax.max);

    // Whole chunk strictly above every column's surface AND above sea level -> pure air, which is
    // already the default a freshly constructed ChunkVoxels starts as -- nothing to do.
    if (worldYBase > maxSurface && worldYBase > params.seaLevel) {
        return;
    }
    // Whole chunk strictly below every column's surface -> pure stone, O(1), no per-voxel loop
    // (§2.4/§4's min/max short-circuit -- the whole reason generate_column_heights returns a
    // min/max instead of just filling the buffer).
    if (worldYTop < minSurface) {
        chunk.voxels().fill_uniform(MaterialID::Stone);
        return;
    }

    // Straddles the surface and/or sea level for at least one column: real per-voxel fill. Only
    // non-Air voxels are ever set() -- Air is already every voxel's default, so this both saves
    // work and keeps chunks that turn out to be homogeneous air, air (no promotion triggered).
    for (std::int32_t lz = 0; lz < kChunkSize; ++lz) {
        for (std::int32_t lx = 0; lx < kChunkSize; ++lx) {
            const float surfaceHeightF = columnHeights[static_cast<std::size_t>(lz) * static_cast<std::size_t>(kChunkSize) +
                                                         static_cast<std::size_t>(lx)];
            const auto surfaceHeight = static_cast<std::int32_t>(surfaceHeightF);

            for (std::int32_t ly = 0; ly < kChunkSize; ++ly) {
                const std::int32_t worldY = worldYBase + ly;
                MaterialID material = (worldY > surfaceHeight) ? MaterialID::Air : MaterialID::Stone;
                if (material == MaterialID::Air && worldY <= params.seaLevel) {
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
