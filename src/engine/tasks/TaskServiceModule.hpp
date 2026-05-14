#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"
#include "engine/tasks/TaskManager.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(TaskService);
BIOFUEL_STATIC_SERVICE(TaskService, "service.tasks", ::biofuel::engine::tasks::TaskManager);
BIOFUEL_SERVICE_MODULE(TaskServiceModule, TaskService)
} // namespace biofuel::engine::runtime::typed
