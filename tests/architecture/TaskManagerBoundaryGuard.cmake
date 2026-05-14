if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

function(read_required relative_path out_var)
    set(full_path "${SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${full_path}")
        message(FATAL_ERROR "Task boundary guard missing required file: ${relative_path}")
    endif()
    file(READ "${full_path}" contents)
    set(${out_var} "${contents}" PARENT_SCOPE)
endfunction()

function(require_contains label contents needle)
    string(FIND "${contents}" "${needle}" found_index)
    if(found_index EQUAL -1)
        message(FATAL_ERROR "Task boundary guard failed: ${label}")
    endif()
endfunction()

read_required("src/engine/tasks/TaskManager.hpp" TASK_MANAGER_HEADER)
read_required("src/engine/tasks/TaskManager.cpp" TASK_MANAGER_CPP)
read_required("src/engine/core/LoadingTask.hpp" LOADING_TASK_HEADER)
read_required("src/game/screens/loading/LoadingScreen.cpp" LOADING_SCREEN)
read_required("src/engine/app/AppLifecycle.cpp" APP_LIFECYCLE)
read_required("CMakeLists.txt" ROOT_CMAKE)
read_required("src/CMakeLists.txt" SRC_CMAKE)

if(TASK_MANAGER_HEADER MATCHES "taskflow")
    message(FATAL_ERROR "Task boundary guard failed: public TaskManager header must not expose Taskflow types")
endif()
if(NOT TASK_MANAGER_CPP MATCHES "taskflow/taskflow.hpp")
    message(FATAL_ERROR "Task boundary guard failed: TaskManager implementation must own Taskflow integration")
endif()
require_contains(
    "TaskManager must expose task-specific cancellation without exposing Taskflow"
    "${TASK_MANAGER_HEADER}"
    "void cancel(TaskId id) noexcept;")
require_contains(
    "TaskManager records must own per-task stop sources"
    "${TASK_MANAGER_CPP}"
    "std::shared_ptr<std::stop_source> stopSource;")
require_contains(
    "TaskManager must create a fresh stop source for each scheduled task"
    "${TASK_MANAGER_CPP}"
    "auto stopSource = std::make_shared<std::stop_source>();")
require_contains(
    "TaskManager worker lambdas must capture the per-task stop token"
    "${TASK_MANAGER_CPP}"
    "auto token = stopSource->get_token();")
if(TASK_MANAGER_CPP MATCHES "std::stop_source stopSource;")
    message(FATAL_ERROR "Task boundary guard failed: TaskManager must not use a manager-wide stop source")
endif()
if(NOT ROOT_CMAKE MATCHES "taskflow/taskflow")
    message(FATAL_ERROR "Task boundary guard failed: root CMake must fetch Taskflow explicitly")
endif()

string(REGEX MATCH "target_link_libraries\\(biofuel_engine[^\\)]*\\)" ENGINE_LINK_BLOCK "${SRC_CMAKE}")
if(ENGINE_LINK_BLOCK STREQUAL "")
    message(FATAL_ERROR "Task boundary guard failed: biofuel_engine link block is missing")
endif()
string(FIND "${ENGINE_LINK_BLOCK}" "PRIVATE" PRIVATE_SECTION_INDEX)
string(FIND "${ENGINE_LINK_BLOCK}" "taskflow" TASKFLOW_LINK_INDEX)
if(PRIVATE_SECTION_INDEX LESS 0 OR TASKFLOW_LINK_INDEX LESS 0)
    message(FATAL_ERROR "Task boundary guard failed: biofuel_engine must link Taskflow privately")
endif()
if(TASKFLOW_LINK_INDEX LESS PRIVATE_SECTION_INDEX)
    message(FATAL_ERROR "Task boundary guard failed: Taskflow must stay a PRIVATE biofuel_engine implementation dependency")
endif()
if(NOT LOADING_TASK_HEADER MATCHES "runAsync" OR NOT LOADING_SCREEN MATCHES "Runtime::tasks")
    message(FATAL_ERROR "Task boundary guard failed: loading queue must integrate async tasks through TaskManager")
endif()
require_contains(
    "LoadingTaskQueue clear must be able to cancel only its active async task"
    "${LOADING_TASK_HEADER}"
    "void clear(::biofuel::engine::tasks::TaskManager& taskManager) noexcept")
if(LOADING_TASK_HEADER MATCHES "void clear\\(\\)")
    message(FATAL_ERROR "Task boundary guard failed: LoadingTaskQueue must not expose a no-arg clear that can drop an active async task")
endif()
require_contains(
    "LoadingTaskQueue reset implementation must stay private"
    "${LOADING_TASK_HEADER}"
    "private:
    void resetState() noexcept")
require_contains(
    "LoadingTaskQueue active async cancellation must use TaskManager::cancel instead of cancelAll"
    "${LOADING_TASK_HEADER}"
    "taskManager.cancel(*m_activeAsyncTask);")
require_contains(
    "LoadingScreen rebuild must cancel stale queue async work with the runtime TaskManager"
    "${LOADING_SCREEN}"
    "m_tasks.clear(::biofuel::engine::runtime::Runtime::tasks());")
if(APP_LIFECYCLE MATCHES "LoadingTask::async[^\\r\\n]*(Load|Unload|Draw)")
    message(FATAL_ERROR "Task boundary guard failed: async startup tasks must not call Raylib resource/render APIs")
endif()

message(STATUS "Task boundary guard passed: Taskflow is wrapped and async loading has a main-thread boundary.")
