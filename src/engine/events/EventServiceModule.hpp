#pragma once

#include "engine/events/EventManager.hpp"
#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(EventService);
BIOFUEL_RUNTIME_SERVICE(EventService, "service.event", ::biofuel::engine::events::EventManager,
    ::biofuel::engine::events::EventManager::instance());
BIOFUEL_SERVICE_MODULE(EventServiceModule, EventService)
} // namespace biofuel::engine::runtime::typed
