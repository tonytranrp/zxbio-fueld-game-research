#pragma once

#ifdef BIOFUEL_ENABLE_DEV_SCREENS

#include "game/screens/dev_hand_lab/DevHandLabScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::DevHandLabScreen> {
    static constexpr ScreenId ID = ScreenId::DevHandLab;
    static constexpr std::string_view NAME = "DevHandLabScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::DevHandLabScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.08f,
        .easing = ::biofuel::engine::animation::Easing::easeOutCubic,
        .composer = TransitionComposer::None,
    };
};

template<>
struct StackPolicy<::biofuel::game::screens::DevHandLabScreen> {
    static constexpr StackPolicyData VALUE{
        .renderBelow = false,
        .updateBelow = false,
        .inputBelow = false,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::DevHandLabScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::DevHandLabScreen,
        RenderElementList<>>;
};

} // namespace biofuel::engine::ui::typed

#endif // BIOFUEL_ENABLE_DEV_SCREENS
