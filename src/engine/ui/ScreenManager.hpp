#pragma once

#include "engine/core/Types.hpp"
#include "engine/core/LoadingTask.hpp"
#include "engine/events/screen/ScreenEvents.hpp"
#include "engine/graphics/RenderSurface.hpp"
#include "engine/ui/typed/ScreenCommandQueue.hpp"
#include "engine/ui/typed/ScreenCatalog.hpp"
#include "engine/ui/typed/TypedScreenStack.hpp"
#include <raylib.h>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace biofuel::engine::ui {

class Screen;

// ------------------------------------------------------------------------------
// ScreenManager - Singleton screen stack
// Owns all screens, delegates per-frame lifecycle, manages fade transitions.
// ------------------------------------------------------------------------------
class ScreenManager {
public:
    using TransitionPolicyResolver = typed::TransitionPolicyData (*)(typed::ScreenId) noexcept;
    using StackPolicyResolver = typed::StackPolicyData (*)(typed::ScreenId) noexcept;

    [[nodiscard]] static ScreenManager& instance() noexcept;

    void init();
    void shutdown();

    // Stack operations
    void push(std::unique_ptr<Screen> screen);
    template<typename TScreen, typename... TArgs>
    void push(TArgs&&... args) {
        push(typed::ScreenSlot::typed<std::remove_cvref_t<TScreen>>(
            std::make_unique<std::remove_cvref_t<TScreen>>(std::forward<TArgs>(args)...),
            transitionPolicyFor(typed::screenIdOf<TScreen>)));
    }
    void push(typed::ScreenSlot slot);
    void pop();
    void replace(std::unique_ptr<Screen> screen);
    template<typename TScreen, typename... TArgs>
    void replace(TArgs&&... args) {
        replace(typed::ScreenSlot::typed<std::remove_cvref_t<TScreen>>(
            std::make_unique<std::remove_cvref_t<TScreen>>(std::forward<TArgs>(args)...),
            transitionPolicyFor(typed::screenIdOf<TScreen>)));
    }
    void replace(typed::ScreenSlot slot);
    void clear();

    // Deferred operations — safe to call from onUpdate() during update loop
    void queuePush(std::unique_ptr<Screen> screen);
    template<typename TScreen, typename... TArgs>
    void queuePush(TArgs&&... args) {
        queuePush(typed::ScreenSlot::typed<std::remove_cvref_t<TScreen>>(
            std::make_unique<std::remove_cvref_t<TScreen>>(std::forward<TArgs>(args)...),
            transitionPolicyFor(typed::screenIdOf<TScreen>)));
    }
    void queuePush(typed::ScreenSlot slot);
    void queueReplace(std::unique_ptr<Screen> screen);
    template<typename TScreen, typename... TArgs>
    void queueReplace(TArgs&&... args) {
        queueReplace(typed::ScreenSlot::typed<std::remove_cvref_t<TScreen>>(
            std::make_unique<std::remove_cvref_t<TScreen>>(std::forward<TArgs>(args)...),
            transitionPolicyFor(typed::screenIdOf<TScreen>)));
    }
    void queueReplace(typed::ScreenSlot slot);
    void queuePop();

    // Per-frame delegation
    void update(f32 dt);
    void render();
    void handleInput();

    // Queries
    [[nodiscard]] Screen* currentScreen() const noexcept;
    [[nodiscard]] Screen* screenBelowTop() const noexcept;
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] size_t stackSize() const noexcept;
    [[nodiscard]] bool isTransitioning() const noexcept;
    [[nodiscard]] bool hasPendingScreenTransition() const noexcept { return m_pendingSlot.has_value(); }
    [[nodiscard]] bool blocksUnderlyingUpdates() const noexcept;
    void captureScreen(Screen* screen, ::biofuel::engine::graphics::RenderSurface& target);
    [[nodiscard]] bool isLayerEnabled(typed::ScreenId screenId, std::string_view layerName) const noexcept;

    // Quit signal — screens call requestQuit() instead of closing the window directly
    void requestQuit() noexcept { m_quitRequested = true; }
    [[nodiscard]] bool quitRequested() const noexcept { return m_quitRequested; }

    // Crossfade transition preloading for startup tasks
    void preloadCrossfadeShader();
    void setPolicyResolvers(TransitionPolicyResolver transitionResolver, StackPolicyResolver stackResolver) noexcept;

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;
    ScreenManager(ScreenManager&&) = delete;
    ScreenManager& operator=(ScreenManager&&) = delete;

