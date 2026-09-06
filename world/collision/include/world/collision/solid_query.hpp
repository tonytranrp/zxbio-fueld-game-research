#pragma once

#include <concepts>

#include "engine/core/math.hpp"

namespace world::collision {

// Axis-aligned box in world meters, half-open like world::svo::Box.
struct Aabb {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    [[nodiscard]] glm::vec3 extent() const noexcept { return max - min; }
    [[nodiscard]] Aabb translated(const glm::vec3& d) const noexcept { return Aabb{min + d, max + d}; }
    [[nodiscard]] bool intersects(const Aabb& o) const noexcept {
        return min.x < o.max.x && o.min.x < max.x && min.y < o.max.y && o.min.y < max.y && min.z < o.max.z &&
               o.min.z < max.z;
    }

    // An upright body standing with its feet center at `feet`: half_width to each side, `height` up.
    [[nodiscard]] static Aabb upright(const glm::vec3& feet, float halfWidth, float height) noexcept {
        return Aabb{feet - glm::vec3{halfWidth, 0.0f, halfWidth},
                    feet + glm::vec3{halfWidth, height, halfWidth}};
    }
};

// What the sweep needs from a world (docs/goals.md Group AA): one yes/no per box. Anything with
// this shape plugs in -- the analytic terrain collider, a test's plane-and-wall fake, or later a
// tree query over the sparse-brick octree once editing (goal 160) makes the analytic world stale.
template <typename Q>
concept SolidQuery = requires(const Q& q, const Aabb& box) {
    { q.overlaps_solid(box) } -> std::convertible_to<bool>;
};

} // namespace world::collision
