#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "game/screens/bevy_demo/BevyDemoScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include "engine/ui/typed/render/RenderElements.hpp"
#include <raylib.h>

namespace biofuel::engine::ui::typed::bevydemo {

struct BevyFrameTag {
    static constexpr std::string_view NAME = "bevy_demo.frame";
    [[nodiscard]] static bool visible(const ::biofuel::game::screens::BevyDemoScreen& screen) noexcept;
    [[nodiscard]] static Texture2D frame(const ::biofuel::game::screens::BevyDemoScreen& screen) noexcept;
};

} // namespace biofuel::engine::ui::typed::bevydemo

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::BevyDemoScreen> {
    static constexpr ScreenId ID = ::biofuel::game::screens::screen_id::BevyDemo;
    static constexpr std::string_view NAME = "BevyDemoScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::BevyDemoScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.0f,
        .easing = ::biofuel::engine::animation::Easing::linear,
        .composer = TransitionComposer::None,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::BevyDemoScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::BevyDemoScreen,
        RenderElementList<render::VideoFrameElement<bevydemo::BevyFrameTag>>>;
};

} // namespace biofuel::engine::ui::typed
