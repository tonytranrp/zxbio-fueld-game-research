#include "Core/App.hpp"

// ------------------------------------------------------------------------------
// Entry Point
// ------------------------------------------------------------------------------
int main()
{
    biofuel::Application::Config config{
        .title = "Biofuel Game - Fuel Farm",
        .width = 1280,
        .height = 720,
        .targetFps = 60,
        .fullscreen = false,
        .resizable = true,
    };

    biofuel::Application app(config);
    return app.run();
}
