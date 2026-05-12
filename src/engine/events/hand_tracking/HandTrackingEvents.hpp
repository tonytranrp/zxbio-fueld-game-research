#pragma once

#include "engine/vision/hand_tracking/HandTrackingTypes.hpp"
#include <string>

namespace biofuel::engine::events::hand_tracking {

struct CameraAccessRequestedEvent {};

struct CameraAccessChangedEvent {
    bool granted = false;
};

struct WorkerStartedEvent {};

struct WorkerStoppedEvent {};

struct WorkerErrorEvent {
    std::string message{};
};

struct FrameReceivedEvent {
    ::biofuel::engine::vision::hand_tracking::HandTrackingFrame frame{};
};

struct HandLostEvent {
    ::biofuel::engine::vision::hand_tracking::HandTrackingHandedness handedness =
        ::biofuel::engine::vision::hand_tracking::HandTrackingHandedness::Unknown;
};

struct GestureChangedEvent {
    ::biofuel::engine::vision::hand_tracking::HandTrackingHandedness handedness =
        ::biofuel::engine::vision::hand_tracking::HandTrackingHandedness::Unknown;
    ::biofuel::engine::vision::hand_tracking::HandTrackingGesture gesture =
        ::biofuel::engine::vision::hand_tracking::HandTrackingGesture::Unknown;
    f32 score = 0.0f;
};

} // namespace biofuel::engine::events::hand_tracking
