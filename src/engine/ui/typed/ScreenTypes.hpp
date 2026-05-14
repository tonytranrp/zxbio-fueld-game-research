#pragma once

#include "engine/core/Types.hpp"
#include "engine/animation/Easing.hpp"
#include <string_view>

namespace biofuel::engine::ui::typed {

enum class ScreenId : u8 {
    Unknown,
    Loading,
    MainMenu,
    Join,
    GamePlay,
    PausePopup,
    Idle,
    Video,
#ifdef BIOFUEL_ENABLE_DEV_SCREENS
    DevHandLab,
#endif
    Count,
};

static constexpr u8 SCREEN_ID_COUNT = static_cast<u8>(ScreenId::Count);

[[nodiscard]] constexpr u8 screenIdIndex(const ScreenId id) noexcept {
    return static_cast<u8>(id);
}

enum class TransitionComposer : u8 {
    None,
    Crossfade,
};

struct TransitionPolicyData {
    f32 duration = 0.5f;
    ::biofuel::engine::animation::Easing::Fn easing = ::biofuel::engine::animation::Easing::easeOutCubic;
    TransitionComposer composer = TransitionComposer::Crossfade;
};

struct StackPolicyData {
    bool renderBelow = false;
    bool updateBelow = false;
    bool inputBelow = false;
};

struct RenderLayerTag {};
struct BackdropLayerTag : RenderLayerTag {};
struct CrossfadeLayerTag : RenderLayerTag {};
struct BlurCaptureLayerTag : RenderLayerTag {};
struct VideoLayerTag : RenderLayerTag {};
struct MenuLayerTag : RenderLayerTag {};
struct ModelOverlayLayerTag : RenderLayerTag {};
struct DebugOverlayLayerTag : RenderLayerTag {};

} // namespace biofuel::engine::ui::typed
