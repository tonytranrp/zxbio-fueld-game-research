#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"
#include "engine/models/ModelSystem.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(ModelService);
BIOFUEL_RUNTIME_SERVICE(ModelService, "service.model", ::biofuel::engine::models::ModelSystem,
    ::biofuel::engine::models::ModelSystem::instance());
BIOFUEL_SERVICE_MODULE(ModelServiceModule, ModelService)
} // namespace biofuel::engine::runtime::typed

