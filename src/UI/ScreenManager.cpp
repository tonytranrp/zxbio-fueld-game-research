#include "ScreenManager.hpp"
#include "Screen.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/ShaderManager.hpp"
#include "Utils/render/Shader/CrossfadeModule.hpp"
#include "AnimationController/animation/Easing.hpp"
#include "Data/Data.hpp"
#include <spdlog/spdlog.h>

namespace biofuel::ui {

// ------------------------------------------------------------------------------
// Singleton
// ------------------------------------------------------------------------------

ScreenManager& ScreenManager::instance() {
    static ScreenManager instance;
    return instance;
}

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------

void ScreenManager::init() {
    // Screen stack starts empty
}

void ScreenManager::shutdown() {
    clear();

    releaseTransitionTextures();
    // m_crossfadeShader is owned by ShaderManager — do NOT UnloadShader here.
    // ShaderManager::shutdown() handles unloading all shaders in its map.
    // Calling UnloadShader directly would double-free the shader's locs array
    // since ShaderManager holds a copy of the same Shader struct.
    m_crossfadeShader = {};
}

// ------------------------------------------------------------------------------
// Stack Operations
// ------------------------------------------------------------------------------

void ScreenManager::push(std::unique_ptr<Screen> screen) {
    if (isTransitioning()) {
        spdlog::warn("ScreenManager::push() ignored — transition in progress");
        return;
    }

    if (!m_screens.empty()) {
        m_screens.back()->onPause();
    }

    screen->setManager(this);
    screen->onEnter();
    screen->m_transitionState = Screen::TransitionState::TransitionIn;
    screen->m_transitionProgress = 0.0f;

    Data::eventBus().trigger(event::animation::ScreenTransitionStartedEvent{
        .screenName = "Screen",
        .isEntering = true
    });

    m_screens.push_back(std::move(screen));
}

void ScreenManager::pop() {
    if (isTransitioning()) {
        spdlog::warn("ScreenManager::pop() ignored — transition in progress");
        return;
    }

    if (m_screens.empty()) {
        spdlog::warn("ScreenManager::pop() called on empty stack");
        return;
    }

    m_screens.back()->m_transitionState = Screen::TransitionState::TransitionOut;
    m_screens.back()->m_transitionProgress = 0.0f;

    Data::eventBus().trigger(event::animation::ScreenTransitionStartedEvent{
        .screenName = "Screen",
        .isEntering = false
    });
}

void ScreenManager::replace(std::unique_ptr<Screen> screen) {
    if (isTransitioning()) {
        spdlog::warn("ScreenManager::replace() ignored — transition in progress");
        return;
    }

    if (!m_screens.empty()) {
        m_screens.back()->onPause();
        m_screens.back()->m_transitionState = Screen::TransitionState::TransitionOut;
        m_screens.back()->m_transitionProgress = 0.0f;
    }

    screen->setManager(this);
    screen->onEnter();
    screen->m_transitionState = Screen::TransitionState::TransitionIn;
    screen->m_transitionProgress = 0.0f;

    m_screens.push_back(std::move(screen));
}

// ------------------------------------------------------------------------------
// Deferred Stack Operations — safe to call from onUpdate()
// ------------------------------------------------------------------------------

void ScreenManager::queuePush(std::unique_ptr<Screen> screen) {
    if (m_pendingAction != PendingAction::None || m_pendingScreen) {
        spdlog::warn("ScreenManager::queuePush() overwriting a pending screen action");
    }
    m_pendingAction = PendingAction::Push;
    m_pendingScreen = std::move(screen);
}

void ScreenManager::queueReplace(std::unique_ptr<Screen> screen) {
    if (m_pendingAction != PendingAction::None || m_pendingScreen) {
        spdlog::warn("ScreenManager::queueReplace() overwriting a pending screen action");
    }
    m_pendingAction = PendingAction::Replace;
    m_pendingScreen = std::move(screen);
}

void ScreenManager::processPendingActions() {
    if (m_pendingAction == PendingAction::None) return;
    if (!m_pendingScreen) return;

    const auto action = m_pendingAction;
    m_pendingAction = PendingAction::None;

    switch (action) {
        case PendingAction::Push:
            push(std::move(m_pendingScreen));
            break;
        case PendingAction::Replace:
            replace(std::move(m_pendingScreen));
            break;
        default:
            break;
    }
}

void ScreenManager::clear() {
    while (!m_screens.empty()) {
        m_screens.back()->onExit();
        m_screens.pop_back();
    }
}

// ------------------------------------------------------------------------------
// Crossfade Transition Preloading (called during LoadingScreen init tasks)
// ------------------------------------------------------------------------------

void ScreenManager::preloadCrossfadeShader() {
    ensureCrossfadeShader();
}

void ScreenManager::preloadTransitionTextures() {
    const i32 sw = utils::render::Renderer::screenWidth();
    const i32 sh = utils::render::Renderer::screenHeight();
    ensureTransitionTextures(sw, sh);
}

// ------------------------------------------------------------------------------
// Crossfade Transition Rendering
// ------------------------------------------------------------------------------

void ScreenManager::ensureCrossfadeShader() {
    if (m_crossfadeShader.id > 0) return;

    // Shader is already compiled during LoadingScreen — just look it up.
    using namespace utils::render::shader;
    auto& sm = utils::render::ShaderManager::instance();
    m_crossfadeShader = sm.get(CrossfadeModule::NAME.data());

    if (m_crossfadeShader.id > 0) {
        m_crossfadeProgressLoc = sm.getLocation(m_crossfadeShader, CrossfadeModule::UNIFORM_PROGRESS.data());
        m_crossfadeTexInLoc = sm.getLocation(m_crossfadeShader, CrossfadeModule::UNIFORM_TEXTURE_IN.data());
    }
}

void ScreenManager::ensureTransitionTextures(i32 width, i32 height) {
    m_transitionOut.ensureSize(width, height);
    m_transitionIn.ensureSize(width, height);
}

void ScreenManager::renderCrossfade(Screen* outgoing, Screen* incoming) {
    using namespace utils::render;
    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    ensureCrossfadeShader();
    ensureTransitionTextures(sw, sh);

    // Render outgoing screen
    {
        ScopedTextureMode textureScope(m_transitionOut.target());
        ClearBackground(BLANK);
        outgoing->onRender();
    }

    // Render incoming screen
    {
        ScopedTextureMode textureScope(m_transitionIn.target());
        ClearBackground(BLANK);
        incoming->onRender();
    }

    if (m_crossfadeShader.id == 0) {
        // Fallback: just draw incoming screen directly
        Renderer::drawRenderTexture(m_transitionIn.texture());
        return;
    }

    // Composite with crossfade shader
    const f32 progress = incoming->m_transitionProgress;
    utils::render::ShaderManager::setValueTexture(m_crossfadeShader, m_crossfadeTexInLoc, m_transitionIn.texture());
    utils::render::ShaderManager::setValue(m_crossfadeShader, m_crossfadeProgressLoc, &progress, SHADER_UNIFORM_FLOAT);

    {
        ScopedShaderMode shaderScope(m_crossfadeShader);
        Renderer::drawRenderTexture(m_transitionOut.texture());
    }
}

// ------------------------------------------------------------------------------
// Per-Frame Delegation
// ------------------------------------------------------------------------------

void ScreenManager::update(f32 dt) {
    // Update screens top-to-bottom
    for (auto it = m_screens.rbegin(); it != m_screens.rend(); ++it) {
        bool isTop = (it == m_screens.rbegin());
        if (isTop || (*it)->m_passthroughUpdate) {
            (*it)->onUpdate(dt);
        }
    }

    // Advance transitions for all screens with easing support
    for (auto& screen : m_screens) {
        if (screen->isTransitioning()) {
            if (screen->m_transitionDuration > 0.0f) {
                screen->m_transitionProgress += dt / screen->m_transitionDuration;
            } else {
                // Instant transition — avoid division by zero / NaN
                screen->m_transitionProgress = 1.0f;
            }

            if (screen->m_transitionProgress >= 1.0f) {
                screen->m_transitionProgress = 1.0f;

                if (screen->m_transitionState == Screen::TransitionState::TransitionIn) {
                    screen->m_transitionState = Screen::TransitionState::None;
                    Data::eventBus().trigger(event::animation::ScreenTransitionCompletedEvent{
                        .screenName = "Screen",
                        .isEntering = true
                    });
                }
                // TransitionOut: stay at TransitionOut + progress=1.0 so needsRemoval() returns true
            }
        }
    }

    // Remove screens that completed transition-out
    for (auto it = m_screens.begin(); it != m_screens.end(); ) {
        if ((*it)->needsRemoval()) {
            bool wasTop = (it->get() == m_screens.back().get());
            (*it)->onExit();
            it = m_screens.erase(it);
            if (wasTop && !m_screens.empty()) {
                m_screens.back()->onResume();
            }
        } else {
            ++it;
        }
    }

    // Process any deferred push/replace from onUpdate() calls above
    processPendingActions();
}

void ScreenManager::render() {
    // Find transitioning screens for crossfade
    Screen* outgoing = nullptr;
    Screen* incoming = nullptr;
    for (auto& screen : m_screens) {
        if (screen->m_transitionState == Screen::TransitionState::TransitionOut) {
            outgoing = screen.get();
        }
        if (screen->m_transitionState == Screen::TransitionState::TransitionIn) {
            incoming = screen.get();
        }
    }

    if (outgoing && incoming) {
        renderCrossfade(outgoing, incoming);
        return;
    }

    // Normal render path — screens bottom-to-top
    for (auto& screen : m_screens) {
        bool isTop = (screen.get() == m_screens.back().get());
        if (isTop || screen->m_passthroughRender) {
            screen->onRender();
        }
    }
}

void ScreenManager::handleInput() {
    if (m_screens.empty()) {
        return;
    }

    m_screens.back()->onInput();

    if (m_screens.back()->m_passthroughInput && m_screens.size() > 1) {
        auto it = m_screens.rbegin();
        ++it;
        (*it)->onInput();
    }
}

// ------------------------------------------------------------------------------
// Accessors
// ------------------------------------------------------------------------------

Screen* ScreenManager::currentScreen() const noexcept {
    if (m_screens.empty()) {
        return nullptr;
    }
    return m_screens.back().get();
}

Screen* ScreenManager::screenBelowTop() const noexcept {
    if (m_screens.size() < 2) {
        return nullptr;
    }
    return m_screens[m_screens.size() - 2].get();
}

bool ScreenManager::isEmpty() const noexcept {
    return m_screens.empty();
}

size_t ScreenManager::stackSize() const noexcept {
    return m_screens.size();
}

bool ScreenManager::isTransitioning() const noexcept {
    for (const auto& screen : m_screens) {
        if (screen->isTransitioning()) {
            return true;
        }
    }
    return false;
}

void ScreenManager::releaseTransitionTextures() noexcept {
    m_transitionOut.release();
    m_transitionIn.release();
}

} // namespace biofuel::ui
