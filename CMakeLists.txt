cmake_minimum_required(VERSION 3.20)
project(BiofuelGame VERSION 0.1.0 LANGUAGES CXX)
include(CTest)

# ------------------------------------------------------------------------------
# C++ Standard
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Rust's MSVC toolchain links the dynamic release CRT even for debug Rust
    # profiles. Use the same CRT from C++ targets to make static Rust libraries
    # link cleanly in all configurations.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "MSVC runtime library" FORCE)
endif()

# ------------------------------------------------------------------------------
# Build Output Directories
# ------------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ------------------------------------------------------------------------------
# Compiler Optimizations (Release only)
# ------------------------------------------------------------------------------
if(MSVC)
    add_compile_options($<$<CONFIG:Release>:/GL>)
    add_link_options($<$<CONFIG:Release>:/LTCG>)
    add_link_options($<$<CONFIG:Release>:/OPT:REF>)
    add_link_options($<$<CONFIG:Release>:/OPT:ICF>)
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_options($<$<CONFIG:Release>:/Ob2>)
    add_compile_options($<$<CONFIG:Release>:/Gw>)
    # Compile the .cpp files within one project in parallel (MSBuild only
    # parallelizes across projects on its own). No behavior change, all
    # configs -- this is the standard MSVC multi-TU compile flag.
    add_compile_options(/MP)
endif()

# ------------------------------------------------------------------------------
# CPM.cmake - CMake Package Manager
# ------------------------------------------------------------------------------
set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}...")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
        EXPECTED_HASH SHA256=c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d
        STATUS CPM_DOWNLOAD_STATUS
    )
    list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_STATUS_CODE)
    if(NOT CPM_DOWNLOAD_STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download CPM.cmake")
    endif()
endif()

include(${CPM_DOWNLOAD_LOCATION})

# ------------------------------------------------------------------------------
# Fetch Dependencies
# ------------------------------------------------------------------------------

# Raylib - 2D/3D game library (shared library for smaller EXE)
set(BUILD_SHARED_LIBS ON)
CPMAddPackage(
    NAME raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
    OPTIONS
        "BUILD_EXAMPLES OFF"
        "BUILD_GAMES OFF"
)
set(BUILD_SHARED_LIBS OFF)

# entt - used here only for entt::dispatcher (the typed signal/event-bus part);
# the ECS/registry half is not used in this codebase. (header-only)
CPMAddPackage(
    NAME entt
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.14.0.tar.gz
    URL_HASH SHA256=e31f6e95a30e2977a50449ef9a607a9ff40febe6f9da2a8144a183f8606f7719
    OPTIONS
        "ENTT_BUILD_TESTING OFF"
)

# nlohmann/json - typed runtime config and preset loading.
CPMAddPackage(
    NAME nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
    URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
    OPTIONS
        "JSON_BuildTests OFF"
        "JSON_Install OFF"
)

# Taskflow - header-only task/job scheduling used behind engine/tasks/TaskManager.
CPMAddPackage(
    NAME taskflow
    URL https://github.com/taskflow/taskflow/archive/refs/tags/v4.0.0.tar.gz
    URL_HASH SHA256=a9d27ad29caffc95e394976c6a362debb94194f9b3fbb7f25e34aaf54272f497
    DOWNLOAD_ONLY YES
)
if(taskflow_SOURCE_DIR AND NOT TARGET taskflow)
    add_library(taskflow INTERFACE)
    target_include_directories(taskflow SYSTEM INTERFACE "${taskflow_SOURCE_DIR}")
endif()

# spdlog - Logging (compiled-lib mode -- links spdlog::spdlog, which
# propagates SPDLOG_COMPILED_LIB; lighter headers, faster compiles of the
# handful of files that include spdlog than the header-only target would give)
CPMAddPackage(
    NAME spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
    URL_HASH SHA256=1586508029a7d0670dfcb2d97575dcdc242d3868a259742b69f100801ab4e16b
    OPTIONS
        "SPDLOG_BUILD_EXAMPLE OFF"
        "SPDLOG_INSTALL OFF"
        "SPDLOG_FMT_EXTERNAL OFF"
)

# Pipeline-c- - Compile-time validated pipeline builder for gameplay systems.
CPMAddPackage(
    NAME pipeline_c
    # Pinned to an immutable commit (not the moving main branch) so the URL_HASH
    # stays valid. Bump both the SHA in the URL and URL_HASH together to update.
    URL https://github.com/tonytranrp/Pipeline-c-/archive/d8bc48f9f4b2c52424dd08e5e82f13052b3bbeb6.tar.gz
    URL_HASH SHA256=ae5e58178e5c7c6ef7bf91fbb22051b1abd5dd3165639c4d95084e450f335dce
    OPTIONS
        "PB_BUILD_TESTS OFF"
        "PB_BUILD_EXAMPLES OFF"
        "PB_BUILD_BENCHMARKS OFF"
        "PB_ENABLE_WARNINGS OFF"
        "PB_WARNINGS_AS_ERRORS OFF"
        "PB_ENABLE_UNITY OFF"
)

# Corrosion - Cargo/Rust integration for the Rapier physics bridge.
CPMAddPackage(
    NAME Corrosion
    URL https://github.com/corrosion-rs/corrosion/archive/refs/tags/v0.6.1.tar.gz
    URL_HASH SHA256=e9e95b1ee2bad52681f347993fb1a5af5cce458c5ce8a2636c9e476e4babf8e3
    OPTIONS
        "CORROSION_BUILD_TESTS OFF"
)

corrosion_import_crate(
    MANIFEST_PATH "${CMAKE_SOURCE_DIR}/src/engine/physics/rapier_bridge/Cargo.toml"
    LOCKED
    CRATES biofuel_rapier_bridge
    CRATE_TYPES staticlib
)

corrosion_add_cxxbridge(
    biofuel_rapier_bridge_cxx
    CRATE biofuel_rapier_bridge
    REGEN_TARGET biofuel_rapier_bridge_cxx_regen
    FILES lib.rs
)

# ------------------------------------------------------------------------------
# Source Code
# ------------------------------------------------------------------------------
add_subdirectory(src)
