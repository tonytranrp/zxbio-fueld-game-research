#pragma once

#include "engine/custom/procedural/hand/HandPoseService.hpp"
#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(HandPoseService);
BIOFUEL_STATIC_SERVICE(
    HandPoseService,
    "service.hand_pose",
    ::biofuel::engine::custom::procedural::hand::HandPoseService);
BIOFUEL_SERVICE_MODULE(HandPoseServiceModule, HandPoseService)
} // namespace biofuel::engine::runtime::typed
