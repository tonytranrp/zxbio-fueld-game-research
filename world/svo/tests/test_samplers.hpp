#pragma once

// Analytic samplers for the world/svo tests: exact, cheap, and with a trivially correct
// pointwise ground truth the tree can be compared against voxel by voxel.

#include <cstdint>

#include "engine/core/math.hpp"
#include "world/svo/sampler.hpp"

namespace svo_tests {

using world::chunk::MaterialID;
using world::svo::Box;
using world::svo::BoxClass;
using world::svo::BoxClassification;
using world::svo::Brick;

// Solid ball: a voxel is `material` iff its CENTER lies strictly inside the sphere.
struct SphereSampler {
    glm::vec3 center{0.0f};
    float radius = 1.0f;
    MaterialID material = MaterialID::Stone;

    [[nodiscard]] MaterialID material_at_center(const glm::vec3& c) const noexcept {
        return glm::length(c - center) < radius ? material : MaterialID::Air;
    }

    [[nodiscard]] BoxClassification classify(const Box& box) const noexcept {
        const glm::vec3 nearest = glm::clamp(center, box.min, box.max);
        const float dMin = glm::length(nearest - center);
        float dMax = 0.0f;
        for (int corner = 0; corner < 8; ++corner) {
            const glm::vec3 p{(corner & 1) != 0 ? box.max.x : box.min.x,
                              (corner & 2) != 0 ? box.max.y : box.min.y,
                              (corner & 4) != 0 ? box.max.z : box.min.z};
            dMax = glm::max(dMax, glm::length(p - center));
        }
        if (dMax < radius) {
            return {BoxClass::Solid, material};
        }
        if (dMin >= radius) {
            return {BoxClass::Air, MaterialID::Air};
        }
        return {BoxClass::Mixed, MaterialID::Air};
    }

    void fill_brick(const glm::vec3& origin, float voxelEdge, Brick& brick) const {
        world::svo::fill_brick_pointwise([&](const glm::vec3& c, float) { return material_at_center(c); },
                                         origin, voxelEdge, brick);
    }
};

// Hash-noise occupancy at a fixed voxel lattice (`lattice` meters): thin, scattered structure the
// hierarchy cannot skip, which is what stresses traversal the hardest. Always classifies Mixed
// (the sampler has no cheap bound), so the tree is dense.
struct RandomSampler {
    float lattice = 1.0f;
    std::uint32_t density256 = 40; // occupied probability = density256 / 256
    std::uint32_t seed = 7;

    [[nodiscard]] static std::uint32_t hash(std::uint32_t x) noexcept {
        x ^= x >> 16;
        x *= 0x7feb352dU;
        x ^= x >> 15;
        x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    }

    [[nodiscard]] MaterialID material_at_center(const glm::vec3& c) const noexcept {
        const auto ix = static_cast<std::int32_t>(glm::floor(c.x / lattice));
        const auto iy = static_cast<std::int32_t>(glm::floor(c.y / lattice));
        const auto iz = static_cast<std::int32_t>(glm::floor(c.z / lattice));
        const std::uint32_t h =
            hash(hash(hash(static_cast<std::uint32_t>(ix) + seed) ^ static_cast<std::uint32_t>(iy)) ^
                 static_cast<std::uint32_t>(iz));
        if ((h & 0xFFu) >= density256) {
            return MaterialID::Air;
        }
        return static_cast<MaterialID>(1u + ((h >> 8) % 7u)); // 1..7, never Air
    }

    [[nodiscard]] BoxClassification classify(const Box&) const noexcept {
        return {BoxClass::Mixed, MaterialID::Air};
    }

    void fill_brick(const glm::vec3& origin, float voxelEdge, Brick& brick) const {
        world::svo::fill_brick_pointwise([&](const glm::vec3& c, float) { return material_at_center(c); },
                                         origin, voxelEdge, brick);
    }
};

static_assert(world::svo::VoxelSampler<SphereSampler>);
static_assert(world::svo::VoxelSampler<RandomSampler>);

} // namespace svo_tests
