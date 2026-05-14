#include "game/app/GameApp.hpp"

// ------------------------------------------------------------------------------
// Entry Point
// ------------------------------------------------------------------------------
int main()
{
    auto app = biofuel::game::app::makeApplication();
    return app.run();
}
