#pragma once

#include "game/screens/main_menu/MainMenuScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include "engine/graphics/shaders/MainMenuBgModule.hpp"

namespace biofuel::engine::ui::typed::mainmenu {

struct BackdropElement { static constexpr std::string_view NAME = "main_menu.backdrop"; };
struct TitleBlockElement { static constexpr std::string_view NAME = "main_menu.title_block"; };
struct HintTextElement { static constexpr std::string_view NAME = "main_menu.hints"; };
struct HorizontalMenuElement { static constexpr std::string_view NAME = "main_menu.horizontal_menu"; };
struct FooterTextElement { static constexpr std::string_view NAME = "main_menu.footer"; };

} // namespace biofuel::engine::ui::typed::mainmenu

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::MainMenuScreen> {
    static constexpr ScreenId ID = ScreenId::MainMenu;
    static constexpr std::string_view NAME = "MainMenuScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::MainMenuScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.5f,
        .easing = ::biofuel::engine::animation::Easing::easeOutCubic,
        .composer = TransitionComposer::Crossfade,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::MainMenuScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::MainMenuScreen,
        RenderElementList<
            mainmenu::BackdropElement,
            mainmenu::TitleBlockElement,
            mainmenu::HintTextElement,
            mainmenu::HorizontalMenuElement,
            mainmenu::FooterTextElement>>;
};

} // namespace biofuel::engine::ui::typed
