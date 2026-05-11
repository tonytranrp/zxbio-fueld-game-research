#pragma once

#include "Core/Types.hpp"
#include "Utils/render/RenderSurface.hpp"
#include <raylib.h>
#include <memory>
#include <vector>

namespace biofuel::ui {

class Screen;

// ------------------------------------------------------------------------------
// ScreenManager - Singleton screen stack
// Owns all screens, delegates per-frame lifecycle, manages fade transitions.
// ------------------------------------------------------------------------------
class ScreenManager {
public:
    [[nodiscard]] static ScreenManager& instance() noexcept;

    void init();
    void shutdown();

    // Stack operations
    void push(std::unique_ptr<Screen> screen);
    void pop();
    void replace(std::unique_ptr<Screen> screen);
    void clear();

    // Deferred operations — safe to call from onUpdate() during update loop
    void queuePush(std::unique_ptr<Screen> screen);
    void queueReplace(std::unique_ptr<Screen> screen);
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

    // Quit signal — screens call requestQuit() instead of closing the window directly
    void requestQuit() noexcept { m_quitRequested = true; }
    [[nodiscard]] bool quitRequested() const noexcept { return m_quitRequested; }

    // Crossfade transition preloading — called during LoadingScreen init tasks
    void preloadCrossfadeShader();
    void preloadTransitionTextures();

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;
    ScreenManager(ScreenManager&&) = delete;
    ScreenManager& operator=(ScreenManager&&) = delete;

private:
    ScreenManager() = default;
    ~ScreenManager() = default;

    std::vector<std::unique_ptr<Screen>> m_screens;
    bool m_quitRequested = false;

    enum class PendingAction { None, Push, Replace };
    PendingAction m_pendingAction = PendingAction::None;
    std::unique_ptr<Screen> m_pendingScreen;
    bool m_pendingPop = false;

    void processPendingActions();
    void releaseTransitionTextures() noexcept;

    // Crossfade transition rendering
    Shader m_crossfadeShader{};
    utils::render::RenderSurface m_transitionOut;
    utils::render::RenderSurface m_transitionIn;
    i32 m_crossfadeProgressLoc = -1;
    i32 m_crossfadeTexInLoc = -1;

    void ensureCrossfadeShader();
    void ensureTransitionTextures(i32 width, i32 height);
    void renderCrossfade(Screen* outgoing, Screen* incoming);
};

} // namespace biofuel::ui
