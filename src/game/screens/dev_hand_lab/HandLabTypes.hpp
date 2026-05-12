#pragma once

#include "engine/core/Types.hpp"
#include "engine/custom/procedural/hand/HandTypes.hpp"
#include <array>
#include <string_view>
#include <raylib.h>

namespace biofuel::game::screens::dev_hand_lab {

using ::biofuel::engine::custom::procedural::hand::FingerId;
using ::biofuel::engine::custom::procedural::hand::HandSide;

enum class HandLabTab : u8 {
    Pose,
    Animation,
    IK,
    Materials,
    Debug,
    Count,
};

enum class TargetVisibility : u8 {
    Selected,
    All,
    Hidden,
};

enum class SliderId : u8 {
    None,
    Curl,
    Spread,
    Iterations,
    Tolerance,
    WristX,
    WristY,
    WristZ,
    WristPitch,
    WristYaw,
    WristRoll,
    ShellIntensity,
    AccentIntensity,
    JointIntensity,
    AnimationSpeed,
    AnimationScrub,
};

struct HandLabSelection {
    HandSide hand = HandSide::Left;
    FingerId finger = FingerId::Index;
};

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

struct HandLabMaterialState {
    f32 shellIntensity = 1.0f;
    f32 accentIntensity = 1.0f;
    f32 jointIntensity = 1.0f;
};

struct HandLabDebugState {
    TargetVisibility targets = TargetVisibility::Selected;
    bool showBones = false;
    bool showAxes = false;
    bool showJointNames = false;
    bool jointLimits = true;
};

template<HandLabTab TTab>
struct HandLabToolTab {
    static constexpr HandLabTab value = TTab;
};

using PoseTab = HandLabToolTab<HandLabTab::Pose>;
using AnimationTab = HandLabToolTab<HandLabTab::Animation>;
using IkTab = HandLabToolTab<HandLabTab::IK>;
using MaterialsTab = HandLabToolTab<HandLabTab::Materials>;
using DebugTab = HandLabToolTab<HandLabTab::Debug>;

template<typename... TTabs>
struct HandLabToolTabs {
    static constexpr std::array<HandLabTab, sizeof...(TTabs)> values{TTabs::value...};
};

using DefaultHandLabTabs = HandLabToolTabs<PoseTab, AnimationTab, IkTab, MaterialsTab, DebugTab>;

[[nodiscard]] constexpr std::string_view tabName(const HandLabTab tab) noexcept {
    switch (tab) {
    case HandLabTab::Pose: return "POSE";
    case HandLabTab::Animation: return "ANIM";
    case HandLabTab::IK: return "IK";
    case HandLabTab::Materials: return "MATS";
    case HandLabTab::Debug: return "DBG";
    case HandLabTab::Count: break;
    }
    return "?";
}

[[nodiscard]] constexpr std::string_view targetVisibilityName(const TargetVisibility visibility) noexcept {
    switch (visibility) {
    case TargetVisibility::Selected: return "Selected";
    case TargetVisibility::All: return "All";
    case TargetVisibility::Hidden: return "Hidden";
    }
    return "?";
}

} // namespace biofuel::game::screens::dev_hand_lab
