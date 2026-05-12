#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>

namespace biofuel::game::screens::dev_hand_lab {

struct HandLabCameraState {
    Vector3 target{0.0f, 0.02f, 0.0f};
    f32 yaw = 0.0f;
    f32 pitch = 0.12f;
    f32 distance = 2.25f;
};

struct HandLabWristPose {
    Vector3 leftOrigin{-0.42f, -0.18f, 0.0f};
    Vector3 rightOrigin{0.42f, -0.18f, 0.0f};
    f32 pitch = 0.0f;
    f32 yaw = 0.0f;
    f32 roll = 0.0f;
};

} // namespace biofuel::game::screens::dev_hand_lab
