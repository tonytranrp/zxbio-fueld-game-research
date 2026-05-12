#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"
#include "engine/vision/hand_tracking/HandTrackingService.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(HandTrackingService);
BIOFUEL_STATIC_SERVICE(
    HandTrackingService,
    "service.hand_tracking",
    ::biofuel::engine::vision::hand_tracking::HandTrackingService);
BIOFUEL_SERVICE_MODULE(HandTrackingServiceModule, HandTrackingService)
} // namespace biofuel::engine::runtime::typed
