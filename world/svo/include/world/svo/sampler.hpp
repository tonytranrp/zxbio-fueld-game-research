#pragma once

#include <concepts>
#include <cstdint>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"
#include "world/svo/brick.hpp"

namespace world::svo {

// Axis-aligned world-space box, half-open [min, max) in meters.
struct Box {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    [[nodiscard]] glm::vec3 extent() const noexcept { return max - min; }
    [[nodiscard]] bool intersects(const Box& o) const noexcept {
        return min.x < o.max.x && o.min.x < max.x && min.y < o.max.y && o.min.y < max.y && min.z < o.max.z &&
               o.min.z < max.z;
    }
};

// What a sampler can prove about a whole box without visiting its voxels. SOUNDNESS is the
// contract: Air/Solid may only be answered when EVERY voxel of the box, at ANY voxel size the
// builder might use, would sample that way; Mixed is always a safe answer (it just costs
// subdivision). The builder never checks -- a wrong Solid/Air is a hole in the world, which is why
// the terrain sampler's classification is tested against dense sampling.
enum class BoxClass : std::uint8_t { Air, Solid, Mixed };

struct BoxClassification {
    BoxClass cls = BoxClass::Mixed;
    world::chunk::MaterialID material = world::chunk::MaterialID::Air; // meaningful for Solid
};

// The one interface the tree builder needs (cpp-heavy-templates rule 1: a constrained template
// parameter, not a bare typename): whole-box classification for empty-space/solid-interior
// skipping, and a brick fill for the boxes that straddle a surface.
template <typename S>
concept VoxelSampler =
    requires(const S& s, const Box& box, const glm::vec3& origin, float voxelEdge, Brick& brick) {
        { s.classify(box) } -> std::same_as<BoxClassification>;
        // Fill the 8x8x8 brick whose min corner is `origin` and whose voxels have edge `voxelEdge`.
        // `brick` arrives all-Air.
        { s.fill_brick(origin, voxelEdge, brick) } -> std::same_as<void>;
    };

// Helper for simple point-sampled samplers (tests, analytic shapes): fills a brick by evaluating
// `materialAt(voxelCenter, voxelEdge)` per voxel.
template <typename F>
void fill_brick_pointwise(F&& materialAt, const glm::vec3& origin, float voxelEdge, Brick& brick) {
    for (int z = 0; z < kBrickEdge; ++z) {
        for (int y = 0; y < kBrickEdge; ++y) {
            for (int x = 0; x < kBrickEdge; ++x) {
                const glm::vec3 center =
                    origin +
                    (glm::vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)} + 0.5f) *
                        voxelEdge;
                const world::chunk::MaterialID m = materialAt(center, voxelEdge);
                if (m != world::chunk::MaterialID::Air) {
                    brick.set(x, y, z, m);
                }
            }
        }
    }
}

} // namespace world::svo
