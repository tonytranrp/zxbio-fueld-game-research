#include "game/app/GameApp.hpp"

#ifdef _WIN32
// GPU-selection hints (must live in the EXE, not a DLL). On a hybrid iGPU/dGPU
// laptop, leaving GPU choice to driver heuristics can route OpenGL rendering
// and presentation across two GPUs, forcing the driver to stage frames through
// system RAM -- measured elsewhere to balloon a raylib app's RAM well past its
// dGPU-only baseline. This is the standard, NVIDIA/AMD-documented way shipped
// games force the discrete GPU deterministically.
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

// ------------------------------------------------------------------------------
// Entry Point
// ------------------------------------------------------------------------------
int main()
{
    auto app = biofuel::game::app::makeApplication();
    return app.run();
}
