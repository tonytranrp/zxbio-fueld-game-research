#pragma once

// ------------------------------------------------------------------------------
// Data Hook - Bridge between event system and the rest of the engine
// Include this file anywhere you need events.
// ------------------------------------------------------------------------------

#include "event/EventManager.hpp"
#include "event/animation/AnimationEvents.hpp"
#include "event/input/InputEvents.hpp"
#include "event/mouse/MouseEvents.hpp"
#include "event/screen/ScreenEvents.hpp"
#include "event/window/WindowEvents.hpp"
#include "UI/ScreenManager.hpp"

namespace biofuel {

// ------------------------------------------------------------------------------
// Convenience accessor for the global event bus
// ------------------------------------------------------------------------------
class Data {
public:
    [[nodiscard]] static entt::dispatcher& eventBus() {
        return EventManager::instance().dispatcher();
    }

    [[nodiscard]] static EventManager& events() {
        return EventManager::instance();
    }

    [[nodiscard]] static ui::ScreenManager& screens() {
        return ui::ScreenManager::instance();
    }
};

} // namespace biofuel
