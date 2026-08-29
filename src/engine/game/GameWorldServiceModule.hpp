#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::gameworld {
class GameWorldService; // forward-declare only -- keeps the cxx-bridge header out of Runtime.hpp consumers
}

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(GameWorldRuntimeService);
BIOFUEL_SERVICE_SPEC(GameWorldRuntimeService, "service.game_world");

template<> struct ServiceModule<GameWorldRuntimeService> {
    using Service = GameWorldRuntimeService;
    using Backend = ::biofuel::engine::gameworld::GameWorldService;
    static Backend& get();
};
BIOFUEL_SERVICE_MODULE(GameWorldServiceModule, GameWorldRuntimeService)
} // namespace biofuel::engine::runtime::typed
