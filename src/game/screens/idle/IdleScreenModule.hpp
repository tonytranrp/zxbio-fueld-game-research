#pragma once

#include "game/screens/idle/IdleScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include "engine/ui/typed/render/RenderElements.hpp"
#include "engine/graphics/shaders/MainMenuBgModule.hpp"
#include <raylib.h>

namespace biofuel::engine::ui::typed::idle {

struct VideoFrameTag {
    static constexpr std::string_view NAME = "idle.video";
    [[nodiscard]] static bool visible(const ::biofuel::game::screens::IdleScreen& screen) noexcept;
    [[nodiscard]] static Texture2D frame(const ::biofuel::game::screens::IdleScreen& screen) noexcept;
};

struct FallbackColorTag {
    static constexpr std::string_view NAME = "idle.fallback_color";
    [[nodiscard]] static bool visible(const ::biofuel::game::screens::IdleScreen& screen) noexcept;
    [[nodiscard]] static Color color(const ::biofuel::game::screens::IdleScreen& screen) noexcept;
};

struct FallbackBackdropTag {
    static constexpr std::string_view NAME = "idle.fallback_backdrop";
    [[nodiscard]] static bool visible(const ::biofuel::game::screens::IdleScreen& screen) noexcept;
    static void render(const ::biofuel::game::screens::IdleScreen& screen, RenderContext& context);
};

} // namespace biofuel::engine::ui::typed::idle

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::IdleScreen> {
    static constexpr ScreenId ID = ScreenId::Idle;
    static constexpr std::string_view NAME = "IdleScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::IdleScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.5f,
        .easing = ::biofuel::engine::animation::Easing::easeOutCubic,
        .composer = TransitionComposer::None,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::IdleScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::IdleScreen,
        RenderElementList<
            render::VideoFrameElement<idle::VideoFrameTag>,
            render::FullscreenColorElement<idle::FallbackColorTag>,
            render::BackdropElement<idle::FallbackBackdropTag>>>;
};

} // namespace biofuel::engine::ui::typed
