#pragma once

#include "engine/animation/typed/AnimationTracks.hpp"
#include "engine/runtime/typed/Assets.hpp"
#include "engine/ui/typed/RenderPipeline.hpp"
#include <string_view>

namespace biofuel::engine::ui::typed::render {

template<typename TBlurConfigTag>
struct BlurCaptureElement {
    static constexpr std::string_view NAME = TBlurConfigTag::NAME;
};

template<typename TShaderAsset>
struct ShaderBackdropElement {
    static constexpr std::string_view NAME = TShaderAsset::Name;
};

template<typename TModelAsset>
struct ModelOverlayElement {
    static constexpr std::string_view NAME = TModelAsset::Name;
};

template<typename TTrackTag>
struct DebugOverlayElement {
    static constexpr std::string_view NAME = TTrackTag::Name;
};

} // namespace biofuel::engine::ui::typed::render

namespace biofuel::engine::ui::typed {

template<typename TDebugTag, typename TScreen>
struct RenderElementExecutor<render::DebugOverlayElement<TDebugTag>, TScreen> {
    static void render(TScreen&, RenderContext&) {}
};

} // namespace biofuel::engine::ui::typed
