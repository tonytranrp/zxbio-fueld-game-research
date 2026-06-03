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

    // Discard any pending async screen transition
    m_loadingTasks.clear(::biofuel::engine::runtime::Runtime::tasks());
    m_pendingSlot.reset();
    m_pendingSlotAction = typed::ScreenCommandQueue::Action::None;

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
    if (hasPendingScreenTransition()) {
        spdlog::warn("ScreenManager::push() ignored — pending screen transition in progress");
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
    if (hasPendingScreenTransition()) {
        spdlog::warn("ScreenManager::pop() ignored — pending screen transition in progress");
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
    if (hasPendingScreenTransition()) {
        spdlog::warn("ScreenManager::replace() ignored — pending screen transition in progress");
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
    if (hasPendingScreenTransition()) {
        spdlog::warn("ScreenManager::queuePush() ignored — pending screen transition in progress");
        return;
    }
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
    if (hasPendingScreenTransition()) {
        spdlog::warn("ScreenManager::queueReplace() ignored — pending screen transition in progress");
        return;
    }
    if (m_commands.action() != typed::ScreenCommandQueue::Action::None || m_commands.hasSlot()) {
        spdlog::warn("ScreenManager::queueReplace() overwriting a pending screen action");
    }
    m_commands.replace(std::move(slot));
}

void ScreenManager::queuePop() {
    if (hasPendingScreenTransition()) {
        spdlog::warn("ScreenManager::queuePop() ignored — pending screen transition in progress");
        return;
    }
    m_commands.pop();
}

void ScreenManager::processPendingActions() {
    if (isTransitioning()) {
        return;
    }
    if (hasPendingScreenTransition()) {
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

    // Set manager so buildLoadingTasks() can access Runtime::tasks() via manager()
    slot->setManager(this);

    // Let the screen register async init tasks before onEnter()
    m_loadingTasks.clear(::biofuel::engine::runtime::Runtime::tasks());
    slot->buildLoadingTasks(m_loadingTasks);

    if (m_loadingTasks.totalTasks() == 0) {
        // Fast path: no loading tasks, proceed synchronously
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
        return;
    }

    // Slow path: store pending slot and process tasks over multiple frames
    m_pendingSlot = std::move(slot);
    m_pendingSlotAction = action;
}

void ScreenManager::processLoadingTransition() {
    if (!m_pendingSlot.has_value()) {
        return;
    }

    if (m_loadingTasks.isFailed()) {
        spdlog::error("ScreenManager::processLoadingTransition() task failed: {} — discarding pending screen",
                      m_loadingTasks.failureMessage());
        m_loadingTasks.cancelActive(::biofuel::engine::runtime::Runtime::tasks());
        m_loadingTasks.waitForActive(::biofuel::engine::runtime::Runtime::tasks());
        m_pendingSlot.reset();
        m_pendingSlotAction = typed::ScreenCommandQueue::Action::None;
        m_loadingTasks.clear(::biofuel::engine::runtime::Runtime::tasks());
        return;
    }

    m_loadingTasks.processNext(&::biofuel::engine::runtime::Runtime::tasks());

    if (!m_loadingTasks.isDone()) {
        return;
    }

    // All loading tasks complete — finalize the transition
    auto slot = std::move(*m_pendingSlot);
    const auto action = m_pendingSlotAction;
    m_pendingSlot.reset();
    m_pendingSlotAction = typed::ScreenCommandQueue::Action::None;

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
    // Discard any pending async screen transition
    m_loadingTasks.cancelActive(::biofuel::engine::runtime::Runtime::tasks());
    m_loadingTasks.waitForActive(::biofuel::engine::runtime::Runtime::tasks());
    m_loadingTasks.clear(::biofuel::engine::runtime::Runtime::tasks());
    m_pendingSlot.reset();
    m_pendingSlotAction = typed::ScreenCommandQueue::Action::None;

    while (!m_screens.empty()) {
        typed::LifecycleContext exitContext{*this, &::biofuel::engine::runtime::Runtime::services(), m_screens.back().id};
        m_screens.back().dispatch->onExit(*m_screens.back().screen, exitContext);
        m_screens.popBack();
    }
    m_commands.clear();
}

// ------------------------------------------------------------------------------
// Crossfade Transition Preloading
// ------------------------------------------------------------------------------

void ScreenManager::preloadCrossfadeShader() {
    ensureCrossfadeShader();
}

void ScreenManager::setPolicyResolvers(
    const TransitionPolicyResolver transitionResolver,
    const StackPolicyResolver stackResolver) noexcept
{
    m_transitionPolicyResolver = transitionResolver ? transitionResolver : typed::transitionPolicyForId;
    m_stackPolicyResolver = stackResolver ? stackResolver : typed::stackPolicyForId;
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
    processLoadingTransition();
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
    return m_transitionPolicyResolver(screenId);
}

typed::StackPolicyData ScreenManager::stackPolicyFor(const Screen& screen) const noexcept {
    return stackPolicyFor(screen.screenId());
}

typed::StackPolicyData ScreenManager::stackPolicyFor(const typed::ScreenId screenId) const noexcept {
    return m_stackPolicyResolver(screenId);
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

bool ScreenManager::blocksUnderlyingUpdates() const noexcept {
    if (m_screens.size() < 2) {
        return false;
    }
    return !stackPolicyFor(m_screens.back().id).updateBelow;
}

void ScreenManager::releaseTransitionTextures() noexcept {
    m_transitionOut.release();
    m_transitionIn.release();
}

} // namespace biofuel::engine::ui
