#pragma once

#include "engine/core/Types.hpp"
#include <functional>
#include <string>
#include <memory>

namespace biofuel::engine::app {

// ------------------------------------------------------------------------------
// Application - Main game wrapper
// Manages the Raylib window, game loop, and high-level lifecycle.
// ------------------------------------------------------------------------------
class Application {
public:
    struct Config {
        std::string title = "Biofuel Game";
        i32 width = 1280;
        i32 height = 720;
        i32 targetFps = 60;
        bool fullscreen = false;
        bool resizable = false;
        std::function<void(i32 width, i32 height, i32 targetFps)> startup;
        std::function<void()> globalInput;
    };

    explicit Application(Config config = {});
    ~Application() noexcept;

    // Non-copyable, non-movable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // Main entry point - blocks until window closes
    [[nodiscard]] i32 run();

    // Explicit lifecycle (if you want manual control instead of run())
    void init();
    void shutdown();

    [[nodiscard]] bool isRunning() const noexcept { return m_running; }
    [[nodiscard]] const Config& config() const noexcept { return m_config; }

private:
    void processInput();
    void update(f32 deltaTime);
    void render();

    const Config m_config;
    bool m_initialized = false;
    bool m_running = false;

public:
    // Exposed for hot-path inlining — the anonymous-namespace accumulator
    // helper in App.cpp references these at compile-time.
    static constexpr f64 kFixedDt = 1.0 / 60.0;
    static constexpr f64 kMaxFrameCatchupMultiplier = 5.0;

private:

#ifdef _WIN32
    void setupWindowDragTimer();
    void killWindowDragTimer();
    static void flushDragMove();
#endif
};

} // namespace biofuel::engine::app
