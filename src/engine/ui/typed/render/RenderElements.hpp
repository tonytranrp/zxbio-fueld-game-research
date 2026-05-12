#pragma once

#include "engine/ui/typed/RenderPipeline.hpp"
#include "engine/graphics/Render.hpp"
#include <raylib.h>

namespace biofuel::engine::ui::typed::render {

template<typename TVideoTag>
struct VideoFrameElement {
    static constexpr std::string_view NAME = TVideoTag::NAME;
};

template<typename TColorTag>
struct FullscreenColorElement {
    static constexpr std::string_view NAME = TColorTag::NAME;
};

template<typename TBackdropTag>
struct BackdropElement {
    static constexpr std::string_view NAME = TBackdropTag::NAME;
};

} // namespace biofuel::engine::ui::typed::render

namespace biofuel::engine::ui::typed {

template<typename TVideoTag, typename TScreen>
struct RenderElementExecutor<render::VideoFrameElement<TVideoTag>, TScreen> {
    static void render(TScreen& screen, RenderContext&) {
        if (!TVideoTag::visible(screen)) {
            return;
        }

        const Texture2D frame = TVideoTag::frame(screen);
        if (frame.id != 0) {
            ::biofuel::engine::graphics::Renderer::drawFullscreenTexture(frame);
        }
    }
};

template<typename TColorTag, typename TScreen>
struct RenderElementExecutor<render::FullscreenColorElement<TColorTag>, TScreen> {
    static void render(TScreen& screen, RenderContext&) {
        if (TColorTag::visible(screen)) {
            ::biofuel::engine::graphics::Renderer::drawFullscreen(TColorTag::color(screen));
        }
    }
};

template<typename TBackdropTag, typename TScreen>
struct RenderElementExecutor<render::BackdropElement<TBackdropTag>, TScreen> {
    static void render(TScreen& screen, RenderContext& context) {
        if (TBackdropTag::visible(screen)) {
            TBackdropTag::render(screen, context);
        }
    }
};

} // namespace biofuel::engine::ui::typed

#include "engine/ui/typed/render/EffectElements.hpp"
