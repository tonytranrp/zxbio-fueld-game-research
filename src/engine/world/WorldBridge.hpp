#pragma once

#include "engine/core/Types.hpp"

namespace biofuel::engine::world {

// Mirrors src/World/src/lib.rs's cxx bridge SessionExit.reason encoding --
// keep the two in sync by hand if it ever grows past three values.
enum class WorldSessionExitReason : u8 {
    ReturnedToMenu = 0,
    VulkanUnavailable = 1,
    InternalError = 2,
};

struct WorldSessionInput {
    i32 saveSlot = -1; // -1 = new game. Unread by Phase 1(a); see lib.rs.
};

struct WorldSessionOutcome {
    WorldSessionExitReason reason = WorldSessionExitReason::InternalError;
};

// Deliberately NOT a typed-registry service (see src/World/README.md and
// the World Engine Migration plan's "Target folder/file structure"
// section) -- this is one blocking call, not per-frame service access, so
// wrapping it in Runtime/ServiceModule machinery would fight that pattern's
// own premise for no benefit.
//
// Blocks for the entire gameplay session. Call exactly once per "New
// Game"/"Continue", only AFTER raylib's window has been fully closed
// (CloseWindow()) on the same thread that will make this call -- it
// internally reuses one process-lifetime winit EventLoop across every call
// (src/World/src/event_loop_cell.rs), which winit requires to be built and
// driven from one consistent OS thread for the process's whole life. Never
// call this from more than one call site or a different thread each time.
[[nodiscard]] WorldSessionOutcome runWorldSession(const WorldSessionInput& input);

} // namespace biofuel::engine::world
