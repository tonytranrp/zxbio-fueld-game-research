#include "game/screens/GameScreenIds.hpp"
﻿#pragma once

#include "game/screens/gameplay/GamePlayScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"

namespace biofuel::engine::ui::typed::gameplay {

struct PlaceholderElement { static constexpr std::string_view NAME = "gameplay.placeholder"; };

} // namespace biofuel::engine::ui::typed::gameplay

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::GamePlayScreen> {
    static constexpr ScreenId ID = screen_id::GamePlay;
    static constexpr std::string_view NAME = "GamePlayScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::GamePlayScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.35f,
        .easing = ::biofuel::engine::animation::Easing::easeOutCubic,
        .composer = TransitionComposer::Crossfade,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::GamePlayScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::GamePlayScreen,
        RenderElementList<gameplay::PlaceholderElement>>;
};

} // namespace biofuel::engine::ui::typed
