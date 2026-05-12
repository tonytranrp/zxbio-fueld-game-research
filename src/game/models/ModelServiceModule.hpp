#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"
#include "game/models/ModelSystem.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(ModelService);
BIOFUEL_RUNTIME_SERVICE(ModelService, "service.model", ::biofuel::game::models::ModelSystem,
    ::biofuel::game::models::ModelSystem::instance());
BIOFUEL_SERVICE_MODULE(ModelServiceModule, ModelService)
} // namespace biofuel::engine::runtime::typed

