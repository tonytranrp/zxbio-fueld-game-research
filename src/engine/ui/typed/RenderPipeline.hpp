#pragma once

#include "engine/ui/typed/RenderContext.hpp"
#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include <concepts>
#include <type_traits>

namespace biofuel::engine::ui::typed {

template<typename TScreen>
struct ScreenRenderState {};

template<typename TElement, typename TScreen>
struct RenderElementExecutor {
    static void render(TScreen& screen, RenderContext& context) {
        TElement::render(screen, context);
    }
};

template<typename TLayer, typename TScreen>
struct RenderLayerExecutor {
    static void render(TScreen& screen, RenderContext& context) {
        TLayer::render(screen, context);
    }
};

namespace detail {

template<typename T>
concept NamedRenderNode = requires {
    { T::NAME } -> std::convertible_to<std::string_view>;
};

template<typename TElement, typename TScreen>
void renderElement(TScreen& screen, RenderContext& context) {
    if constexpr (NamedRenderNode<TElement>) {
        if (!context.layerEnabled(TElement::NAME)) {
            return;
        }
    }

    RenderElementExecutor<TElement, TScreen>::render(screen, context);
}

template<typename TLayer, typename TScreen>
void renderLayer(TScreen& screen, RenderContext& context) {
    if constexpr (NamedRenderNode<TLayer>) {
        if (!context.layerEnabled(TLayer::NAME)) {
            return;
        }
    }

    RenderLayerExecutor<TLayer, TScreen>::render(screen, context);
}

template<typename TScreen, typename TLayerList>
struct PipelineRunner;

template<typename TScreen, typename... TLayers>
struct PipelineRunner<TScreen, RenderLayerList<TScreen, TLayers...>> {
    static void render(TScreen& screen, RenderContext& context) {
        (renderLayer<TLayers>(screen, context), ...);
    }
};

} // namespace detail

template<typename... TElements, typename TScreen>
struct RenderLayerExecutor<RenderElementList<TElements...>, TScreen> {
    static void render(TScreen& screen, RenderContext& context) {
        (detail::renderElement<TElements>(screen, context), ...);
    }
};

template<typename TScreen>
struct RenderPipeline {
    static void render(TScreen& screen, RenderContext& context) {
        using CleanScreen = std::remove_cvref_t<TScreen>;
        using LayerList = typename RenderLayers<CleanScreen>::Type;
        detail::PipelineRunner<CleanScreen, LayerList>::render(screen, context);
    }
};

} // namespace biofuel::engine::ui::typed
