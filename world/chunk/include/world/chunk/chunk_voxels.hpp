#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <vector>

#include "world/chunk/material.hpp"

namespace world::chunk {

inline constexpr std::int32_t kChunkSize = 32;
inline constexpr std::size_t kVoxelsPerChunk =
    static_cast<std::size_t>(kChunkSize) * static_cast<std::size_t>(kChunkSize) * static_cast<std::size_t>(kChunkSize);

// Paletted voxel storage for one chunk (M1_2_BRIEF.md §1): a small palette of the distinct
// materials actually present, plus a bit-packed index per voxel at the minimum width the current
// palette size needs (0/1/2/4/8 bits -- always a power of two so no voxel's index ever straddles
// a byte boundary). palette_.size() == 1 is the common case for real terrain (chunks entirely
// above the generated surface, or entirely below it) and stores no index buffer at all -- a
// freshly constructed chunk starts in exactly that state (implicitly all Air), not something
// fill code has to reach for.
class ChunkVoxels {
public:
    explicit ChunkVoxels(std::pmr::memory_resource* resource = std::pmr::get_default_resource());

    [[nodiscard]] MaterialID at(std::size_t localIndex) const;
    void set(std::size_t localIndex, MaterialID material);

    // Replaces the entire chunk's content with a single uniform material in O(1) -- the direct
    // homogeneous fast path (§1.1/§2.4), not a loop of per-voxel set() calls. Used when the
    // heightmap's own min/max already proves the whole chunk is above or below the surface.
    void fill_uniform(MaterialID material);

    [[nodiscard]] std::size_t palette_size() const noexcept { return palette_.size(); }
    [[nodiscard]] bool is_homogeneous() const noexcept { return palette_.size() == 1; }
    [[nodiscard]] std::uint8_t bits_per_voxel() const noexcept { return bits_; }

private:
    [[nodiscard]] std::size_t palette_index_of(MaterialID material) const;
    void promote(std::uint8_t newBits);

    std::pmr::vector<MaterialID> palette_;
    std::pmr::vector<std::byte> indices_; // empty when palette_.size() == 1
    std::uint8_t bits_ = 0;
};

} // namespace world::chunk
