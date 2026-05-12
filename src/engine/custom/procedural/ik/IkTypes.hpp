#pragma once

#include "engine/core/Types.hpp"

namespace biofuel::engine::custom::procedural::ik {

struct IkSolveSettings {
    i32 maxIterations = 16;
    f32 tolerance = 0.002f;
};

struct IkSolveResult {
    i32 iterations = 0;
    f32 error = 0.0f;
    bool reached = false;
};

} // namespace biofuel::engine::custom::procedural::ik
