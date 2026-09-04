#pragma once

#include "engine/core/math.hpp"

namespace render::interface {

// Free-flying spectator camera pose + lens (Phase 1 brief §6). Orientation is the
// world-from-view rotation as a quaternion (no Euler angles -- gimbal lock at ±90° pitch is a
// real failure mode for a spectator camera): view-space forward is -Z, up is +Y, GLM's
// right-handed convention. Plain data + pure-GLM helpers only -- no DiligentCore types cross this
// boundary (project brief §3), which is also what makes the frustum-culling math testable
// headless.
struct Camera {
    glm::vec3 position{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f}; // identity: looking down world -Z
    float fov_y_radians = glm::radians(70.0f);
    float near_plane = 0.1f;
    float far_plane = 2000.0f;
};

[[nodiscard]] inline glm::mat4 view_matrix(const Camera& camera) noexcept {
    // view = inverse(world-from-view) = conjugate(rotation) * translate(-position).
    return glm::mat4_cast(glm::conjugate(camera.orientation)) * glm::translate(glm::mat4(1.0f), -camera.position);
}

// Produces a [0,1]-NDC-depth matrix (Diligent's normalized convention across every backend) --
// engine/core/math.hpp's GLM_FORCE_DEPTH_ZERO_TO_ONE is what makes glm::perspective do this;
// including GLM any other way is the silent depth-range bug Phase 1 brief §2.3 exists to
// prevent.
[[nodiscard]] inline glm::mat4 projection_matrix(const Camera& camera, float aspect) noexcept {
    return glm::perspective(camera.fov_y_radians, aspect, camera.near_plane, camera.far_plane);
}

} // namespace render::interface
