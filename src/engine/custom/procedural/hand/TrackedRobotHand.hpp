#pragma once

#include "engine/custom/procedural/hand/HandTypes.hpp"
#include <array>
#include <raylib.h>

namespace biofuel::engine::custom::procedural::hand {

struct TrackedRobotHandPose {
    static constexpr usize LANDMARK_COUNT = 21U;

    bool valid = false;
    HandSide side = HandSide::Left;
    f32 confidence = 0.0f;
    std::array<Vector3, LANDMARK_COUNT> landmarks{};
};

} // namespace biofuel::engine::custom::procedural::hand
