// Manual verification, NOT an automated ctest test: opens two real windows
// in a row, each needing a human to close it, so it can't run unattended in
// CI. This is Phase 1(a)'s actual verification gate from the World Engine
// Migration plan -- re-validates, through the REAL C++ -> cxx ->
// biofuel_world path (not the standalone Phase-0 spike, and not a `cargo
// test` run, which can't exercise this at all -- see session.rs's own
// comment on why), that the persisted-EventLoop reentrancy pattern survives
// being called from C++ more than once in one process, on the process's
// real main thread -- exactly how MainMenuScreen will call it once this is
// wired into the real New Game flow.
//
// Run manually, close each window when it appears:
//   Build/dev/bin/WorldBridgeManualCheck.exe
#include "engine/world/WorldBridge.hpp"

#include <cstdio>

namespace {

const char* reasonName(const biofuel::engine::world::WorldSessionExitReason reason) {
    using biofuel::engine::world::WorldSessionExitReason;
    switch (reason) {
    case WorldSessionExitReason::ReturnedToMenu: return "ReturnedToMenu";
    case WorldSessionExitReason::VulkanUnavailable: return "VulkanUnavailable";
    case WorldSessionExitReason::InternalError: return "InternalError";
    }
    return "Unknown";
}

} // namespace

int main() {
    using namespace biofuel::engine::world;

    for (int session = 1; session <= 2; ++session) {
        std::printf("--- session %d: close the window when it appears ---\n", session);
        const WorldSessionOutcome outcome = runWorldSession(WorldSessionInput{.saveSlot = -1});
        std::printf("--- session %d result: %s ---\n", session, reasonName(outcome.reason));

        if (outcome.reason != WorldSessionExitReason::ReturnedToMenu) {
            std::printf("FAILED: session %d did not return ReturnedToMenu\n", session);
            return 1;
        }
    }

    std::printf("PASSED: 2/2 sequential sessions returned to menu cleanly\n");
    return 0;
}
