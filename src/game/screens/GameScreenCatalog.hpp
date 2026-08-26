#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "engine/ui/typed/ScreenRegistry.hpp"
#include "engine/ui/typed/ScreenValidation.hpp"
#include "game/screens/loading/LoadingScreenModule.hpp"
#include "game/screens/main_menu/MainMenuScreenModule.hpp"
#include "game/screens/pause_popup/PausePopupScreenModule.hpp"
#include "game/screens/idle/IdleScreenModule.hpp"
#include "game/screens/video/VideoScreenModule.hpp"
#ifdef BIOFUEL_WITH_BEVY_BRIDGE
#include "game/screens/bevy_demo/BevyDemoScreenModule.hpp"
#endif
namespace biofuel::game::screens {

using GameScreenRegistry = ::biofuel::engine::ui::typed::ScreenRegistry<
    LoadingScreen,
    MainMenuScreen,
    PausePopupScreen,
    IdleScreen,
    VideoScreen
#ifdef BIOFUEL_WITH_BEVY_BRIDGE
    , BevyDemoScreen
#endif
    >;

static_assert(::biofuel::engine::ui::typed::validateScreenRegistry<GameScreenRegistry>());

namespace detail {

[[nodiscard]] constexpr bool hasTransitionPolicySwitchEntry(const ::biofuel::engine::ui::typed::ScreenId id) noexcept {
    switch (id) {
    case screen_id::Loading:
    case screen_id::MainMenu:
    case screen_id::PausePopup:
    case screen_id::Idle:
    case screen_id::Video:
#ifdef BIOFUEL_WITH_BEVY_BRIDGE
    case screen_id::BevyDemo:
#endif
        return true;
    case ::biofuel::engine::ui::typed::ScreenId::Unknown:
    case ::biofuel::engine::ui::typed::ScreenId::Count:
        break;
    default:
        break;
    }
    return false;
}

[[nodiscard]] constexpr bool hasStackPolicySwitchEntry(const ::biofuel::engine::ui::typed::ScreenId id) noexcept {
    switch (id) {
    case screen_id::PausePopup:
        return true;
    case screen_id::Loading:
    case screen_id::MainMenu:
    case screen_id::Idle:
    case screen_id::Video:
#ifdef BIOFUEL_WITH_BEVY_BRIDGE
    case screen_id::BevyDemo:
#endif
    case ::biofuel::engine::ui::typed::ScreenId::Unknown:
    case ::biofuel::engine::ui::typed::ScreenId::Count:
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
    case screen_id::Loading: return typed::TransitionPolicy<LoadingScreen>::VALUE;
    case screen_id::MainMenu: return typed::TransitionPolicy<MainMenuScreen>::VALUE;
    case screen_id::PausePopup: return typed::TransitionPolicy<PausePopupScreen>::VALUE;
    case screen_id::Idle: return typed::TransitionPolicy<IdleScreen>::VALUE;
    case screen_id::Video: return typed::TransitionPolicy<VideoScreen>::VALUE;
#ifdef BIOFUEL_WITH_BEVY_BRIDGE
    case screen_id::BevyDemo: return typed::TransitionPolicy<BevyDemoScreen>::VALUE;
#endif
    case ::biofuel::engine::ui::typed::ScreenId::Unknown:
    case ::biofuel::engine::ui::typed::ScreenId::Count:
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
    case screen_id::PausePopup: return typed::StackPolicy<PausePopupScreen>::VALUE;
    case screen_id::Loading:
    case screen_id::MainMenu:
    case screen_id::Idle:
    case screen_id::Video:
#ifdef BIOFUEL_WITH_BEVY_BRIDGE
    case screen_id::BevyDemo:
#endif
    case ::biofuel::engine::ui::typed::ScreenId::Unknown:
    case ::biofuel::engine::ui::typed::ScreenId::Count:
        break;
    default:
        break;
    }
    return typed::StackPolicyData{};
}

} // namespace biofuel::game::screens
