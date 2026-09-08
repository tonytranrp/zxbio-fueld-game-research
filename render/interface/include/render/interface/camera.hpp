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
    // Prompt 007 goal 330. 4096 m, raised from 2000 m because goal 329's region reaches **2048 m in
    // each direction and 3547 m to a corner**, so a 2000 m far plane was clipping geometry the
    // marcher had already found. (The marcher itself has no far clip -- `maxT = 1.0e30` -- but the
    // depth it writes comes from this projection, so the far plane still governs SV_Depth.)
    //
    // DEPTH PRECISION, measured as one float32 ULP of the [0,1] depth converted back to metres:
    //
    //          far = 2000 m        far = 4096 m
    //   100 m    0.51 cm             0.48 cm
    //   500 m   19.2  cm            13.6  cm
    //  1000 m   65.4  cm            40.4  cm
    //  2000 m  238.1  cm           133.2  cm
    //
    // Raising the far plane IMPROVES precision at every distance that matters here, which is the
    // opposite of the usual intuition and happens because the near plane, not the far, dominates a
    // standard [0,1] projection's distribution.
    //
    // REVERSED-Z ASSESSED AND DEFERRED, with the reason. It is the standard answer and would give
    // roughly uniform relative precision instead of the table above. It is not taken here because
    // the benefit is currently unclaimable: **nothing z-fights**, since the marcher is the only
    // writer of terrain depth and there are no coplanar surfaces to resolve. Switching touches the
    // projection, the depth-comparison state in every PSO, the TAA reprojection and the post chain
    // -- real risk against no present symptom. Goal 339's instanced grass overlay is the first thing
    // that will genuinely share a depth buffer with the marcher, and it is near-field where the
    // table above reads sub-centimetre. Re-open this when something distant needs to be composed.
    float far_plane = 4096.0f;
};

[[nodiscard]] inline glm::mat4 view_matrix(const Camera& camera) noexcept {
    // view = inverse(world-from-view) = conjugate(rotation) * translate(-position).
    return glm::mat4_cast(glm::conjugate(camera.orientation)) *
           glm::translate(glm::mat4(1.0f), -camera.position);
}

// Produces a [0,1]-NDC-depth matrix (Diligent's normalized convention across every backend) --
// engine/core/math.hpp's GLM_FORCE_DEPTH_ZERO_TO_ONE is what makes glm::perspective do this;
// including GLM any other way is the silent depth-range bug Phase 1 brief §2.3 exists to
// prevent.
[[nodiscard]] inline glm::mat4 projection_matrix(const Camera& camera, float aspect) noexcept {
    return glm::perspective(camera.fov_y_radians, aspect, camera.near_plane, camera.far_plane);
}

} // namespace render::interface
