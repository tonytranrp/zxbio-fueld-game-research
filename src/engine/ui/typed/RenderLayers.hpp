#pragma once

#include "engine/ui/typed/ScreenTypes.hpp"
#include <tuple>
#include <type_traits>

namespace biofuel::engine::ui::typed {

template<typename TScreen, typename... TLayers>
struct RenderLayerList {
    using Screen = TScreen;
    using Layers = std::tuple<TLayers...>;
    static constexpr auto size = sizeof...(TLayers);

    template<typename TLayer>
    static constexpr bool contains =
        (std::is_same_v<TLayer, TLayers> || ...);
};

template<typename TShaderModule>
struct BackdropLayer : BackdropLayerTag {
    using ShaderModule = TShaderModule;
};

template<typename TShaderModule>
struct CrossfadeLayer : CrossfadeLayerTag {
    using ShaderModule = TShaderModule;
};

template<typename TConfigTag>
struct BlurCaptureLayer : BlurCaptureLayerTag {
    using ConfigTag = TConfigTag;
};

struct VideoLayer : VideoLayerTag {};
struct MenuLayer : MenuLayerTag {};

template<auto ModelAsset>
struct ModelOverlayLayer : ModelOverlayLayerTag {
    static constexpr auto ASSET = ModelAsset;
};

struct DebugOverlayLayer : DebugOverlayLayerTag {};

template<typename... TElements>
struct RenderElementList {
    using Elements = std::tuple<TElements...>;
    static constexpr auto size = sizeof...(TElements);
};

} // namespace biofuel::engine::ui::typed
