#pragma once

#include <array>

#include "engine/core/math.hpp"

namespace render::diligent {

// View frustum as 6 clip-space half-space planes extracted from a view-projection matrix
// (Gribb–Hartmann). Each plane is (n.xyz, d); a point p is on the inside of a plane iff
// dot(n, p) + d >= 0. Planes are deliberately NOT normalized -- only the sign matters for the
// in/out test below, and callers must not treat plane distances as metric.
struct Frustum {
    std::array<glm::vec4, 6> planes;
};

struct Aabb {
    glm::vec3 min;
    glm::vec3 max;
};

// `viewProj` must be the exact matrix the vertex shader applies (projection * view, GLM
// column-vector convention, [0,1] NDC depth per engine/core/math.hpp) -- extracting from any
// other matrix silently culls against the wrong volume.
[[nodiscard]] Frustum extract_frustum(const glm::mat4& viewProj) noexcept;

// Conservative AABB-vs-frustum test (positive-vertex): never culls a visible box; may keep a box
// that is actually outside near a frustum corner (the standard, acceptable false-positive).
[[nodiscard]] bool intersects(const Frustum& frustum, const Aabb& box) noexcept;

} // namespace render::diligent
