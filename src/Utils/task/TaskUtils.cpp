#include "TaskUtils.hpp"

namespace biofuel::utils::task {

TaskSystem::Executor TaskSystem::s_executor;

TaskSystem::Executor& TaskSystem::getExecutor() {
    return s_executor;
}

void TaskSystem::waitForAll() {
    getExecutor().wait_for_all();
}

} // namespace biofuel::utils::task
