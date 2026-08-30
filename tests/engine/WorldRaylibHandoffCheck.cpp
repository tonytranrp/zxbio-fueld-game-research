// Manual verification, NOT an automated ctest test: isolates the ONE new
// risk Phase 2 of the World Engine Migration plan introduces beyond what
// Phase 1(a)/(b) already proved -- does raylib's own window survive being
// closed mid-process and reopened later, sandwiched around a real
// biofuel_world session? This is tested here in complete isolation from
// the real engine's typed-registry services (ShaderManager, ModelSystem,
// etc.), which all cache GPU resource handles tied to the live GL context
// and have never been exercised across a window teardown/rebuild -- if
// THIS isolated check doesn't pass cleanly, wiring the real handoff into
// MainMenuScreen would be unsafe to attempt at all.
//
// Run manually: draws a few raylib frames, closes the window, runs a World
// session (close its window when it appears), then re-opens a raylib
// window and draws a few more frames to confirm raylib itself still works.
#include "engine/world/WorldBridge.hpp"

#include <raylib.h>
#include <cstdio>

namespace {

void drawRaylibFrames(const char* label, const int frames) {
    for (int i = 0; i < frames && !WindowShouldClose(); ++i) {
        BeginDrawing();
        ClearBackground(Color{20, 20, 30, 255});
        DrawText(label, 20, 20, 20, RAYWHITE);
        EndDrawing();
    }
}

} // namespace

int main() {
    std::printf("--- phase A: raylib window before World handoff ---\n");
    InitWindow(640, 360, "raylib before World");
    if (!IsWindowReady()) {
        std::printf("FAILED: raylib window did not open\n");
        return 1;
    }
    drawRaylibFrames("raylib window A -- closing shortly", 60);
    CloseWindow();
    std::printf("--- phase A done: raylib window closed cleanly ---\n");

    std::printf("--- phase B: World session (close its window when it appears) ---\n");
    using namespace biofuel::engine::world;
    const WorldSessionOutcome outcome = runWorldSession(WorldSessionInput{.saveSlot = -1});
    if (outcome.reason != WorldSessionExitReason::ReturnedToMenu) {
        std::printf("FAILED: World session did not return ReturnedToMenu\n");
        return 1;
    }
    std::printf("--- phase B done: World session returned cleanly ---\n");

    std::printf("--- phase C: raylib window after World handoff ---\n");
    InitWindow(640, 360, "raylib after World");
    if (!IsWindowReady()) {
        std::printf("FAILED: raylib window did not re-open after the World session\n");
        return 1;
    }
    drawRaylibFrames("raylib window C -- post-World, closing shortly", 60);
    CloseWindow();
    std::printf("--- phase C done: raylib window re-opened and closed cleanly ---\n");

    std::printf("PASSED: raylib survives CloseWindow -> World session -> InitWindow\n");
    return 0;
}
