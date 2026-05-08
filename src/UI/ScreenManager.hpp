#pragma once

#include "Core/Types.hpp"
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
    [[nodiscard]] static ScreenManager& instance();

    void init();
    void shutdown();

    // Stack operations
    void push(std::unique_ptr<Screen> screen);
    void pop();
    void replace(std::unique_ptr<Screen> screen);
    void clear();

    // Per-frame delegation
    void update(f32 dt);
    void render();
    void handleInput();

    // Queries
    [[nodiscard]] Screen* currentScreen() const noexcept;
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] size_t stackSize() const noexcept;
    [[nodiscard]] bool isTransitioning() const noexcept;

    // Quit signal — screens call requestQuit() instead of closing the window directly
    void requestQuit() noexcept { m_quitRequested = true; }
    [[nodiscard]] bool quitRequested() const noexcept { return m_quitRequested; }

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;
    ScreenManager(ScreenManager&&) = delete;
    ScreenManager& operator=(ScreenManager&&) = delete;

private:
    ScreenManager() = default;
    ~ScreenManager() = default;

    std::vector<std::unique_ptr<Screen>> m_screens;
    bool m_quitRequested = false;
};

} // namespace biofuel::ui