private:
    ScreenManager() = default;
    ~ScreenManager() = default;

    typed::TypedScreenStack<typed::AppScreenRegistry> m_screens;
    typed::ScreenCommandQueue m_commands;
    TransitionPolicyResolver m_transitionPolicyResolver = typed::transitionPolicyForId;
    StackPolicyResolver m_stackPolicyResolver = typed::stackPolicyForId;
    bool m_quitRequested = false;
    bool m_overrideSinksConnected = false;

    // Async screen transition support
    LoadingTaskQueue m_loadingTasks;
    std::optional<typed::ScreenSlot> m_pendingSlot;
    typed::ScreenCommandQueue::Action m_pendingSlotAction = typed::ScreenCommandQueue::Action::None;

    void processPendingActions();
    void processLoadingTransition();
    void releaseTransitionTextures() noexcept;

    // Crossfade transition rendering
    Shader m_crossfadeShader{};
    ::biofuel::engine::graphics::RenderSurface m_transitionOut;
    ::biofuel::engine::graphics::RenderSurface m_transitionIn;
    i32 m_crossfadeProgressLoc = -1;
    i32 m_crossfadeTexInLoc = -1;

    void ensureCrossfadeShader();
    [[nodiscard]] bool ensureTransitionTextures(i32 width, i32 height);
    void renderSlotToBackbuffer(typed::ScreenSlot& slot, i32 width, i32 height);
    void renderCrossfade(typed::ScreenSlot& outgoing, typed::ScreenSlot& incoming);
    void syncBridgeTransition(typed::ScreenSlot& slot) const noexcept;
    [[nodiscard]] typed::TransitionPolicyData transitionPolicyFor(const Screen& screen) const noexcept;
    [[nodiscard]] typed::TransitionPolicyData transitionPolicyFor(typed::ScreenId screenId) const noexcept;
    [[nodiscard]] typed::StackPolicyData stackPolicyFor(const Screen& screen) const noexcept;
    [[nodiscard]] typed::StackPolicyData stackPolicyFor(typed::ScreenId screenId) const noexcept;

    struct TransitionOverrideState {
        bool active = false;
        bool persistent = false;
        typed::TransitionPolicyData policy{};
    };

    struct LayerOverrideState {
        bool enabled = true;
        bool persistent = false;
    };

    struct DebugOverrideState {
        bool active = false;
        bool enabled = true;
        bool persistent = false;
    };

    struct ScreenOverrideState {
        TransitionOverrideState transition;
        std::unordered_map<std::string, LayerOverrideState, TransparentHash, std::equal_to<>> layers;
        DebugOverrideState debug;
    };

    std::array<ScreenOverrideState, typed::SCREEN_ID_COUNT> m_overrides{};

    [[nodiscard]] ScreenOverrideState& overridesFor(typed::ScreenId screenId) noexcept;
    [[nodiscard]] const ScreenOverrideState& overridesFor(typed::ScreenId screenId) const noexcept;
    void clearTransientOverrides(typed::ScreenId screenId);
    void connectOverrideSinks();
    void disconnectOverrideSinks();
    void onTransitionOverride(const ::biofuel::engine::events::screen::ScreenTransitionOverrideEvent& event);
    void onLayerOverride(const ::biofuel::engine::events::screen::ScreenLayerOverrideEvent& event);
    void onDebugOverride(const ::biofuel::engine::events::screen::ScreenDebugRenderOverrideEvent& event);
};

} // namespace biofuel::engine::ui
