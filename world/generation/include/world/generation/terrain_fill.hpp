#pragma once

#include <cstdint>

#include "world/chunk/chunk.hpp"
#include "world/generation/heightmap_generator.hpp"

namespace world::generation {

struct TerrainFillParams {
    std::int32_t seaLevel = 0;
};

// Fills `chunk` in place from `heightmap` (M1.2 brief §4): samples one surface height per
// (worldX, worldZ) column, short-circuits to a uniform Air/Stone chunk via the heightmap's own
// min/max (§2.4) before touching a single voxel where possible, and falls through to a per-voxel
// fill only for chunks that actually straddle the generated surface or sea level. Deliberately no
// stored density -- occupancy is read directly off material identity (material != Air); see §4
// for why.
void fill_terrain(world::chunk::Chunk& chunk, const HeightmapGenerator& heightmap, const TerrainFillParams& params = {});

} // namespace world::generation
