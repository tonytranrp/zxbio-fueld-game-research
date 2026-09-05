// Goal 135 (Voxel Representation Redesign SS4.2, Phase 1): the existing per-chunk palette
// compression's real total memory across the full static world at the 48-radius trial size. A
// one-shot report, not a repeatable timing benchmark -- deliberately a plain standalone program
// rather than a Google Benchmark case, since running world generation many times over for
// statistical timing would be pure waste for a question that's purely about the resulting bytes.
#include <cstdint>
#include <cstdio>

#include "world/chunk/chunk.hpp"
#include "world/chunk/chunk_coord.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/streaming/world_bounds.hpp"

using world::chunk::kVoxelsPerChunk;

namespace {

// Mirrors chunk_voxels.cpp's own private packed_byte_count exactly (bits -> index-buffer bytes for
// kVoxelsPerChunk voxels) -- duplicated here deliberately rather than exposing it, since the whole
// point is measuring the PUBLIC contract's (palette_size(), bits_per_voxel()) real cost, the same
// information any external caller could compute, not reaching into ChunkVoxels' own internals.
std::size_t index_buffer_bytes(std::uint8_t bits) {
    if (bits == 0) {
        return 0;
    }
    const std::size_t voxelsPerByte = 8 / bits;
    return (kVoxelsPerChunk + voxelsPerByte - 1) / voxelsPerByte;
}

std::size_t chunk_bytes(const world::chunk::Chunk& chunk) {
    const auto& voxels = chunk.voxels();
    const std::size_t paletteBytes = voxels.palette_size(); // 1 byte per MaterialID entry
    return paletteBytes + index_buffer_bytes(voxels.bits_per_voxel());
}

} // namespace

int main() {
    const world::generation::HeightmapGenerator heightmap(1337);
    const world::streaming::WorldBounds bounds = world::streaming::kDefaultWorldBounds;
    const world::streaming::WorldBounds haloBounds{bounds.radius_chunks + 1, bounds.y_min - 1,
                                                   bounds.y_max + 1};

    std::size_t realBytes = 0;
    std::size_t realCount = 0;
    std::size_t haloOnlyBytes = 0;
    std::size_t haloOnlyCount = 0;
    std::size_t homogeneousCount = 0;

    // Generate the halo set directly (it's a strict superset of the real set); classify each
    // column as "real" or "halo-only" by the same bounds check WorldLoader itself uses.
    for (std::int32_t cz = -haloBounds.radius_chunks; cz <= haloBounds.radius_chunks; ++cz) {
        for (std::int32_t cx = -haloBounds.radius_chunks; cx <= haloBounds.radius_chunks; ++cx) {
            for (std::int32_t cy = haloBounds.y_min; cy <= haloBounds.y_max; ++cy) {
                world::chunk::Chunk chunk{world::chunk::ChunkCoord{cx, cy, cz}};
                world::generation::fill_terrain(chunk, heightmap);
                const std::size_t bytes = chunk_bytes(chunk);
                if (chunk.voxels().is_homogeneous()) {
                    ++homogeneousCount;
                }
                const bool isReal = cx >= -bounds.radius_chunks && cx <= bounds.radius_chunks &&
                                    cz >= -bounds.radius_chunks && cz <= bounds.radius_chunks &&
                                    cy >= bounds.y_min && cy <= bounds.y_max;
                if (isReal) {
                    realBytes += bytes;
                    ++realCount;
                } else {
                    haloOnlyBytes += bytes;
                    ++haloOnlyCount;
                }
            }
        }
    }

    constexpr double kMiB = 1024.0 * 1024.0;
    std::printf("real chunks:      %zu, %.2f MiB voxel storage (%.1f bytes/chunk avg)\n", realCount,
                static_cast<double>(realBytes) / kMiB,
                static_cast<double>(realBytes) / static_cast<double>(realCount));
    std::printf("halo-only chunks: %zu, %.2f MiB voxel storage\n", haloOnlyCount,
                static_cast<double>(haloOnlyBytes) / kMiB);
    std::printf("total (real+halo, as WorldLoader currently retains both forever): %.2f MiB\n",
                static_cast<double>(realBytes + haloOnlyBytes) / kMiB);
    std::printf("homogeneous (single-material, near-zero-cost) chunks: %zu / %zu (%.1f%%)\n",
                homogeneousCount, realCount + haloOnlyCount,
                100.0 * static_cast<double>(homogeneousCount) /
                    static_cast<double>(realCount + haloOnlyCount));
    return 0;
}
