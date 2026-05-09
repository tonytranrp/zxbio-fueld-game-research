#pragma once

#include "Core/Types.hpp"
#include <string>
#include <memory>

namespace biofuel {

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
    };

    explicit Application(Config config = {});
    ~Application();

    // Non-copyable, non-movable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // Main entry point - blocks until window closes
    [[nodiscard]] int run();

    // Explicit lifecycle (if you want manual control instead of run())
    void init();
    void shutdown();

    [[nodiscard]] bool isRunning() const noexcept { return m_running; }
    [[nodiscard]] const Config& config() const noexcept { return m_config; }

private:
    void processInput();
    void update(f32 deltaTime);
    void render();

    Config m_config;
    bool m_initialized = false;
    bool m_running = false;
};

} // namespace biofuel
