#include "world/chunk/chunk_voxels.hpp"

namespace world::chunk {

namespace {

// Rounds a palette size up to the next bit width in {0,1,2,4,8} (M1_2_BRIEF.md §1.2's table) --
// always a power of two so a voxel's index never straddles a byte boundary, at the cost of a
// looser bound than a tight ceil(log2(n)) would give (e.g. 5 distinct materials costs 4
// bits/voxel here, not 3) in exchange for trivially simple, branch-light packing.
std::uint8_t bits_for_palette_size(std::size_t paletteSize) {
    if (paletteSize <= 1) return 0;
    if (paletteSize <= 2) return 1;
    if (paletteSize <= 4) return 2;
    if (paletteSize <= 16) return 4;
    return 8; // <= 256
}

std::uint8_t read_index(const std::pmr::vector<std::byte>& indices, std::size_t localIndex, std::uint8_t bits) {
    if (bits == 0) {
        return 0; // homogeneous: every voxel is implicitly palette index 0 (see packed_byte_count)
    }
    const std::size_t voxelsPerByte = 8 / bits;
    const std::size_t byteIndex = localIndex / voxelsPerByte;
    const auto bitOffset = static_cast<std::uint8_t>((localIndex % voxelsPerByte) * bits);
    const auto mask = static_cast<std::uint8_t>((1u << bits) - 1u);
    return static_cast<std::uint8_t>((std::to_integer<std::uint8_t>(indices[byteIndex]) >> bitOffset) & mask);
}

void write_index(std::pmr::vector<std::byte>& indices, std::size_t localIndex, std::uint8_t bits, std::uint8_t value) {
    if (bits == 0) {
        return; // homogeneous: no index buffer exists to write into (see packed_byte_count)
    }
    const std::size_t voxelsPerByte = 8 / bits;
    const std::size_t byteIndex = localIndex / voxelsPerByte;
    const auto bitOffset = static_cast<std::uint8_t>((localIndex % voxelsPerByte) * bits);
    const auto mask = static_cast<std::uint8_t>((1u << bits) - 1u);

    auto byteVal = std::to_integer<std::uint8_t>(indices[byteIndex]);
    byteVal = static_cast<std::uint8_t>(byteVal & static_cast<std::uint8_t>(~(mask << bitOffset)));
    byteVal = static_cast<std::uint8_t>(byteVal | static_cast<std::uint8_t>((value & mask) << bitOffset));
    indices[byteIndex] = std::byte{byteVal};
}

std::size_t packed_byte_count(std::size_t voxelCount, std::uint8_t bits) {
    if (bits == 0) {
        // Homogeneous representation stores no index buffer at all. Unreachable from promote()
        // (its callers only ever widen to >= 1 bit), but the invariant lives two functions away --
        // cheap to state here rather than trust at a distance.
        return 0;
    }
    const std::size_t voxelsPerByte = 8 / bits;
    return (voxelCount + voxelsPerByte - 1) / voxelsPerByte;
}

} // namespace

ChunkVoxels::ChunkVoxels(std::pmr::memory_resource* resource) : palette_(resource), indices_(resource) {
    palette_.push_back(MaterialID::Air);
}

MaterialID ChunkVoxels::at(std::size_t localIndex) const {
    if (palette_.size() == 1) {
        return palette_[0];
    }
    return palette_[read_index(indices_, localIndex, bits_)];
}

std::size_t ChunkVoxels::palette_index_of(MaterialID material) const {
    for (std::size_t i = 0; i < palette_.size(); ++i) {
        if (palette_[i] == material) {
            return i;
        }
    }
    return palette_.size();
}

void ChunkVoxels::promote(std::uint8_t newBits) {
    std::pmr::vector<std::byte> newIndices(indices_.get_allocator());
    newIndices.resize(packed_byte_count(kVoxelsPerChunk, newBits), std::byte{0});

    // bits_ == 0 means every voxel was previously implicitly palette index 0 (no indices_ buffer
    // existed at all) -- newIndices is already zero-initialized, which already encodes index 0
    // for every voxel at the new width, so there is nothing to re-read for that case.
    if (bits_ != 0) {
        for (std::size_t i = 0; i < kVoxelsPerChunk; ++i) {
            write_index(newIndices, i, newBits, read_index(indices_, i, bits_));
        }
    }

    indices_ = std::move(newIndices);
    bits_ = newBits;
}

void ChunkVoxels::set(std::size_t localIndex, MaterialID material) {
    std::size_t paletteIdx = palette_index_of(material);
    if (paletteIdx == palette_.size()) {
        palette_.push_back(material);
        const std::uint8_t newBits = bits_for_palette_size(palette_.size());
        if (newBits != bits_) {
            promote(newBits); // re-packs every EXISTING voxel's index at the new width -- §1.3
        }
    }

    if (bits_ == 0) {
        // Still homogeneous (palette_.size() == 1): the material being set must be palette_[0]
        // itself (paletteIdx == 0), so there is nothing to write into indices_.
        return;
    }
    write_index(indices_, localIndex, bits_, static_cast<std::uint8_t>(paletteIdx));
}

void ChunkVoxels::fill_uniform(MaterialID material) {
    palette_.clear();
    palette_.push_back(material);
    indices_.clear();
    bits_ = 0;
}

} // namespace world::chunk
