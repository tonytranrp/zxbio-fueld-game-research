#pragma once

#include "engine/core/Types.hpp"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace biofuel::engine::vision::hand_tracking {

enum class HandTrackingConnectionState : u8 {
    Disabled,
    Idle,
    WaitingForConsent,
    Starting,
    Online,
    Offline,
    Error,
};

enum class HandTrackingHandedness : u8 {
    Unknown,
    Left,
    Right,
};

enum class HandTrackingGesture : u8 {
    Unknown,
    None,
    ClosedFist,
    OpenPalm,
    PointingUp,
    ThumbDown,
    ThumbUp,
    Victory,
    ILoveYou,
};

struct HandTrackingLandmark {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
};

struct HandTrackingHand {
    static constexpr usize LANDMARK_COUNT = 21U;

    bool valid = false;
    HandTrackingHandedness handedness = HandTrackingHandedness::Unknown;
    f32 handednessScore = 0.0f;
    HandTrackingGesture gesture = HandTrackingGesture::Unknown;
    f32 gestureScore = 0.0f;
    std::array<HandTrackingLandmark, LANDMARK_COUNT> imageLandmarks{};
    std::array<HandTrackingLandmark, LANDMARK_COUNT> worldLandmarks{};
};

struct HandTrackingFrame {
    static constexpr usize MAX_HANDS = 2U;

    bool valid = false;
    u64 sequence = 0U;
    u64 timestampMs = 0U;
    u16 cameraWidth = 0U;
    u16 cameraHeight = 0U;
    f32 latencyMs = 0.0f;
    u8 handCount = 0U;
    std::array<HandTrackingHand, MAX_HANDS> hands{};
};

struct HandTrackingPreviewFrame {
    u64 sequence = 0U;
    std::vector<u8> jpegBytes{};
};

struct HandTrackingStatus {
    HandTrackingConnectionState state = HandTrackingConnectionState::Disabled;
    bool featureEnabled = false;
    bool cameraConsentRequested = false;
    bool cameraConsentGranted = false;
    bool workerRunning = false;
    bool previewEnabled = false;
    f32 packetsPerSecond = 0.0f;
    f32 secondsSinceLastFrame = 0.0f;
    std::string message{};
};

[[nodiscard]] constexpr std::string_view toString(const HandTrackingConnectionState state) noexcept {
    switch (state) {
    case HandTrackingConnectionState::Disabled: return "Disabled";
    case HandTrackingConnectionState::Idle: return "Idle";
    case HandTrackingConnectionState::WaitingForConsent: return "WaitingForConsent";
    case HandTrackingConnectionState::Starting: return "Starting";
    case HandTrackingConnectionState::Online: return "Online";
    case HandTrackingConnectionState::Offline: return "Offline";
    case HandTrackingConnectionState::Error: return "Error";
    }
    return "Unknown";
}

[[nodiscard]] constexpr std::string_view toString(const HandTrackingHandedness handedness) noexcept {
    switch (handedness) {
    case HandTrackingHandedness::Unknown: return "Unknown";
    case HandTrackingHandedness::Left: return "Left";
    case HandTrackingHandedness::Right: return "Right";
    }
    return "Unknown";
}

[[nodiscard]] constexpr std::string_view toString(const HandTrackingGesture gesture) noexcept {
    switch (gesture) {
    case HandTrackingGesture::Unknown: return "Unknown";
    case HandTrackingGesture::None: return "None";
    case HandTrackingGesture::ClosedFist: return "Closed_Fist";
    case HandTrackingGesture::OpenPalm: return "Open_Palm";
    case HandTrackingGesture::PointingUp: return "Pointing_Up";
    case HandTrackingGesture::ThumbDown: return "Thumb_Down";
    case HandTrackingGesture::ThumbUp: return "Thumb_Up";
    case HandTrackingGesture::Victory: return "Victory";
    case HandTrackingGesture::ILoveYou: return "ILoveYou";
    }
    return "Unknown";
}

[[nodiscard]] constexpr usize handIndex(const HandTrackingHandedness handedness) noexcept {
    return handedness == HandTrackingHandedness::Right ? 1U : 0U;
}

} // namespace biofuel::engine::vision::hand_tracking
