#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "game/screens/exploration/ExplorationScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include "engine/ui/typed/render/RenderElements.hpp"
#include <raylib.h>

namespace biofuel::engine::ui::typed::exploration {

// BackdropElement is structurally generic ("call Tag::render(screen, context)
// if visible") despite its name -- see engine/ui/typed/render/RenderElements.hpp.
// Used here for arbitrary 3D-scene rendering, not an actual backdrop.

struct WorldLayerTag {
    static constexpr std::string_view NAME = "exploration.world";
    [[nodiscard]] static bool visible(const ::biofuel::game::screens::ExplorationScreen&) noexcept { return true; }
    static void render(::biofuel::game::screens::ExplorationScreen& screen, RenderContext& context);
};

// Rendered after the world, before the HUD, into its own offscreen surface
// (see engine/graphics/ViewmodelPass.hpp) so it can never clip into or be
// clipped by world geometry.
struct ViewmodelLayerTag {
    static constexpr std::string_view NAME = "exploration.viewmodel";
    [[nodiscard]] static bool visible(const ::biofuel::game::screens::ExplorationScreen&) noexcept { return true; }
    static void render(::biofuel::game::screens::ExplorationScreen& screen, RenderContext& context);
};

struct HudLayerTag {
    static constexpr std::string_view NAME = "exploration.hud";
    [[nodiscard]] static bool visible(const ::biofuel::game::screens::ExplorationScreen&) noexcept { return true; }
    static void render(::biofuel::game::screens::ExplorationScreen& screen, RenderContext& context);
};

} // namespace biofuel::engine::ui::typed::exploration

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::ExplorationScreen> {
    static constexpr ScreenId ID = ::biofuel::game::screens::screen_id::Exploration;
    static constexpr std::string_view NAME = "ExplorationScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::ExplorationScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.8f,
        .easing = ::biofuel::engine::animation::Easing::easeOutCubic,
        .composer = TransitionComposer::Crossfade,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::ExplorationScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::ExplorationScreen,
        RenderElementList<
            render::BackdropElement<exploration::WorldLayerTag>,
            render::BackdropElement<exploration::ViewmodelLayerTag>,
            render::BackdropElement<exploration::HudLayerTag>>>;
};

} // namespace biofuel::engine::ui::typed
