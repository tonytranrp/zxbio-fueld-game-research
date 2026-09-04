find_package(Threads REQUIRED)

add_library(engine_jobs STATIC
    src/thread_pool.cpp
)

target_sources(engine_jobs PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES include/engine/jobs/thread_pool.hpp
)

target_link_libraries(engine_jobs PUBLIC project_warnings Threads::Threads)

# PRIVATE on purpose: the queue is an implementation detail of thread_pool.cpp -- the public
# header exposes no moodycamel types (see the Impl split), so consumers never see this include.
target_include_directories(engine_jobs SYSTEM PRIVATE "${concurrentqueue_SOURCE_DIR}")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
