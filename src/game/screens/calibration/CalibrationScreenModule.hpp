#pragma once

#include "game/screens/calibration/CalibrationScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::CalibrationScreen> {
    static constexpr ScreenId ID = ScreenId::Calibration;
    static constexpr std::string_view NAME = "CalibrationScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::CalibrationScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.08f,
        .easing = ::biofuel::engine::animation::Easing::easeOutCubic,
        .composer = TransitionComposer::None,
    };
};

template<>
struct StackPolicy<::biofuel::game::screens::CalibrationScreen> {
    static constexpr StackPolicyData VALUE{
        .renderBelow = true,
        .updateBelow = false,
        .inputBelow = false,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::CalibrationScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::CalibrationScreen,
        RenderElementList<>>;
};

} // namespace biofuel::engine::ui::typed
