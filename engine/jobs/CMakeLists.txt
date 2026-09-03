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

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
