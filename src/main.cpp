#include "Core/App.hpp"

// ------------------------------------------------------------------------------
// Entry Point
// ------------------------------------------------------------------------------
int main()
{
    biofuel::Application::Config config;
    config.title = "Biofuel Game - Fuel Farm";
    config.width = 1280;
    config.height = 720;
    config.targetFps = 60;
    config.fullscreen = false;
    config.resizable = true;

    biofuel::Application app(config);
    return app.run();
}
