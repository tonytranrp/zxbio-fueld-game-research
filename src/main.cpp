#include "engine/app/App.hpp"

// ------------------------------------------------------------------------------
// Entry Point
// ------------------------------------------------------------------------------
int main()
{
    biofuel::engine::app::Application::Config config{
        .title = "Biofuel Game - Fuel Farm",
        .width = 1280,
        .height = 720,
        .targetFps = 60,
        .fullscreen = false,
        .resizable = true,
    };

    biofuel::engine::app::Application app(config);
    return app.run();
}
