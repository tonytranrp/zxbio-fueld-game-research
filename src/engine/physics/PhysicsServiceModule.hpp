#pragma once

#include "engine/physics/PhysicsSystem.hpp"
#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(PhysicsService);
BIOFUEL_STATIC_SERVICE(PhysicsService, "service.physics", ::biofuel::engine::physics::PhysicsSystem);
BIOFUEL_SERVICE_MODULE(PhysicsServiceModule, PhysicsService)
} // namespace biofuel::engine::runtime::typed
