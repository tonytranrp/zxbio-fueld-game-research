#pragma once

#include "engine/core/Types.hpp"
#include "engine/animation/Easing.hpp"
#include <string_view>

namespace biofuel::engine::ui::typed {

enum class ScreenId : u8 {
    Unknown,
    Slot0,
    Slot1,
    Slot2,
    Slot3,
    Slot4,
    Slot5,
    Slot6,
    Slot7,
    Count,
};

static constexpr u8 SCREEN_ID_COUNT = static_cast<u8>(ScreenId::Count);

[[nodiscard]] constexpr u8 screenIdIndex(const ScreenId id) noexcept {
    return static_cast<u8>(id);
}

enum class TransitionComposer : u8 {
    None,
    Crossfade,
};

struct TransitionPolicyData {
    f32 duration = 0.5f;
    ::biofuel::engine::animation::Easing::Fn easing = ::biofuel::engine::animation::Easing::easeOutCubic;
    TransitionComposer composer = TransitionComposer::Crossfade;
};

struct StackPolicyData {
    bool renderBelow = false;
    bool updateBelow = false;
    bool inputBelow = false;
};

} // namespace biofuel::engine::ui::typed
