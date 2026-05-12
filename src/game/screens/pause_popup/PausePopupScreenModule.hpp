#pragma once

#include "game/screens/pause_popup/PausePopupScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"

namespace biofuel::engine::ui::typed::pausepopup {

struct BlurCaptureElement { static constexpr std::string_view NAME = "pause.blur_capture"; };
struct PopupPanelElement { static constexpr std::string_view NAME = "pause.panel"; };
struct TitleTextElement { static constexpr std::string_view NAME = "pause.title"; };
struct SeparatorElement { static constexpr std::string_view NAME = "pause.separator"; };
struct VerticalMenuElement { static constexpr std::string_view NAME = "pause.vertical_menu"; };
struct HintTextElement { static constexpr std::string_view NAME = "pause.hint"; };

} // namespace biofuel::engine::ui::typed::pausepopup

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::PausePopupScreen> {
    static constexpr ScreenId ID = ScreenId::PausePopup;
    static constexpr std::string_view NAME = "PausePopupScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::PausePopupScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.0f,
        .easing = ::biofuel::engine::animation::Easing::linear,
        .composer = TransitionComposer::None,
    };
};

template<>
struct StackPolicy<::biofuel::game::screens::PausePopupScreen> {
    static constexpr StackPolicyData VALUE{
        .renderBelow = false,
        .updateBelow = false,
        .inputBelow = false,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::PausePopupScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::PausePopupScreen,
        RenderElementList<
            pausepopup::BlurCaptureElement,
            pausepopup::PopupPanelElement,
            pausepopup::TitleTextElement,
            pausepopup::SeparatorElement,
            pausepopup::VerticalMenuElement,
            pausepopup::HintTextElement>>;
};

} // namespace biofuel::engine::ui::typed
