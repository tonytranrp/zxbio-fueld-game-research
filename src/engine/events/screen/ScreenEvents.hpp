#pragma once

#include "engine/core/Types.hpp"
#include "engine/ui/typed/ScreenTypes.hpp"
#include <string>

// ------------------------------------------------------------------------------
// Screen Events - Screen stack transition, layer, and debug-render overrides
// ------------------------------------------------------------------------------
namespace biofuel::engine::events::screen {

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
