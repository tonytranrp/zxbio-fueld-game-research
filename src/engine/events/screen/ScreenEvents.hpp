#pragma once

#include "engine/core/Types.hpp"
#include "engine/ui/typed/ScreenTypes.hpp"
#include <string>

// ------------------------------------------------------------------------------
// Screen Events - Resolution changes, fullscreen toggles
// ------------------------------------------------------------------------------
namespace biofuel::engine::events::screen {

struct ScreenResizedEvent {
    i32 width;
    i32 height;
    i32 prevWidth;
    i32 prevHeight;
};

struct FullscreenToggledEvent {
    bool fullscreen;
};

struct ScreenTransitionOverrideEvent {
    ::biofuel::engine::ui::typed::ScreenId screenId = ::biofuel::engine::ui::typed::ScreenId::Unknown;
    bool enabled = true;
    bool persistent = false;
    ::biofuel::engine::ui::typed::TransitionPolicyData policy{};
};

struct ScreenLayerOverrideEvent {
    ::biofuel::engine::ui::typed::ScreenId screenId = ::biofuel::engine::ui::typed::ScreenId::Unknown;
    std::string layerName;
    bool enabled = true;
    bool persistent = false;
};

struct ScreenDebugRenderOverrideEvent {
    ::biofuel::engine::ui::typed::ScreenId screenId = ::biofuel::engine::ui::typed::ScreenId::Unknown;
    bool enabled = true;
    bool persistent = false;
};

} // namespace biofuel::engine::events::screen
