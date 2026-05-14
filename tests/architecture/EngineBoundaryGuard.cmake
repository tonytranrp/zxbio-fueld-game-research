if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(ENGINE_DIR "${SOURCE_DIR}/src/engine")
if(NOT EXISTS "${ENGINE_DIR}")
    message(FATAL_ERROR "Engine source directory not found: ${ENGINE_DIR}")
endif()

file(GLOB_RECURSE ENGINE_HEADERS_AND_SOURCES
    "${ENGINE_DIR}/*.h"
    "${ENGINE_DIR}/*.hpp"
    "${ENGINE_DIR}/*.cpp"
)

set(BOUNDARY_VIOLATIONS "")
foreach(FILE_PATH IN LISTS ENGINE_HEADERS_AND_SOURCES)
    file(READ "${FILE_PATH}" CONTENTS)
    if(CONTENTS MATCHES "#[ \t]*include[ \t]*[<\"](game|app)/")
        file(RELATIVE_PATH REL_FILE "${SOURCE_DIR}" "${FILE_PATH}")
        list(APPEND BOUNDARY_VIOLATIONS "${REL_FILE}")
    endif()
endforeach()

if(BOUNDARY_VIOLATIONS)
    list(JOIN BOUNDARY_VIOLATIONS "\n  " VIOLATION_TEXT)
    message(FATAL_ERROR "Engine boundary violation: src/engine files must not include game/ or app/ headers:\n  ${VIOLATION_TEXT}")
endif()

file(READ "${SOURCE_DIR}/src/CMakeLists.txt" SRC_CMAKE)
string(FIND "${SRC_CMAKE}" "target_include_directories(biofuel_engine" ENGINE_INCLUDE_BLOCK_START)
if(ENGINE_INCLUDE_BLOCK_START EQUAL -1)
    message(FATAL_ERROR "Engine boundary guard failed: biofuel_engine include directories are not declared")
endif()
string(SUBSTRING "${SRC_CMAKE}" ${ENGINE_INCLUDE_BLOCK_START} 500 ENGINE_INCLUDE_BLOCK)
if(ENGINE_INCLUDE_BLOCK MATCHES "\\$\\{CMAKE_CURRENT_SOURCE_DIR\\}")
    message(FATAL_ERROR "Engine boundary guard failed: biofuel_engine must not expose the full src/ tree as an include directory")
endif()
if(NOT ENGINE_INCLUDE_BLOCK MATCHES "GENERATED_INCLUDE_ROOT")
    message(FATAL_ERROR "Engine boundary guard failed: biofuel_engine must include through the generated engine-only include root")
endif()

message(STATUS "Engine boundary guard passed: no src/engine includes of game/ or app/.")
