#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "game/screens/loading/LoadingScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include "engine/graphics/shaders/LoadingPreludeModule.hpp"

namespace biofuel::engine::ui::typed::loading {

struct BackdropElement { static constexpr std::string_view NAME = "loading.backdrop"; };
struct TitleTextElement { static constexpr std::string_view NAME = "loading.title"; };
struct LoadingPanelElement { static constexpr std::string_view NAME = "loading.panel"; };
struct ProgressBarElement { static constexpr std::string_view NAME = "loading.progress"; };
struct StatusTextElement { static constexpr std::string_view NAME = "loading.status"; };
struct SkipHintElement { static constexpr std::string_view NAME = "loading.skip_hint"; };
struct FooterTextElement { static constexpr std::string_view NAME = "loading.footer"; };

} // namespace biofuel::engine::ui::typed::loading

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::LoadingScreen> {
    static constexpr ScreenId ID = screen_id::Loading;
    static constexpr std::string_view NAME = "LoadingScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::LoadingScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.5f,
        .easing = ::biofuel::engine::animation::Easing::easeOutCubic,
        .composer = TransitionComposer::Crossfade,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::LoadingScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::LoadingScreen,
        RenderElementList<
            loading::BackdropElement,
            loading::TitleTextElement,
            loading::LoadingPanelElement,
            loading::ProgressBarElement,
            loading::StatusTextElement,
            loading::SkipHintElement,
            loading::FooterTextElement>>;
};

} // namespace biofuel::engine::ui::typed
