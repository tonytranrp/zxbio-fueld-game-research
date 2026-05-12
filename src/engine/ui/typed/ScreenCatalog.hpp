#pragma once

#include "engine/ui/typed/ScreenRegistry.hpp"
#include "engine/ui/typed/ScreenValidation.hpp"
#include "game/screens/loading/LoadingScreenModule.hpp"
#include "game/screens/main_menu/MainMenuScreenModule.hpp"
#include "game/screens/pause_popup/PausePopupScreenModule.hpp"
#include "game/screens/idle/IdleScreenModule.hpp"
#include "game/screens/video/VideoScreenModule.hpp"
#ifdef BIOFUEL_ENABLE_DEV_SCREENS
#include "game/screens/dev_hand_lab/DevHandLabScreenModule.hpp"
#endif

namespace biofuel::engine::ui::typed {

using AppScreenRegistry = ScreenRegistry<
    ::biofuel::game::screens::LoadingScreen,
    ::biofuel::game::screens::MainMenuScreen,
    ::biofuel::game::screens::PausePopupScreen,
    ::biofuel::game::screens::IdleScreen,
    ::biofuel::game::screens::VideoScreen
#ifdef BIOFUEL_ENABLE_DEV_SCREENS
    , ::biofuel::game::screens::DevHandLabScreen
#endif
    >;

static_assert(validateScreenRegistry<AppScreenRegistry>());

[[nodiscard]] constexpr TransitionPolicyData transitionPolicyForId(const ScreenId id) noexcept {
    switch (id) {
    case ScreenId::Loading: return TransitionPolicy<::biofuel::game::screens::LoadingScreen>::VALUE;
    case ScreenId::MainMenu: return TransitionPolicy<::biofuel::game::screens::MainMenuScreen>::VALUE;
    case ScreenId::PausePopup: return TransitionPolicy<::biofuel::game::screens::PausePopupScreen>::VALUE;
    case ScreenId::Idle: return TransitionPolicy<::biofuel::game::screens::IdleScreen>::VALUE;
    case ScreenId::Video: return TransitionPolicy<::biofuel::game::screens::VideoScreen>::VALUE;
#ifdef BIOFUEL_ENABLE_DEV_SCREENS
    case ScreenId::DevHandLab: return TransitionPolicy<::biofuel::game::screens::DevHandLabScreen>::VALUE;
#endif
    case ScreenId::Unknown:
    case ScreenId::Count:
        break;
    }
    return TransitionPolicyData{};
}

[[nodiscard]] constexpr StackPolicyData stackPolicyForId(const ScreenId id) noexcept {
    switch (id) {
    case ScreenId::PausePopup: return StackPolicy<::biofuel::game::screens::PausePopupScreen>::VALUE;
#ifdef BIOFUEL_ENABLE_DEV_SCREENS
    case ScreenId::DevHandLab: return StackPolicy<::biofuel::game::screens::DevHandLabScreen>::VALUE;
#endif
    case ScreenId::Loading:
    case ScreenId::MainMenu:
    case ScreenId::Idle:
    case ScreenId::Video:
    case ScreenId::Unknown:
    case ScreenId::Count:
        break;
    }
    return StackPolicyData{};
}

} // namespace biofuel::engine::ui::typed
