#pragma once

#include "engine/animation/AnimationManager.hpp"
#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(AnimationService);
BIOFUEL_RUNTIME_SERVICE(AnimationService, "service.animation", ::biofuel::engine::animation::AnimationManager,
    ::biofuel::engine::animation::AnimationManager::instance());
BIOFUEL_SERVICE_MODULE(AnimationServiceModule, AnimationService)
} // namespace biofuel::engine::runtime::typed

