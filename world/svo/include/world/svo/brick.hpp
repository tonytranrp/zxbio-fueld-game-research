#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"

namespace world::svo {

// One 8x8x8 brick of the sparse-brick octree (docs/goals.md Group W, research/micro-voxel-pivot-
// log.md §2): the leaf payload the tree's bricked-SVO structure points at (Kämpe/Laine "bricked
// SVO" family -- GigaVoxels-style leaves holding a fixed voxel block instead of a single voxel).
// Word layout is the GPU layout verbatim -- the CPU reference ray marcher and the HLSL marcher
// read the same 70 uint32 words.
//
// PALETTE COMPRESSION (Prompt 004 goal 258 / goal 157, research 7 item 8 -- Teardown ships one
// byte per voxel plus a palette). The old layout spent 512 bytes on materials, one per voxel.
// `tools/palette_probe` measured a real shipping build (64 m, seed 1337, trees on) and found
// 55.2% of bricks hold 2 distinct materials, 43.4% hold 3, 98.6% hold three or fewer, and NOTHING
// exceeds five -- the full table is in research/frame-time-and-gpu-architecture-log.md 7. So the
// materials are stored once per brick and the voxels store an index into them:
//
//   words [0, 16):   512-bit occupancy mask, bit i = voxel i is not Air (linear index below)
//   words [16, 68):  512 three-bit palette indices, TEN PER WORD (30 bits used, top 2 unused)
//   words [68, 70):  the 8-entry palette, one MaterialID byte per entry, 4 per word
//
// TEN PER WORD, not the 10.67 that would fit, is deliberate: 3 does not divide 32, so a densely
// packed index would straddle a word boundary once every ~10 voxels and every material fetch in
// the marcher would need two loads and a shift-select to be correct. Wasting 2 bits per word costs
// four extra words (280 B instead of 264 B, 2.06x instead of 2.18x) and buys a material fetch that
// is exactly one load with no branch -- which is the term goal 258 says to measure, since the
// march is GPU-bound.
//
// PALETTE INVARIANT, and why no separate "entries used" counter exists: entry 0 is ALWAYS Air, and
// an Air voxel always stores index 0. A non-zero entry therefore holds Air if and only if it has
// never been assigned, so `brick_word_set` can intern by scanning for the material and then for
// the first Air -- and an all-zero brick (Brick::clear(), a default-constructed Brick, a zeroed
// pool slot) is already a valid empty brick with an empty palette. Entries 1..7 are seven slots
// for seven possible non-Air materials, which the static_assert below pins to the registry.
inline constexpr int kBrickEdge = 8;
inline constexpr int kBrickEdgeLog2 = 3;
inline constexpr std::size_t kBrickVoxels = 512;
inline constexpr std::size_t kBrickMaskWords = kBrickVoxels / 32; // 16

inline constexpr std::uint32_t kBrickPaletteBits = 3;
inline constexpr std::size_t kBrickPaletteSize = 8; // 1 << kBrickPaletteBits
inline constexpr std::size_t kBrickIndicesPerWord = 10;
inline constexpr std::size_t kBrickIndexWords =
    (kBrickVoxels + kBrickIndicesPerWord - 1) / kBrickIndicesPerWord; // 52
inline constexpr std::size_t kBrickPaletteWords = kBrickPaletteSize / 4; // 2
inline constexpr std::size_t kBrickIndexWord0 = kBrickMaskWords;        // 16
inline constexpr std::size_t kBrickPaletteWord0 = kBrickIndexWord0 + kBrickIndexWords; // 68
inline constexpr std::size_t kBrickWords = kBrickPaletteWord0 + kBrickPaletteWords;    // 70

// The palette holds Air plus one slot per other material. If the registry ever grows past this, a
// single brick could hold more distinct materials than the palette can name and there is no
// non-lossy answer inside a fixed-size brick -- so this fires at BUILD time and forces the
// decision (a wider index, or a second size class in the pool) rather than corrupting a rare
// brick at run time. See the log for both alternatives and their costs.
static_assert(world::chunk::kMaterialCount <= kBrickPaletteSize,
              "brick palette has one entry per material (entry 0 = Air); widen kBrickPaletteBits "
              "or add a fallback size class before adding a ninth material");

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

// One palette entry. Entry 0 is Air by the invariant above.
[[nodiscard]] inline world::chunk::MaterialID brick_palette_entry(const std::uint32_t* words,
                                                                  std::size_t entry) noexcept {
    const std::uint32_t word = words[kBrickPaletteWord0 + (entry >> 2)];
    return static_cast<world::chunk::MaterialID>((word >> ((entry & 3u) * 8u)) & 0xFFu);
}

inline void brick_palette_set(std::uint32_t* words, std::size_t entry,
                              world::chunk::MaterialID material) noexcept {
    const std::uint32_t shift = (entry & 3u) * 8u;
    std::uint32_t& w = words[kBrickPaletteWord0 + (entry >> 2)];
    w = (w & ~(0xFFu << shift)) | (static_cast<std::uint32_t>(material) << shift);
}

// The three-bit index of voxel `index`: ten per word, so exactly one load and no boundary case.
[[nodiscard]] inline std::uint32_t brick_word_index(const std::uint32_t* words,
                                                    std::size_t index) noexcept {
    const std::size_t w = index / kBrickIndicesPerWord;
    const auto shift = static_cast<std::uint32_t>((index - w * kBrickIndicesPerWord) * kBrickPaletteBits);
    return (words[kBrickIndexWord0 + w] >> shift) & 7u;
}

inline void brick_word_index_set(std::uint32_t* words, std::size_t index, std::uint32_t value) noexcept {
    const std::size_t w = index / kBrickIndicesPerWord;
    const auto shift = static_cast<std::uint32_t>((index - w * kBrickIndicesPerWord) * kBrickPaletteBits);
    std::uint32_t& word = words[kBrickIndexWord0 + w];
    word = (word & ~(7u << shift)) | ((value & 7u) << shift);
}

[[nodiscard]] inline world::chunk::MaterialID brick_word_material(const std::uint32_t* words,
                                                                  std::size_t index) noexcept {
    return brick_palette_entry(words, brick_word_index(words, index));
}

inline void brick_word_set(std::uint32_t* words, std::size_t index,
                           world::chunk::MaterialID material) noexcept {
    std::uint32_t& bit = words[index >> 5];
    if (material == world::chunk::MaterialID::Air) {
        bit &= ~(1u << (index & 31u));
        brick_word_index_set(words, index, 0u); // entry 0 is Air; keeps a brick byte-deterministic
        return;
    }
    bit |= 1u << (index & 31u);
    // Intern: an existing entry with this material, else the first never-assigned one. Seven slots
    // for at most seven non-Air materials (the static_assert above), so `free` is always found --
    // the clamp to 1 exists so a hypothetically-overflowing brick shades wrong rather than reading
    // back as Air with its occupancy bit set, which would desync geometry from material.
    std::uint32_t slot = 1u;
    for (std::size_t e = 1; e < kBrickPaletteSize; ++e) {
        const world::chunk::MaterialID held = brick_palette_entry(words, e);
        if (held == material) {
            slot = static_cast<std::uint32_t>(e);
            brick_word_index_set(words, index, slot);
            return;
        }
        if (held == world::chunk::MaterialID::Air) {
            brick_palette_set(words, e, material);
            slot = static_cast<std::uint32_t>(e);
            break;
        }
    }
    brick_word_index_set(words, index, slot);
}

// BULK PACK: 512 material bytes in, a complete brick out -- mask, palette and indices in one
// pass over the scratch. This exists because goal 162 measured `fill_brick` at ~60% of a whole tree
// build and the per-voxel read-modify-write was the single largest term inside it; the palette adds
// an intern step, so the bulk path must stay bulk or the memory win would be paid for in build
// time. The intern is a direct lookup by MaterialID, not a scan of the palette: one load and one
// predictable branch per voxel.
inline void brick_pack_materials(std::uint32_t* words, const std::uint8_t* materials) noexcept {
    for (std::size_t w = 0; w < kBrickWords; ++w) {
        words[w] = 0u;
    }
    std::array<std::int8_t, world::chunk::kMaterialCount> slotOf{};
    slotOf.fill(-1);
    slotOf[static_cast<std::size_t>(world::chunk::MaterialID::Air)] = 0; // the palette invariant
    std::int8_t next = 1;
    std::size_t v = 0;
    for (std::size_t w = 0; w < kBrickIndexWords && v < kBrickVoxels; ++w) {
        std::uint32_t packed = 0u;
        const std::size_t count = std::min(kBrickIndicesPerWord, kBrickVoxels - v);
        for (std::size_t k = 0; k < count; ++k, ++v) {
            const std::size_t m = materials[v];
            std::int8_t slot = slotOf[m];
            if (slot < 0) {
                slot = next++;
                slotOf[m] = slot;
            }
            packed |= static_cast<std::uint32_t>(slot) << (k * kBrickPaletteBits);
            if (m != static_cast<std::size_t>(world::chunk::MaterialID::Air)) {
                words[v >> 5] |= 1u << (v & 31u);
            }
        }
        words[kBrickIndexWord0 + w] = packed;
    }
    for (std::size_t m = 0; m < slotOf.size(); ++m) {
        if (slotOf[m] > 0) {
            brick_palette_set(words, static_cast<std::size_t>(slotOf[m]),
                              static_cast<world::chunk::MaterialID>(m));
        }
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

    // Sum of the outward normals of the brick's EXPOSED faces -- an occupied voxel's face against
    // an empty voxel of the same brick -- in units of one voxel face (Group Z: the per-node
    // average normal the shader blends toward at distance is built bottom-up from this). Faces on
    // the brick's outer boundary have an unknown neighbor and are not counted; the tree builder
    // adds coarse exposure (solid children against absent siblings) at the parent level instead.
    // Water counts as occupied, so a still water surface sums to +y.
    [[nodiscard]] glm::ivec3 exposed_face_sum() const noexcept;

    [[nodiscard]] const std::array<std::uint32_t, kBrickWords>& words() const noexcept { return words_; }
    [[nodiscard]] std::array<std::uint32_t, kBrickWords>& words() noexcept { return words_; }

private:
    std::array<std::uint32_t, kBrickWords> words_{};
};

} // namespace world::svo
