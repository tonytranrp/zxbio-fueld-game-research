#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "world/chunk/chunk_voxels.hpp" // kChunkSize

namespace world::chunk {

inline constexpr std::int32_t kChunkShift = 5; // log2(kChunkSize); kChunkSize must stay 32
inline constexpr std::int32_t kChunkMask = kChunkSize - 1;

struct ChunkCoord {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend bool operator==(const ChunkCoord&, const ChunkCoord&) = default;
};

// World-voxel-space -> chunk coordinate / local-voxel-offset. Correct for negative coordinates by
// construction -- arithmetic right-shift + mask on a power-of-two chunk size, well-defined by
// C++20's two's-complement standardization (M1.2 brief §3) -- not by a branch.
inline std::int32_t world_to_chunk(std::int32_t worldVoxelCoord) noexcept {
    return worldVoxelCoord >> kChunkShift;
}

inline std::int32_t world_to_local(std::int32_t worldVoxelCoord) noexcept {
    return worldVoxelCoord & kChunkMask;
}

// Row-major, X innermost then Y then Z -- matches FastNoise2's own GenUniformGrid3D output
// convention (out[(z*yCount+y)*xCount+x]), kept consistent for whenever a full 3D density grid
// is generated directly into chunk-local storage.
inline std::size_t local_index(std::int32_t lx, std::int32_t ly, std::int32_t lz) noexcept {
    return static_cast<std::size_t>(lx) + static_cast<std::size_t>(ly) * static_cast<std::size_t>(kChunkSize) +
           static_cast<std::size_t>(lz) * static_cast<std::size_t>(kChunkSize) * static_cast<std::size_t>(kChunkSize);
}

} // namespace world::chunk

template <>
struct std::hash<world::chunk::ChunkCoord> {
    std::size_t operator()(const world::chunk::ChunkCoord& c) const noexcept {
        std::size_t h = std::hash<std::int32_t>{}(c.x);
        h = h * 31 + std::hash<std::int32_t>{}(c.y);
        h = h * 31 + std::hash<std::int32_t>{}(c.z);
        return h;
    }
};
