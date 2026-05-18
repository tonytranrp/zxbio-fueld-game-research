#pragma once

#include "engine/world/WorldSystem.hpp"
#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(WorldService);
BIOFUEL_STATIC_SERVICE(WorldService, "service.world", ::biofuel::engine::world::WorldSystem);
BIOFUEL_SERVICE_MODULE(WorldServiceModule, WorldService)
} // namespace biofuel::engine::runtime::typed
