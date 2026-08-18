#pragma once

#include "engine/tasks/LoadingTask.hpp"
#include "engine/core/Types.hpp"
#include <string_view>

namespace biofuel::engine::app {

struct WindowLifecycleConfig {
    std::string_view title = "Biofuel Game";
    i32 width = 1280;
    i32 height = 720;
    bool fullscreen = false;
    bool resizable = false;
    bool vsync = false;
};

struct StartupLifecycleConfig {
    i32 width = 1280;
    i32 height = 720;
    i32 targetFps = 60;
};

class AppLifecycle {
public:
    static void openWindow(WindowLifecycleConfig config);
    static void prepareLoadingPrelude();
    static void addStartupTasks(LoadingTaskQueue& tasks, StartupLifecycleConfig config);
    static void shutdownCoreServices() noexcept;
};

} // namespace biofuel::engine::app
