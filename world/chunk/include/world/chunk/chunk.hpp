#pragma once

#include <memory_resource>

#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_voxels.hpp"

namespace world::chunk {

class Chunk {
public:
    explicit Chunk(ChunkCoord coord, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : coord_(coord), voxels_(resource) {}

    [[nodiscard]] const ChunkCoord& coord() const noexcept { return coord_; }
    [[nodiscard]] ChunkVoxels& voxels() noexcept { return voxels_; }
    [[nodiscard]] const ChunkVoxels& voxels() const noexcept { return voxels_; }

private:
    ChunkCoord coord_;
    ChunkVoxels voxels_;
};

} // namespace world::chunk
