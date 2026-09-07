# The frame report and the JSON writer. GPU-free by construction: this module is HANDED numbers
# (see README.md), so it builds and is tested in the gating no-GPU CI job alongside dev/scenario.
add_library(dev_telemetry STATIC
    src/frame_report.cpp
    src/json.cpp
)

target_sources(dev_telemetry PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/dev/telemetry/frame_record.hpp
        include/dev/telemetry/frame_report.hpp
        include/dev/telemetry/json.hpp
)

target_link_libraries(dev_telemetry PUBLIC project_warnings)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
