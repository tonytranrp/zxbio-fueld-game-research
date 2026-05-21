#include "game/screens/GameScreenIds.hpp"
﻿#pragma once

#include "game/screens/join/JoinScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"

namespace biofuel::engine::ui::typed::join {

struct JoinPanelElement { static constexpr std::string_view NAME = "join.panel"; };

} // namespace biofuel::engine::ui::typed::join

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::JoinScreen> {
    static constexpr ScreenId ID = screen_id::Join;
    static constexpr std::string_view NAME = "JoinScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::JoinScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.35f,
        .easing = ::biofuel::engine::animation::Easing::easeOutCubic,
        .composer = TransitionComposer::Crossfade,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::JoinScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::JoinScreen,
        RenderElementList<join::JoinPanelElement>>;
};

} // namespace biofuel::engine::ui::typed
