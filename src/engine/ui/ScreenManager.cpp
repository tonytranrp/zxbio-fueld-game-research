#include "ScreenManager.hpp"
#include "Screen.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include "engine/graphics/TransientResourceCache.hpp"
#include "engine/graphics/shaders/CrossfadeModule.hpp"
#include "engine/graphics/shaders/TypedShaderModule.hpp"
#include "engine/animation/Easing.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/runtime/Runtime.hpp"
#include <spdlog/spdlog.h>

namespace biofuel::engine::ui {

// ------------------------------------------------------------------------------
// Singleton
// ------------------------------------------------------------------------------

ScreenManager& ScreenManager::instance() noexcept {
    static ScreenManager instance;
    return instance;
}

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------

void ScreenManager::init() {
    connectOverrideSinks();
}

void ScreenManager::shutdown() {
    ::biofuel::engine::debug::MemoryTelemetry::snapshot("screen.shutdown.begin");
    clear();
    disconnectOverrideSinks();

    releaseTransitionTextures();
    ::biofuel::engine::graphics::TransientResourceCache::instance().releaseAll();
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
    if (!screen) {
        return;
    }

    const auto screenId = screen->screenId();
    push(m_screens.makeBridgeSlot(std::move(screen), transitionPolicyFor(screenId)));
}

void ScreenManager::push(typed::ScreenSlot slot) {
    if (isTransitioning()) {
        spdlog::warn("ScreenManager::push() ignored — transition in progress");
        return;
    }

    if (!m_screens.empty()) {
        typed::LifecycleContext pauseContext{*this, &::biofuel::engine::runtime::Runtime::services(), m_screens.back().id};
        m_screens.back().dispatch->onPause(*m_screens.back().screen, pauseContext);
    }

    slot->setManager(this);
    typed::LifecycleContext enterContext{*this, &::biofuel::engine::runtime::Runtime::services(), slot.id};
    slot.dispatch->onEnter(*slot.screen, enterContext);
    slot.transition.startIn(transitionPolicyFor(slot.id));
    syncBridgeTransition(slot);

    ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::animation::ScreenTransitionStarted>({
        .screenName = slot.name,
        .isEntering = true
    });

    m_screens.push(std::move(slot));
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

    m_screens.back().transition.startOut();
    syncBridgeTransition(m_screens.back());

    ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::animation::ScreenTransitionStarted>({
        .screenName = m_screens.back().name,
        .isEntering = false
    });
}

void ScreenManager::replace(std::unique_ptr<Screen> screen) {
    if (!screen) {
        return;
    }

    const auto screenId = screen->screenId();
    replace(m_screens.makeBridgeSlot(std::move(screen), transitionPolicyFor(screenId)));
}

void ScreenManager::replace(typed::ScreenSlot slot) {
    if (isTransitioning()) {
        spdlog::warn("ScreenManager::replace() ignored — transition in progress");
        return;
    }

    if (!m_screens.empty()) {
        typed::LifecycleContext pauseContext{*this, &::biofuel::engine::runtime::Runtime::services(), m_screens.back().id};
        m_screens.back().dispatch->onPause(*m_screens.back().screen, pauseContext);
        m_screens.back().transition.startOut();
        syncBridgeTransition(m_screens.back());
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::animation::ScreenTransitionStarted>({
            .screenName = m_screens.back().name,
            .isEntering = false
        });
    }

    slot->setManager(this);
    typed::LifecycleContext enterContext{*this, &::biofuel::engine::runtime::Runtime::services(), slot.id};
    slot.dispatch->onEnter(*slot.screen, enterContext);
    slot.transition.startIn(transitionPolicyFor(slot.id));
    syncBridgeTransition(slot);

    ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::animation::ScreenTransitionStarted>({
        .screenName = slot.name,
        .isEntering = true
    });

    m_screens.push(std::move(slot));
}

// ------------------------------------------------------------------------------
// Deferred Stack Operations — safe to call from onUpdate()
// ------------------------------------------------------------------------------

void ScreenManager::queuePush(std::unique_ptr<Screen> screen) {
    if (!screen) {
        return;
    }

    const auto screenId = screen->screenId();
    queuePush(m_screens.makeBridgeSlot(std::move(screen), transitionPolicyFor(screenId)));
}

void ScreenManager::queuePush(typed::ScreenSlot slot) {
    if (m_commands.action() != typed::ScreenCommandQueue::Action::None || m_commands.hasSlot()) {
        spdlog::warn("ScreenManager::queuePush() overwriting a pending screen action");
    }
    m_commands.push(std::move(slot));
}

void ScreenManager::queueReplace(std::unique_ptr<Screen> screen) {
    if (!screen) {
        return;
    }

    const auto screenId = screen->screenId();
    queueReplace(m_screens.makeBridgeSlot(std::move(screen), transitionPolicyFor(screenId)));
}

void ScreenManager::queueReplace(typed::ScreenSlot slot) {
    if (m_commands.action() != typed::ScreenCommandQueue::Action::None || m_commands.hasSlot()) {
        spdlog::warn("ScreenManager::queueReplace() overwriting a pending screen action");
    }
    m_commands.replace(std::move(slot));
}

void ScreenManager::queuePop() {
    m_commands.pop();
}

void ScreenManager::processPendingActions() {
    if (isTransitioning()) {
        return;
    }

    // Pop takes priority — process before push/replace
    if (m_commands.popRequested()) {
        m_commands.consumePop();
        pop();
        return;
    }

    if (m_commands.action() == typed::ScreenCommandQueue::Action::None) return;
    if (!m_commands.hasSlot()) return;

    const auto action = m_commands.action();
    auto slot = m_commands.consumeSlot();

    switch (action) {
        case typed::ScreenCommandQueue::Action::Push:
            push(std::move(slot));
            break;
        case typed::ScreenCommandQueue::Action::Replace:
            replace(std::move(slot));
            break;
        default:
            break;
    }
}

void ScreenManager::clear() {
    while (!m_screens.empty()) {
        typed::LifecycleContext exitContext{*this, &::biofuel::engine::runtime::Runtime::services(), m_screens.back().id};
        m_screens.back().dispatch->onExit(*m_screens.back().screen, exitContext);
        m_screens.popBack();
    }
    m_commands.clear();
}

// ------------------------------------------------------------------------------
// Crossfade Transition Preloading (called during LoadingScreen init tasks)
// ------------------------------------------------------------------------------

void ScreenManager::preloadCrossfadeShader() {
    ensureCrossfadeShader();
}

// ------------------------------------------------------------------------------
// Crossfade Transition Rendering
// ------------------------------------------------------------------------------

void ScreenManager::ensureCrossfadeShader() {
    if (m_crossfadeShader.id > 0) return;

    // Shader is already compiled during LoadingScreen — just look it up.
    auto& sm = ::biofuel::engine::runtime::Runtime::shader();
    m_crossfadeShader = ::biofuel::engine::runtime::typed::Shaders::get<::biofuel::engine::runtime::typed::shader::Crossfade>(sm);

    if (m_crossfadeShader.id > 0) {
        using Crossfade = ::biofuel::engine::runtime::typed::shader::Crossfade;
        m_crossfadeProgressLoc = ::biofuel::engine::runtime::typed::Shaders::loc<Crossfade, ::biofuel::engine::runtime::typed::shader::crossfade::Progress>(m_crossfadeShader);
        m_crossfadeTexInLoc = ::biofuel::engine::runtime::typed::Shaders::loc<Crossfade, ::biofuel::engine::runtime::typed::shader::crossfade::TextureIn>(m_crossfadeShader);
    }
}

void ScreenManager::ensureTransitionTextures(i32 width, i32 height) {
    m_transitionOut.ensureSize(width, height);
    m_transitionIn.ensureSize(width, height);
}

void ScreenManager::renderCrossfade(typed::ScreenSlot& outgoing, typed::ScreenSlot& incoming) {
    using namespace ::biofuel::engine::graphics;
    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    ensureCrossfadeShader();
    ensureTransitionTextures(sw, sh);

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
        Renderer::drawRenderTexture(m_transitionIn.texture());
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

// ------------------------------------------------------------------------------
// Per-Frame Delegation
// ------------------------------------------------------------------------------

void ScreenManager::update(f32 dt) {
    ::biofuel::engine::graphics::TransientResourceCache::instance().update(GetTime());

    // Update screens top-to-bottom
    bool updateBelow = true;
    for (auto it = m_screens.rbegin(); it != m_screens.rend(); ++it) {
        const bool isTop = (it == m_screens.rbegin());
        if (isTop || updateBelow) {
            syncBridgeTransition(*it);
            typed::UpdateContext updateContext{*this, &::biofuel::engine::runtime::Runtime::services(), it->id, dt};
            it->dispatch->onUpdate(*it->screen, updateContext);
        }
        updateBelow = updateBelow && stackPolicyFor(it->id).updateBelow;
    }

    // Advance transitions for all screens with easing support
    for (auto& slot : m_screens) {
        const auto previousState = slot.transition.state;
        if (slot.transition.advance(dt)) {
            syncBridgeTransition(slot);
            if (previousState == typed::SlotTransitionState::TransitionIn &&
                slot.transition.state == typed::SlotTransitionState::None) {
                ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::animation::ScreenTransitionCompleted>({
                    .screenName = slot.name,
                    .isEntering = true
                });
            }
        } else {
            syncBridgeTransition(slot);
        }
    }

    // Remove screens that completed transition-out
    for (auto it = m_screens.begin(); it != m_screens.end(); ) {
        if (it->transition.needsRemoval()) {
            const bool wasTop = (it->get() == m_screens.back().get());
            const auto removedId = it->id;
            ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::animation::ScreenTransitionCompleted>({
                .screenName = it->name,
                .isEntering = false
            });
            clearTransientOverrides(removedId);
            typed::LifecycleContext exitContext{*this, &::biofuel::engine::runtime::Runtime::services(), removedId};
            it->dispatch->onExit(*it->screen, exitContext);
            it = m_screens.erase(it);
            if (wasTop && !m_screens.empty()) {
                syncBridgeTransition(m_screens.back());
                typed::ResumeContext resumeContext{
                    .manager = *this,
                    .services = &::biofuel::engine::runtime::Runtime::services(),
                    .screenId = m_screens.back().id,
                    .poppedScreenId = removedId,
                    .reason = typed::ResumeReason::Popped,
                };
                m_screens.back().dispatch->onResume(*m_screens.back().screen, resumeContext);
            }
        } else {
            ++it;
        }
    }

    // Process any deferred push/replace from onUpdate() calls above
    processPendingActions();

    if (!isTransitioning()) {
        releaseTransitionTextures();
    }
}

void ScreenManager::render() {
    // Find transitioning screens for crossfade
    typed::ScreenSlot* outgoing = nullptr;
    typed::ScreenSlot* incoming = nullptr;
    for (auto& slot : m_screens) {
        if (slot.transition.state == typed::SlotTransitionState::TransitionOut) {
            outgoing = &slot;
        }
        if (slot.transition.state == typed::SlotTransitionState::TransitionIn) {
            incoming = &slot;
        }
    }

    if (outgoing && incoming &&
        incoming->transition.policy.composer == typed::TransitionComposer::Crossfade) {
        renderCrossfade(*outgoing, *incoming);
        return;
    }

    // Normal render path — screens bottom-to-top
    if (m_screens.empty()) {
        return;
    }

    size_t firstVisible = m_screens.size() - 1;
    while (firstVisible > 0 && stackPolicyFor(m_screens[firstVisible].id).renderBelow) {
        --firstVisible;
    }

    for (size_t i = firstVisible; i < m_screens.size(); ++i) {
        syncBridgeTransition(m_screens[i]);
        typed::RenderContext context{
            .manager = this,
            .services = &::biofuel::engine::runtime::Runtime::services(),
            .screenId = m_screens[i].id,
            .screenWidth = ::biofuel::engine::graphics::Renderer::screenWidth(),
            .screenHeight = ::biofuel::engine::graphics::Renderer::screenHeight(),
            .transitionAlpha = m_screens[i].transition.alpha(),
            .frameTime = GetFrameTime(),
        };
        m_screens[i].dispatch->onRender(*m_screens[i].screen, context);
    }
}

void ScreenManager::handleInput() {
    if (m_screens.empty()) {
        return;
    }

    bool inputBelow = true;
    const size_t initialSize = m_screens.size();
    for (size_t offset = 0; offset < initialSize; ++offset) {
        const size_t index = initialSize - 1 - offset;
        if (index >= m_screens.size()) {
            break;
        }

        auto& slot = m_screens[index];
        Screen& screen = *slot;
        const bool isTop = (offset == 0);
        const bool screenInputBelow = stackPolicyFor(slot.id).inputBelow;
        if (isTop || inputBelow) {
            syncBridgeTransition(slot);
            typed::InputContext inputContext{*this, &::biofuel::engine::runtime::Runtime::services(), slot.id};
            slot.dispatch->onInput(screen, inputContext);
            if (m_screens.size() != initialSize) {
                break;
            }
        }
        inputBelow = inputBelow && screenInputBelow;
        if (!inputBelow) {
            break;
        }
    }
}

void ScreenManager::captureScreen(Screen* screen, ::biofuel::engine::graphics::RenderSurface& target) {
    if (screen == nullptr) {
        return;
    }

    const i32 sw = ::biofuel::engine::graphics::Renderer::screenWidth();
    const i32 sh = ::biofuel::engine::graphics::Renderer::screenHeight();
    target.ensureSize(sw, sh);
    if (!target.valid()) {
        return;
    }

    ::biofuel::engine::graphics::ScopedTextureMode textureScope(target.target());
    ClearBackground(BLANK);
    screen->onRender();
}

void ScreenManager::syncBridgeTransition(typed::ScreenSlot& slot) const noexcept {
    if (!slot.screen) {
        return;
    }

    switch (slot.transition.state) {
    case typed::SlotTransitionState::None:
        slot.screen->m_transitionState = Screen::TransitionState::None;
        break;
    case typed::SlotTransitionState::TransitionIn:
        slot.screen->m_transitionState = Screen::TransitionState::TransitionIn;
        break;
    case typed::SlotTransitionState::TransitionOut:
        slot.screen->m_transitionState = Screen::TransitionState::TransitionOut;
        break;
    }

    slot.screen->m_transitionProgress = slot.transition.progress;
    slot.screen->m_transitionDuration = slot.transition.policy.duration;
    slot.screen->m_transitionEasing = slot.transition.policy.easing;
}

typed::TransitionPolicyData ScreenManager::transitionPolicyFor(const Screen& screen) const noexcept {
    return transitionPolicyFor(screen.screenId());
}

typed::TransitionPolicyData ScreenManager::transitionPolicyFor(const typed::ScreenId screenId) const noexcept {
    const auto& overrides = overridesFor(screenId);
    if (overrides.transition.active) {
        return overrides.transition.policy;
    }
    return typed::transitionPolicyForId(screenId);
}

typed::StackPolicyData ScreenManager::stackPolicyFor(const Screen& screen) noexcept {
    return stackPolicyFor(screen.screenId());
}

typed::StackPolicyData ScreenManager::stackPolicyFor(const typed::ScreenId screenId) noexcept {
    return typed::stackPolicyForId(screenId);
}

bool ScreenManager::isLayerEnabled(
    const typed::ScreenId screenId,
    const std::string_view layerName) const noexcept
{
    const auto& overrides = overridesFor(screenId);
    const auto layer = overrides.layers.find(layerName);
    if (layer == overrides.layers.end()) {
        return true;
    }
    return layer->second.enabled;
}

ScreenManager::ScreenOverrideState& ScreenManager::overridesFor(const typed::ScreenId screenId) noexcept {
    const auto index = typed::screenIdIndex(screenId);
    if (index >= m_overrides.size()) {
        return m_overrides[typed::screenIdIndex(typed::ScreenId::Unknown)];
    }
    return m_overrides[index];
}

const ScreenManager::ScreenOverrideState& ScreenManager::overridesFor(const typed::ScreenId screenId) const noexcept {
    const auto index = typed::screenIdIndex(screenId);
    if (index >= m_overrides.size()) {
        return m_overrides[typed::screenIdIndex(typed::ScreenId::Unknown)];
    }
    return m_overrides[index];
}

void ScreenManager::clearTransientOverrides(const typed::ScreenId screenId) {
    auto& overrides = overridesFor(screenId);
    if (overrides.transition.active && !overrides.transition.persistent) {
        overrides.transition = {};
    }
    if (overrides.debug.active && !overrides.debug.persistent) {
        overrides.debug = {};
    }

    for (auto it = overrides.layers.begin(); it != overrides.layers.end(); ) {
        if (!it->second.persistent) {
            it = overrides.layers.erase(it);
        } else {
            ++it;
        }
    }
}

void ScreenManager::connectOverrideSinks() {
    if (m_overrideSinksConnected) {
        return;
    }

    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::TransitionOverride>().connect<&ScreenManager::onTransitionOverride>(*this);
    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::LayerOverride>().connect<&ScreenManager::onLayerOverride>(*this);
    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::DebugRenderOverride>().connect<&ScreenManager::onDebugOverride>(*this);
    m_overrideSinksConnected = true;
}

void ScreenManager::disconnectOverrideSinks() {
    if (!m_overrideSinksConnected) {
        return;
    }

    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::TransitionOverride>().disconnect<&ScreenManager::onTransitionOverride>(*this);
    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::LayerOverride>().disconnect<&ScreenManager::onLayerOverride>(*this);
    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::DebugRenderOverride>().disconnect<&ScreenManager::onDebugOverride>(*this);
    m_overrideSinksConnected = false;
}

void ScreenManager::onTransitionOverride(const ::biofuel::engine::events::screen::ScreenTransitionOverrideEvent& event) {
    auto& overrides = overridesFor(event.screenId);
    if (!event.enabled) {
        overrides.transition = {};
        return;
    }

    overrides.transition = TransitionOverrideState{
        .active = true,
        .persistent = event.persistent,
        .policy = event.policy,
    };
}

void ScreenManager::onLayerOverride(const ::biofuel::engine::events::screen::ScreenLayerOverrideEvent& event) {
    if (event.layerName.empty()) {
        return;
    }

    auto& overrides = overridesFor(event.screenId);
    overrides.layers[event.layerName] = LayerOverrideState{
        .enabled = event.enabled,
        .persistent = event.persistent,
    };
}

void ScreenManager::onDebugOverride(const ::biofuel::engine::events::screen::ScreenDebugRenderOverrideEvent& event) {
    auto& overrides = overridesFor(event.screenId);
    overrides.debug = DebugOverrideState{
        .active = true,
        .enabled = event.enabled,
        .persistent = event.persistent,
    };
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
    for (const auto& slot : m_screens) {
        if (slot.transition.active()) {
            return true;
        }
    }
    return false;
}

void ScreenManager::releaseTransitionTextures() noexcept {
    m_transitionOut.release();
    m_transitionIn.release();
}

} // namespace biofuel::engine::ui
