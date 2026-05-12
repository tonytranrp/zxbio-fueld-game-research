#pragma once

#include "engine/events/hand_tracking/HandTrackingEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::hand_tracking {
BIOFUEL_EVENT_TAG(CameraAccessRequested, ::biofuel::engine::events::hand_tracking::CameraAccessRequestedEvent);
BIOFUEL_EVENT_TAG(CameraAccessChanged, ::biofuel::engine::events::hand_tracking::CameraAccessChangedEvent);
BIOFUEL_EVENT_TAG(WorkerStarted, ::biofuel::engine::events::hand_tracking::WorkerStartedEvent);
BIOFUEL_EVENT_TAG(WorkerStopped, ::biofuel::engine::events::hand_tracking::WorkerStoppedEvent);
BIOFUEL_EVENT_TAG(WorkerError, ::biofuel::engine::events::hand_tracking::WorkerErrorEvent);
BIOFUEL_EVENT_TAG(FrameReceived, ::biofuel::engine::events::hand_tracking::FrameReceivedEvent);
BIOFUEL_EVENT_TAG(HandLost, ::biofuel::engine::events::hand_tracking::HandLostEvent);
BIOFUEL_EVENT_TAG(GestureChanged, ::biofuel::engine::events::hand_tracking::GestureChangedEvent);
} // namespace biofuel::engine::runtime::typed::hand_tracking

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(hand_tracking::CameraAccessRequested, "hand_tracking.camera_access_requested");
BIOFUEL_EVENT_SPEC(hand_tracking::CameraAccessChanged, "hand_tracking.camera_access_changed");
BIOFUEL_EVENT_SPEC(hand_tracking::WorkerStarted, "hand_tracking.worker_started");
BIOFUEL_EVENT_SPEC(hand_tracking::WorkerStopped, "hand_tracking.worker_stopped");
BIOFUEL_EVENT_SPEC(hand_tracking::WorkerError, "hand_tracking.worker_error");
BIOFUEL_EVENT_SPEC(hand_tracking::FrameReceived, "hand_tracking.frame_received");
BIOFUEL_EVENT_SPEC(hand_tracking::HandLost, "hand_tracking.hand_lost");
BIOFUEL_EVENT_SPEC(hand_tracking::GestureChanged, "hand_tracking.gesture_changed");
BIOFUEL_EVENT_MODULE(HandTrackingEventModule, InputEvents,
    hand_tracking::CameraAccessRequested,
    hand_tracking::CameraAccessChanged,
    hand_tracking::WorkerStarted,
    hand_tracking::WorkerStopped,
    hand_tracking::WorkerError,
    hand_tracking::FrameReceived,
    hand_tracking::HandLost,
    hand_tracking::GestureChanged)
} // namespace biofuel::engine::runtime::typed
