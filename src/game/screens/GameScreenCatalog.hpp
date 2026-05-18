#pragma once

#include "engine/ui/typed/ScreenRegistry.hpp"
#include "engine/ui/typed/ScreenValidation.hpp"
#include "game/screens/loading/LoadingScreenModule.hpp"
#include "game/screens/main_menu/MainMenuScreenModule.hpp"
#include "game/screens/join/JoinScreenModule.hpp"
#include "game/screens/gameplay/GamePlayScreenModule.hpp"
#include "game/screens/pause_popup/PausePopupScreenModule.hpp"
#include "game/screens/calibration/CalibrationScreenModule.hpp"
#include "game/screens/idle/IdleScreenModule.hpp"
#include "game/screens/video/VideoScreenModule.hpp"
namespace biofuel::game::screens {

using GameScreenRegistry = ::biofuel::engine::ui::typed::ScreenRegistry<
    LoadingScreen,
    MainMenuScreen,
    JoinScreen,
    GamePlayScreen,
    PausePopupScreen,
    CalibrationScreen,
    IdleScreen,
    VideoScreen
    >;

static_assert(::biofuel::engine::ui::typed::validateScreenRegistry<GameScreenRegistry>());

namespace detail {

[[nodiscard]] constexpr bool hasTransitionPolicySwitchEntry(const ::biofuel::engine::ui::typed::ScreenId id) noexcept {
    using enum ::biofuel::engine::ui::typed::ScreenId;
    switch (id) {
    case Loading:
    case MainMenu:
    case Join:
    case GamePlay:
    case PausePopup:
    case Calibration:
    case Idle:
    case Video:
        return true;
    case Unknown:
    case Count:
        break;
    default:
        break;
    }
    return false;
}

[[nodiscard]] constexpr bool hasStackPolicySwitchEntry(const ::biofuel::engine::ui::typed::ScreenId id) noexcept {
    using enum ::biofuel::engine::ui::typed::ScreenId;
    switch (id) {
    case PausePopup:
    case Calibration:
        return true;
    case Loading:
    case MainMenu:
    case Join:
    case GamePlay:
    case Idle:
    case Video:
    case Unknown:
    case Count:
        break;
    default:
        break;
    }
    return false;
}

[[nodiscard]] constexpr bool isDefaultStackPolicy(const ::biofuel::engine::ui::typed::StackPolicyData policy) noexcept {
    return !policy.renderBelow && !policy.updateBelow && !policy.inputBelow;
}

template<typename TRegistry>
struct PolicySwitchValidator;

template<typename... TScreens>
struct PolicySwitchValidator<::biofuel::engine::ui::typed::ScreenRegistry<TScreens...>> {
    static consteval bool valid() {
        namespace typed = ::biofuel::engine::ui::typed;
        static_assert((hasTransitionPolicySwitchEntry(typed::ScreenSpec<TScreens>::ID) && ...),
            "Every registered screen must have an entry in transitionPolicyForId().");
        static_assert(((isDefaultStackPolicy(typed::StackPolicy<TScreens>::VALUE)
            || hasStackPolicySwitchEntry(typed::ScreenSpec<TScreens>::ID)) && ...),
            "Every screen with a non-default StackPolicy must have an entry in stackPolicyForId().");
        return true;
    }
};

} // namespace detail

static_assert(detail::PolicySwitchValidator<GameScreenRegistry>::valid());

[[nodiscard]] constexpr ::biofuel::engine::ui::typed::TransitionPolicyData transitionPolicyForId(
    const ::biofuel::engine::ui::typed::ScreenId id) noexcept
{
    namespace typed = ::biofuel::engine::ui::typed;
    using enum typed::ScreenId;
    switch (id) {
    case Loading: return typed::TransitionPolicy<LoadingScreen>::VALUE;
    case MainMenu: return typed::TransitionPolicy<MainMenuScreen>::VALUE;
    case Join: return typed::TransitionPolicy<JoinScreen>::VALUE;
    case GamePlay: return typed::TransitionPolicy<GamePlayScreen>::VALUE;
    case PausePopup: return typed::TransitionPolicy<PausePopupScreen>::VALUE;
    case Calibration: return typed::TransitionPolicy<CalibrationScreen>::VALUE;
    case Idle: return typed::TransitionPolicy<IdleScreen>::VALUE;
    case Video: return typed::TransitionPolicy<VideoScreen>::VALUE;
    case Unknown:
    case Count:
        break;
    default:
        break;
    }
    return typed::TransitionPolicyData{};
}

[[nodiscard]] constexpr ::biofuel::engine::ui::typed::StackPolicyData stackPolicyForId(
    const ::biofuel::engine::ui::typed::ScreenId id) noexcept
{
    namespace typed = ::biofuel::engine::ui::typed;
    using enum typed::ScreenId;
    switch (id) {
    case PausePopup: return typed::StackPolicy<PausePopupScreen>::VALUE;
    case Calibration: return typed::StackPolicy<CalibrationScreen>::VALUE;
    case Loading:
    case MainMenu:
    case Join:
    case GamePlay:
    case Idle:
    case Video:
    case Unknown:
    case Count:
        break;
    default:
        break;
    }
    return typed::StackPolicyData{};
}

} // namespace biofuel::game::screens
