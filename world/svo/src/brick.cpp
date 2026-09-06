#include "world/svo/brick.hpp"

#include <bit>

namespace world::svo {

world::chunk::MaterialID Brick::representative() const noexcept {
    std::array<std::uint16_t, 256> counts{};
    for (int z = 0; z < kBrickEdge; ++z) {
        for (int x = 0; x < kBrickEdge; ++x) {
            for (int y = kBrickEdge - 1; y >= 0; --y) {
                if (occupied(x, y, z)) {
                    ++counts[static_cast<std::size_t>(at(x, y, z))];
                    break;
                }
            }
        }
    }
    std::size_t best = 0;
    for (std::size_t m = 1; m < counts.size(); ++m) {
        if (counts[m] > counts[best]) {
            best = m;
        }
    }
    return counts[best] == 0 ? world::chunk::MaterialID::Air : static_cast<world::chunk::MaterialID>(best);
}

glm::ivec3 Brick::exposed_face_sum() const noexcept {
    // Occupancy row (y, z) = the 8 x-bits of that row: voxel index x + 8y + 64z lives in mask
    // word (y >> 2) + 2z at bit x + 8 * (y & 3).
    const auto row = [&](int y, int z) -> std::uint32_t {
        return (words_[static_cast<std::size_t>((y >> 2) + 2 * z)] >> (8 * (y & 3))) & 0xFFu;
    };
    glm::ivec3 sum{0, 0, 0};
    for (int z = 0; z < kBrickEdge; ++z) {
        for (int y = 0; y < kBrickEdge; ++y) {
            const std::uint32_t r = row(y, z);
            if (r == 0u) {
                continue;
            }
            // +x face exposed: occupied at x, empty at x+1 (x = 0..6); -x: empty at x-1 (x = 1..7).
            sum.x += std::popcount(r & ~(r >> 1) & 0x7Fu);
            sum.x -= std::popcount(r & ~(r << 1) & 0xFEu);
            if (y + 1 < kBrickEdge) {
                sum.y += std::popcount(r & ~row(y + 1, z));
            }
            if (y > 0) {
                sum.y -= std::popcount(r & ~row(y - 1, z));
            }
            if (z + 1 < kBrickEdge) {
                sum.z += std::popcount(r & ~row(y, z + 1));
            }
            if (z > 0) {
                sum.z -= std::popcount(r & ~row(y, z - 1));
            }
        }
    }
    return sum;
}

} // namespace world::svo
