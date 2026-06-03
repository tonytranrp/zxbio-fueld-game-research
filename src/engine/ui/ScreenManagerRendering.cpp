#include "ScreenManager.hpp"
#include "Screen.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include "engine/graphics/shaders/CrossfadeModule.hpp"
#include "engine/graphics/shaders/TypedShaderModule.hpp"
#include "engine/runtime/Runtime.hpp"

// ------------------------------------------------------------------------------
// ScreenManager — crossfade transition rendering
//
// Split out of ScreenManager.cpp. Owns the offscreen render surfaces and the
// crossfade shader used to composite an outgoing screen over an incoming one.
// ------------------------------------------------------------------------------

namespace biofuel::engine::ui {

void ScreenManager::ensureCrossfadeShader() {
    if (m_crossfadeShader.id > 0) return;

    // Shader is already compiled during startup loading — just look it up.
    auto& sm = ::biofuel::engine::runtime::Runtime::shader();
    m_crossfadeShader = ::biofuel::engine::runtime::typed::Shaders::get<::biofuel::engine::runtime::typed::shader::Crossfade>(sm);

    if (m_crossfadeShader.id > 0) {
        using Crossfade = ::biofuel::engine::runtime::typed::shader::Crossfade;
        m_crossfadeProgressLoc = ::biofuel::engine::runtime::typed::Shaders::loc<Crossfade, ::biofuel::engine::runtime::typed::shader::crossfade::Progress>(m_crossfadeShader);
        m_crossfadeTexInLoc = ::biofuel::engine::runtime::typed::Shaders::loc<Crossfade, ::biofuel::engine::runtime::typed::shader::crossfade::TextureIn>(m_crossfadeShader);
    }
}

bool ScreenManager::ensureTransitionTextures(i32 width, i32 height) {
    m_transitionOut.ensureSize(width, height);
    m_transitionIn.ensureSize(width, height);
    return m_transitionOut.valid() && m_transitionIn.valid();
}

void ScreenManager::renderSlotToBackbuffer(
    typed::ScreenSlot& slot,
    const i32 width,
    const i32 height)
{
    syncBridgeTransition(slot);
    typed::RenderContext context{
        .manager = this,
        .services = &::biofuel::engine::runtime::Runtime::services(),
        .screenId = slot.id,
        .screenWidth = width,
        .screenHeight = height,
        .transitionAlpha = slot.transition.alpha(),
        .frameTime = GetFrameTime(),
    };
    slot.dispatch->onRender(*slot.screen, context);
}

void ScreenManager::renderCrossfade(typed::ScreenSlot& outgoing, typed::ScreenSlot& incoming) {
    using namespace ::biofuel::engine::graphics;
    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    ensureCrossfadeShader();
    if (!ensureTransitionTextures(sw, sh)) {
        renderSlotToBackbuffer(incoming, sw, sh);
        return;
    }

    // Render outgoing screen
    {
        ScopedTextureMode textureScope(m_transitionOut.target());
        ClearBackground(BLANK);
        syncBridgeTransition(outgoing);
        typed::RenderContext context{
            .manager = this,
            .services = &::biofuel::engine::runtime::Runtime::services(),
            .screenId = outgoing.id,
            .screenWidth = sw,
            .screenHeight = sh,
            .transitionAlpha = outgoing.transition.alpha(),
            .frameTime = GetFrameTime(),
        };
        outgoing.dispatch->onRender(*outgoing.screen, context);
    }

    // Render incoming screen
    {
        ScopedTextureMode textureScope(m_transitionIn.target());
        ClearBackground(BLANK);
        syncBridgeTransition(incoming);
        typed::RenderContext context{
            .manager = this,
            .services = &::biofuel::engine::runtime::Runtime::services(),
            .screenId = incoming.id,
            .screenWidth = sw,
            .screenHeight = sh,
            .transitionAlpha = incoming.transition.alpha(),
            .frameTime = GetFrameTime(),
        };
        incoming.dispatch->onRender(*incoming.screen, context);
    }

    if (m_crossfadeShader.id == 0) {
        // Fallback: just draw incoming screen directly
        if (m_transitionIn.valid()) {
            Renderer::drawRenderTexture(m_transitionIn.texture());
        } else {
            renderSlotToBackbuffer(incoming, sw, sh);
        }
        return;
    }

    // Composite with crossfade shader
    using Crossfade = ::biofuel::engine::runtime::typed::shader::Crossfade;
    const f32 progress = incoming.transition.alpha();
    ::biofuel::engine::runtime::typed::Shaders::setTexture<Crossfade, ::biofuel::engine::runtime::typed::shader::crossfade::TextureIn>(
        m_crossfadeShader, m_crossfadeTexInLoc, m_transitionIn.texture());
    ::biofuel::engine::runtime::typed::Shaders::set<Crossfade, ::biofuel::engine::runtime::typed::shader::crossfade::Progress>(
        m_crossfadeShader, m_crossfadeProgressLoc, &progress);

    {
        ScopedShaderMode shaderScope(m_crossfadeShader);
        Renderer::drawRenderTexture(m_transitionOut.texture());
    }
}

} // namespace biofuel::engine::ui
