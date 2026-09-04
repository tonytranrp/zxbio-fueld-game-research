#pragma once

#include <string>

#include "engine/core/math.hpp"

namespace engine::ecs {

struct Transform {
    glm::vec3 position{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f}; // (w, x, y, z) — identity
};

struct Name {
    std::string value;
};

// Lens half of a camera entity (Phase 1 brief §6): pose lives in the entity's Transform, so
// the camera is an ordinary entity, not a bespoke class outside the ECS. Mirrors
// render::interface::Camera's lens fields -- app code copies Transform + CameraLens into that
// boundary struct each frame rather than render/ ever reading the registry.
struct CameraLens {
    float fov_y_radians = 1.2217305f; // 70 degrees
    float near_plane = 0.1f;
    float far_plane = 2000.0f;
};

} // namespace engine::ecs
