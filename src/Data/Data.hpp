#pragma once

// ------------------------------------------------------------------------------
// Data Hook - Bridge between event system and the rest of the engine
// Include this file anywhere you need events.
// ------------------------------------------------------------------------------

#include "event/EventManager.hpp"
#include "event/animation/AnimationEvents.hpp"
#include "event/input/InputEvents.hpp"
#include "event/model/ModelEvents.hpp"
#include "event/mouse/MouseEvents.hpp"
#include "event/screen/ScreenEvents.hpp"
#include "event/window/WindowEvents.hpp"
#include "Systems/Model/ModelSystem.hpp"
#include "UI/ScreenManager.hpp"
#include "Utils/font/FontUtils.hpp"

#include "event/video/VideoEvents.hpp"

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

    [[nodiscard]] static utils::font::FontManager& fonts() {
        return utils::font::FontManager::instance();
    }

    [[nodiscard]] static systems::model::ModelSystem& models() {
        return systems::model::ModelSystem::instance();
    }
};

} // namespace biofuel
