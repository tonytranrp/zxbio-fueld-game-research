#include "engine/world/WorldBridge.hpp"

#include "biofuel_world_cxx/lib.h"

namespace biofuel::engine::world {

WorldSessionOutcome runWorldSession(const WorldSessionInput& input) {
    const ::biofuel::world::SessionInput bridgeInput{.save_slot = input.saveSlot};
    const ::biofuel::world::SessionExit bridgeExit = ::biofuel::world::run_world_session(bridgeInput);
    return WorldSessionOutcome{.reason = static_cast<WorldSessionExitReason>(bridgeExit.reason)};
}

} // namespace biofuel::engine::world
