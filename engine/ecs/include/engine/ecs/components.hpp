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

} // namespace engine::ecs
