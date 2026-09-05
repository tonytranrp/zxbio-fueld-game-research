#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include "world/chunk/material.hpp"

namespace world::svo {

// One 8x8x8 brick of the sparse-brick octree (docs/goals.md Group W, research/micro-voxel-pivot-
// log.md §2): the leaf payload the tree's bricked-SVO structure points at (Kämpe/Laine "bricked
// SVO" family -- GigaVoxels-style leaves holding a fixed voxel block instead of a single voxel).
// Word layout is the GPU layout verbatim -- the CPU reference ray marcher and the HLSL marcher
// read the same 144 uint32 words:
//   words [0, 16):   512-bit occupancy mask, bit i = voxel i is not Air (linear index below)
//   words [16, 144): 512 material bytes (world::chunk::MaterialID), 4 per word, little-endian
//                    byte order inside a word (byte k of voxel index i lives at bits 8*(i%4))
// The mask exists purely so the marcher's per-step test touches 64 bytes (1-2 cache lines) instead
// of 512, and so the mask alone answers "anything here?" -- the material is read only on a hit.
inline constexpr int kBrickEdge = 8;
inline constexpr int kBrickEdgeLog2 = 3;
inline constexpr std::size_t kBrickVoxels = 512;
inline constexpr std::size_t kBrickMaskWords = kBrickVoxels / 32;                 // 16
inline constexpr std::size_t kBrickMaterialWords = kBrickVoxels / 4;              // 128
inline constexpr std::size_t kBrickWords = kBrickMaskWords + kBrickMaterialWords; // 144

// Linear voxel index, X innermost (the chunk module's own convention) -- keep bit-compatible
// with svo_march.psh.hlsl's BrickIndex().
[[nodiscard]] constexpr std::size_t brick_voxel_index(int x, int y, int z) noexcept {
    return static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * kBrickEdge +
           static_cast<std::size_t>(z) * kBrickEdge * kBrickEdge;
}

// Raw-word accessors: the traversal runs on flat uint32 arrays (the serialized tree), so these
// take a pointer to the brick's first word rather than a Brick object.
[[nodiscard]] inline bool brick_word_occupied(const std::uint32_t* words, std::size_t index) noexcept {
    return ((words[index >> 5] >> (index & 31u)) & 1u) != 0u;
}

[[nodiscard]] inline world::chunk::MaterialID brick_word_material(const std::uint32_t* words,
                                                                  std::size_t index) noexcept {
    const std::uint32_t word = words[kBrickMaskWords + (index >> 2)];
    return static_cast<world::chunk::MaterialID>((word >> ((index & 3u) * 8u)) & 0xFFu);
}

inline void brick_word_set(std::uint32_t* words, std::size_t index,
                           world::chunk::MaterialID material) noexcept {
    const auto m = static_cast<std::uint32_t>(material);
    const std::uint32_t shift = (index & 3u) * 8u;
    std::uint32_t& mw = words[kBrickMaskWords + (index >> 2)];
    mw = (mw & ~(0xFFu << shift)) | (m << shift);
    std::uint32_t& bit = words[index >> 5];
    if (material == world::chunk::MaterialID::Air) {
        bit &= ~(1u << (index & 31u));
    } else {
        bit |= 1u << (index & 31u);
    }
}

// Value-type brick for the builder and tests. Starts all-Air (all-zero words).
class Brick {
public:
    [[nodiscard]] world::chunk::MaterialID at(int x, int y, int z) const noexcept {
        return brick_word_material(words_.data(), brick_voxel_index(x, y, z));
    }
    [[nodiscard]] world::chunk::MaterialID at(std::size_t index) const noexcept {
        return brick_word_material(words_.data(), index);
    }
    [[nodiscard]] bool occupied(int x, int y, int z) const noexcept {
        return brick_word_occupied(words_.data(), brick_voxel_index(x, y, z));
    }
    void set(int x, int y, int z, world::chunk::MaterialID material) noexcept {
        brick_word_set(words_.data(), brick_voxel_index(x, y, z), material);
    }
    void set(std::size_t index, world::chunk::MaterialID material) noexcept {
        brick_word_set(words_.data(), index, material);
    }
    void clear() noexcept { words_.fill(0u); }

    [[nodiscard]] std::size_t occupied_count() const noexcept {
        std::size_t n = 0;
        for (std::size_t w = 0; w < kBrickMaskWords; ++w) {
            n += static_cast<std::size_t>(std::popcount(words_[w]));
        }
        return n;
    }
    [[nodiscard]] bool empty() const noexcept { return occupied_count() == 0; }

    // True when every voxel holds the same material (Air included) -- such a brick never needs
    // to exist as a brick leaf; the builder collapses it to a solid leaf / absent child.
    [[nodiscard]] bool is_homogeneous() const noexcept {
        const world::chunk::MaterialID first = at(std::size_t{0});
        for (std::size_t i = 1; i < kBrickVoxels; ++i) {
            if (at(i) != first) {
                return false;
            }
        }
        return true;
    }

    // The material a distant viewer would see: majority over the TOPMOST occupied voxel of each
    // of the 64 columns (what a heightmap world exposes from above). Air only if nothing is
    // occupied. Used as the LOD-cube shading material carried in the parent node header.
    [[nodiscard]] world::chunk::MaterialID representative() const noexcept;

    [[nodiscard]] const std::array<std::uint32_t, kBrickWords>& words() const noexcept { return words_; }
    [[nodiscard]] std::array<std::uint32_t, kBrickWords>& words() noexcept { return words_; }

private:
    std::array<std::uint32_t, kBrickWords> words_{};
};

} // namespace world::svo
