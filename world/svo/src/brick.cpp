#include "world/svo/brick.hpp"

#include <bit>

#include "world/materials/materials.hpp"

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
        return (words_[static_cast<std::size_t>(y >> 2) + static_cast<std::size_t>(z) * 2] >> (8 * (y & 3))) &
               0xFFu;
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



Brick::AlbedoSum Brick::exposed_albedo_sum() const noexcept {
    // Goal 278. Weighted by EXPOSED FACES, not by volume, and getting that wrong was a real bug
    // with a viewed capture: averaging every occupied voxel includes the dirt and stone BURIED
    // under a grass cap, which no viewer ever sees, and it turned every green hillside olive.
    // A voxel's contribution is the number of its own faces that meet air -- the same quantity
    // `exposed_face_sum` accumulates as a vector, counted here as a scalar.
    //
    // Faces on the brick's OUTER boundary are not counted, for the same reason exposed_face_sum
    // does not count them: the neighbour is unknown here, and the tree builder adds coarse
    // exposure at the parent instead.
    //
    // Materials are components (Group AC): the colour is asked of world::materials, never derived
    // from an ID by arithmetic or by a table in this file.
    const auto row = [&](int y, int z) -> std::uint32_t {
        return (words_[static_cast<std::size_t>(y >> 2) + static_cast<std::size_t>(z) * 2] >> (8 * (y & 3))) &
               0xFFu;
    };
    AlbedoSum out;
    for (int z = 0; z < kBrickEdge; ++z) {
        for (int y = 0; y < kBrickEdge; ++y) {
            const std::uint32_t r = row(y, z);
            if (r == 0u) {
                continue;
            }
            const std::uint32_t up = y + 1 < kBrickEdge ? row(y + 1, z) : 0xFFu;
            const std::uint32_t down = y > 0 ? row(y - 1, z) : 0xFFu;
            const std::uint32_t fwd = z + 1 < kBrickEdge ? row(y, z + 1) : 0xFFu;
            const std::uint32_t back = z > 0 ? row(y, z - 1) : 0xFFu;
            for (int x = 0; x < kBrickEdge; ++x) {
                const std::uint32_t bit = 1u << x;
                if ((r & bit) == 0u) {
                    continue;
                }
                int faces = 0;
                faces += (x + 1 < kBrickEdge && (r & (bit << 1)) == 0u) ? 1 : 0;
                faces += (x > 0 && (r & (bit >> 1)) == 0u) ? 1 : 0;
                faces += (up & bit) == 0u ? 1 : 0;
                faces += (down & bit) == 0u ? 1 : 0;
                faces += (fwd & bit) == 0u ? 1 : 0;
                faces += (back & bit) == 0u ? 1 : 0;
                if (faces == 0) {
                    continue; // fully buried: contributes no visible surface
                }
                const world::materials::MaterialDef& props =
                    world::materials::properties_of(at(brick_voxel_index(x, y, z)));
                const auto w = static_cast<float>(faces);
                out.sum += glm::vec3{props.albedo.r, props.albedo.g, props.albedo.b} * w;
                out.faces += w;
            }
        }
    }
    return out;
}

} // namespace world::svo
