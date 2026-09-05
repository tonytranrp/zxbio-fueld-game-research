#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <vector>

#include "world/chunk/material.hpp"

namespace world::chunk {

inline constexpr std::int32_t kChunkSize = 32;
inline constexpr std::size_t kVoxelsPerChunk = static_cast<std::size_t>(kChunkSize) *
                                               static_cast<std::size_t>(kChunkSize) *
                                               static_cast<std::size_t>(kChunkSize);

// Paletted voxel storage for one chunk (M1.2 brief §1): a small palette of the distinct
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

    // Widens storage to at least `bits` bits/voxel up front, in one O(voxel count) repack from the
    // cheap bits_==0 starting state -- a no-op if already >= bits (never narrows). A caller who
    // already knows (from domain knowledge, not the palette's own growth) that several distinct
    // materials are about to be set() should call this once before its fill loop: without it,
    // promote() re-packs the ENTIRE index buffer from scratch at every bit-width boundary the
    // palette crosses (0->1, 1->2, 2->4), so a chunk that discovers e.g. Stone/Dirt/Sand/Grass/
    // Water in scattered order during a fill loop pays that O(voxel count) cost up to three times
    // redundantly instead of once (goal: real chunk-generation profiling pass, 2026-09-05).
    void reserve_bits(std::uint8_t bits);

    [[nodiscard]] std::size_t palette_size() const noexcept { return palette_.size(); }
    [[nodiscard]] bool is_homogeneous() const noexcept { return palette_.size() == 1; }
    [[nodiscard]] std::uint8_t bits_per_voxel() const noexcept { return bits_; }

    // The palette-size -> bit-width table (§1.2): always a power of two so a voxel's index never
    // straddles a byte boundary. Public and static so callers (e.g. reserve_bits's own call sites)
    // can compute a target bit width from a known upper bound on distinct materials without
    // duplicating this table.
    [[nodiscard]] static constexpr std::uint8_t bits_for_palette_size(std::size_t paletteSize) noexcept {
        if (paletteSize <= 1)
            return 0;
        if (paletteSize <= 2)
            return 1;
        if (paletteSize <= 4)
            return 2;
        if (paletteSize <= 16)
            return 4;
        return 8; // <= 256
    }

private:
    [[nodiscard]] std::size_t palette_index_of(MaterialID material) const;
    void promote(std::uint8_t newBits);

    std::pmr::vector<MaterialID> palette_;
    std::pmr::vector<std::byte> indices_; // empty when palette_.size() == 1
    std::uint8_t bits_ = 0;
};

} // namespace world::chunk
