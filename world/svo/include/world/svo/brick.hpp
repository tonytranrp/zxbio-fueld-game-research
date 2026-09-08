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

// THE PALETTE INVARIANT IS PER BRICK, NOT GLOBAL -- and it used to be the other way round.
//
// Until Prompt 007 goal 338 this was a static_assert that kMaterialCount <= 8, on the reasoning
// that seven non-Air slots provably cover seven possible non-Air materials. Goal 338 needs a ninth
// (a grass blade must be Phase::Foliage where ground grass is Phase::Solid, and one material cannot
// be both), so the guarantee had to give. Its own text named two ways out and both are expensive:
//
//   * A WIDER INDEX. Four bits divides 32 exactly -- 8 indices per word, no straddle, no waste --
//     but 512 four-bit indices is 64 words against 52, and the palette doubles to 4: 84 words,
//     336 B, against 280. That is +20% on every brick in the world, which is most of goal 275's
//     measured -264.2 MB given back to add one material.
//   * A SECOND SIZE CLASS. BrickPool is a fixed-size slot allocator and the whole AK-D resident
//     cache is built on that; two sizes is a different allocator.
//
// The third way, which is what is here: keep 3 bits and 280 B, and make the invariant a PER-BRICK
// one backed by measurement. tools/palette_probe on a real shipping build found 98.6% of bricks
// hold three or fewer distinct materials and NOTHING exceeds five, against eight slots. A brick
// that overflows is therefore not merely rare, it has never been observed -- but "never observed"
// is not "cannot happen", so overflow has a defined, deterministic answer and a counter:
//
//   * the material's palette_fallback (world/materials), if that is already interned -- a grass
//     blade becomes the ground grass it stands in, not Air;
//   * failing that, entry 1, the brick's first material. Wrong shading, never a hole: a voxel that
//     read back as Air with its occupancy bit set would desync geometry from material, which is far
//     worse than a mis-shaded voxel.
//   * and brick_palette_overflows() counts every one, so a build can assert zero. test_brick.cpp
//     asserts it is zero over a real build and non-zero on a deliberately-overflowing brick, which
//     is what makes the counter evidence rather than decoration.
static_assert(kBrickPaletteSize >= 6,
              "the measured worst case is five distinct materials in one brick; a palette smaller "
              "than that would overflow on real content rather than on a constructed test");

// Process-wide count of palette overflows since start. Not per-brick state: an overflow is a
// global-health question ("did this build produce any?"), and a counter in the brick would cost
// bytes on every brick to record something that should always be zero.
[[nodiscard]] std::uint64_t brick_palette_overflows() noexcept;
void brick_reset_palette_overflows() noexcept;

namespace detail {
// The slot to use when the palette is full. Declared here and defined in brick.cpp so the header
// stays free of the registry's lookup tables.
[[nodiscard]] std::uint32_t brick_overflow_slot(const std::uint32_t* words,
                                                world::chunk::MaterialID material) noexcept;
// The same policy for the bulk pack, which knows its palette as a material->slot table rather than
// as packed words.
[[nodiscard]] std::uint32_t brick_overflow_slot_packed(const std::int8_t* slotOf,
                                                       world::chunk::MaterialID material) noexcept;
} // namespace detail

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
    // Intern: an existing entry with this material, else the first never-assigned one, else the
    // overflow policy documented above.
    std::uint32_t slot = 0u;
    bool placed = false;
    for (std::size_t e = 1; e < kBrickPaletteSize; ++e) {
        const world::chunk::MaterialID held = brick_palette_entry(words, e);
        if (held == material) {
            brick_word_index_set(words, index, static_cast<std::uint32_t>(e));
            return;
        }
        if (held == world::chunk::MaterialID::Air) {
            brick_palette_set(words, e, material);
            slot = static_cast<std::uint32_t>(e);
            placed = true;
            break;
        }
    }
    if (!placed) {
        slot = detail::brick_overflow_slot(words, material);
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
                if (next < static_cast<std::int8_t>(kBrickPaletteSize)) {
                    slot = next++;
                    slotOf[m] = slot;
                } else {
                    // Same policy as the per-voxel path, and it must be the same or the bulk and
                    // incremental fills would disagree about a brick nobody has ever seen.
                    slot = static_cast<std::int8_t>(detail::brick_overflow_slot_packed(
                        slotOf.data(), static_cast<world::chunk::MaterialID>(m)));
                }
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

    // Prompt 005 goal 278: the albedo a viewer whose pixel covers this whole brick actually sees,
    // as a sum weighted by EXPOSED FACE COUNT plus its denominator, so a parent can combine
    // children by real surface area. `representative()` above answers a different question --
    // "which single material best stands for this brick" -- and goal 277 measured that a
    // representative is a quantiser rather than a filter: it turns fine speckle into coarse
    // blotches. Weighting by exposure rather than by volume matters and was got wrong once: a
    // volume average includes the stone buried under a grass cap and turns hillsides olive.
    struct AlbedoSum {
        glm::vec3 sum{0.0f};
        float faces = 0.0f;
    };
    [[nodiscard]] AlbedoSum exposed_albedo_sum() const noexcept;

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
