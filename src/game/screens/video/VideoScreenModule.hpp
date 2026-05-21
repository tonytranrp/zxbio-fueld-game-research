#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "game/screens/video/VideoScreen.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include "engine/ui/typed/render/RenderElements.hpp"
#include <raylib.h>

namespace biofuel::engine::ui::typed::videoscreen {

struct VideoFrameTag {
    static constexpr std::string_view NAME = "video.frame";
    [[nodiscard]] static bool visible(const ::biofuel::game::screens::VideoScreen& screen) noexcept;
    [[nodiscard]] static Texture2D frame(const ::biofuel::game::screens::VideoScreen& screen) noexcept;
};

struct FallbackColorTag {
    static constexpr std::string_view NAME = "video.fallback_color";
    [[nodiscard]] static bool visible(const ::biofuel::game::screens::VideoScreen& screen) noexcept;
    [[nodiscard]] static Color color(const ::biofuel::game::screens::VideoScreen& screen) noexcept;
};

} // namespace biofuel::engine::ui::typed::videoscreen

namespace biofuel::engine::ui::typed {

template<>
struct ScreenSpec<::biofuel::game::screens::VideoScreen> {
    static constexpr ScreenId ID = screen_id::Video;
    static constexpr std::string_view NAME = "VideoScreen";
};

template<>
struct TransitionPolicy<::biofuel::game::screens::VideoScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.0f,
        .easing = ::biofuel::engine::animation::Easing::linear,
        .composer = TransitionComposer::None,
    };
};

template<>
struct RenderLayers<::biofuel::game::screens::VideoScreen> {
    using Type = RenderLayerList<
        ::biofuel::game::screens::VideoScreen,
        RenderElementList<
            render::VideoFrameElement<videoscreen::VideoFrameTag>,
            render::FullscreenColorElement<videoscreen::FallbackColorTag>>>;
};

} // namespace biofuel::engine::ui::typed
