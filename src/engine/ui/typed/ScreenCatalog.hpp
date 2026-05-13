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

namespace detail {

[[nodiscard]] constexpr bool hasTransitionPolicySwitchEntry(const ScreenId id) noexcept {
    switch (id) {
    case ScreenId::Loading:
    case ScreenId::MainMenu:
    case ScreenId::PausePopup:
    case ScreenId::Idle:
    case ScreenId::Video:
#ifdef BIOFUEL_ENABLE_DEV_SCREENS
    case ScreenId::DevHandLab:
#endif
        return true;
    case ScreenId::Unknown:
    case ScreenId::Count:
        break;
    }
    return false;
}

[[nodiscard]] constexpr bool hasStackPolicySwitchEntry(const ScreenId id) noexcept {
    switch (id) {
    case ScreenId::PausePopup:
#ifdef BIOFUEL_ENABLE_DEV_SCREENS
    case ScreenId::DevHandLab:
#endif
        return true;
    case ScreenId::Loading:
    case ScreenId::MainMenu:
    case ScreenId::Idle:
    case ScreenId::Video:
    case ScreenId::Unknown:
    case ScreenId::Count:
        break;
    }
    return false;
}

[[nodiscard]] constexpr bool isDefaultStackPolicy(const StackPolicyData policy) noexcept {
    return !policy.renderBelow && !policy.updateBelow && !policy.inputBelow;
}

template<typename TRegistry>
struct PolicySwitchValidator;

template<typename... TScreens>
struct PolicySwitchValidator<ScreenRegistry<TScreens...>> {
    static consteval bool valid() {
        static_assert((hasTransitionPolicySwitchEntry(ScreenSpec<TScreens>::ID) && ...),
            "Every registered screen must have an entry in transitionPolicyForId().");
        static_assert(((isDefaultStackPolicy(StackPolicy<TScreens>::VALUE)
            || hasStackPolicySwitchEntry(ScreenSpec<TScreens>::ID)) && ...),
            "Every screen with a non-default StackPolicy must have an entry in stackPolicyForId().");
        return true;
    }
};

} // namespace detail

static_assert(detail::PolicySwitchValidator<AppScreenRegistry>::valid());

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
