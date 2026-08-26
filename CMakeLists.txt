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
    # RelWithDebInfo ("release-debug" preset): /Ob2 matches Release's
    # auto-inlining (CMake's RelWithDebInfo default of /Ob1 inlines only
    # explicitly-inline functions, leaving real perf on the table), and /Zo
    # (Enhanced Optimized Debugging) keeps local-variable and line
    # information usable when debugging that inlined /O2 code -- without it,
    # many locals and intermediate lines are invisible to the debugger.
    # /Zo costs essentially nothing at runtime; both flags only matter in
    # optimized builds, so there is no Debug entry.
    add_compile_options($<$<CONFIG:RelWithDebInfo>:/Ob2>)
    add_compile_options($<$<CONFIG:RelWithDebInfo>:/Zo>)
    # Compile the .cpp files within one project in parallel (MSBuild only
    # parallelizes across projects on its own). No behavior change, all
    # configs -- this is the standard MSVC multi-TU compile flag.
    add_compile_options(/MP)
endif()

# ------------------------------------------------------------------------------
# sccache - optional compiler-output cache (C/C++ via CMake, Rust via Cargo)
# ------------------------------------------------------------------------------
# Unlike CPM_SOURCE_CACHE (below, avoids re-fetching dependency source) or
# Cargo's own target-dir (which Corrosion always nests inside CMAKE_BINARY_DIR
# with no override -- so it dies with a wiped build tree either way), sccache
# keys its cache purely on compiler input content and stores it outside any
# build tree. A hit skips the actual compile/rustc invocation entirely, so
# even a full `build-ninja/` wipe replays from cache instead of recompiling.
# Purely additive: falls back to normal uncached compiles when not installed,
# so it is never a hard requirement to build this project.
find_program(SCCACHE_PROGRAM sccache)
if(SCCACHE_PROGRAM)
    message(STATUS "sccache found: ${SCCACHE_PROGRAM} (compiler output caching enabled)")
    set(CMAKE_C_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
    if(MSVC)
        # MSVC's default Debug/RelWithDebInfo /Zi writes debug info into one
        # shared, incrementally-updated .pdb per target; normally /FS (already
        # set below) serializes concurrent cl.exe writes to it via mspdbsrv.exe.
        # sccache intercepts the compiler invocation itself, which breaks that
        # coordination -- concurrent cache-miss compiles of the same target
        # (e.g. spdlog's several .cpp files) then fail with C1041 ("cannot
        # open program database ... if multiple CL.EXE write to the same .PDB
        # file, please use /FS") even though /FS is present. /Z7 embeds debug
        # info directly in each .obj instead of a shared .pdb, sidestepping
        # the problem entirely; this is sccache's own documented fix for
        # MSVC. Debugging (breakpoints, stepping) is unaffected.
        foreach(flag_var CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_RELWITHDEBINFO)
            string(REPLACE "/Zi" "/Z7" ${flag_var} "${${flag_var}}")
        endforeach()
    endif()
    # rust/.cargo/config.toml is configure-time generated (gitignored) rather
    # than committed, so a machine without sccache never inherits a hard
    # dependency on it -- Cargo would hard-fail on a rustc-wrapper that
    # doesn't exist, unlike CMake's launcher vars which degrade gracefully.
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/rust/.cargo")
    file(WRITE "${CMAKE_SOURCE_DIR}/rust/.cargo/config.toml"
        "# Generated by CMake configure (see root CMakeLists.txt) -- not authored, do not edit.\n[build]\nrustc-wrapper = \"${SCCACHE_PROGRAM}\"\n")
else()
    message(STATUS "sccache not found - compiler output caching disabled (install via 'winget install Mozilla.sccache' for much faster rebuilds after a clean/wiped build dir)")
    file(REMOVE "${CMAKE_SOURCE_DIR}/rust/.cargo/config.toml")
endif()

# ------------------------------------------------------------------------------
# Build-time feature switches (see README.md's Build section)
# ------------------------------------------------------------------------------
# biofuel_bevy_bridge pulls in a ~430-crate dependency graph (bevy_render,
# wgpu, naga, gltf...) for a single F6-hotkey tech-demo screen
# (BevyDemoScreen) that nothing else depends on. That dwarfs every other
# build-time cost in this repo combined, so it is opt-in rather than part of
# the default dev build. Pass -DBIOFUEL_ENABLE_BEVY_BRIDGE=ON to build it.
option(BIOFUEL_ENABLE_BEVY_BRIDGE "Build the embedded Bevy render bridge and its F6 tech-demo screen" OFF)

# ------------------------------------------------------------------------------
# CPM.cmake - CMake Package Manager
# ------------------------------------------------------------------------------
# Persist fetched/extracted dependency sources outside build/ so deleting the
# build tree (a clean reconfigure, or a fresh CI-style build) doesn't
# re-download and re-extract raylib/entt/json/taskflow/spdlog/Pipeline-c-/
# Corrosion every time. Override with -DCPM_SOURCE_CACHE=<path> to share one
# cache across multiple clones/worktrees.
if(NOT DEFINED CPM_SOURCE_CACHE)
    set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cpm-cache" CACHE PATH "CPM dependency source cache")
endif()
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
#
# On Windows, use raylib's own prebuilt binaries (vendored under third_party/,
# see PROVENANCE.md there) instead of CPM-building from source: raylib's own
# CMake configure (compiler capability probes, GLFW backend selection) plus
# compiling ~15-20 .c files is a real, repeatable chunk of every from-scratch
# build. A prebuilt DLL + import lib needs none of that -- just an IMPORTED
# target. Other platforms keep building from source via CPM (no equivalent
# prebuilt-binary channel to vendor safely).
if(WIN32)
    set(RAYLIB_VENDOR_DIR "${CMAKE_SOURCE_DIR}/third_party/raylib-5.5-win64-msvc16")
    if(NOT EXISTS "${RAYLIB_VENDOR_DIR}/lib/raylib.dll")
        message(FATAL_ERROR "Vendored raylib not found at ${RAYLIB_VENDOR_DIR} -- see third_party/raylib-5.5-win64-msvc16/PROVENANCE.md")
    endif()
    add_library(raylib SHARED IMPORTED GLOBAL)
    set_target_properties(raylib PROPERTIES
        IMPORTED_LOCATION "${RAYLIB_VENDOR_DIR}/lib/raylib.dll"
        IMPORTED_IMPLIB "${RAYLIB_VENDOR_DIR}/lib/raylibdll.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_VENDOR_DIR}/include"
        # Matches raylib.h's own dllimport/dllexport switch -- required for a
        # consumer linking against the DLL build rather than building it.
        INTERFACE_COMPILE_DEFINITIONS "USE_LIBTYPE_SHARED"
    )
else()
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
endif()

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
#
# This is the project owner's own library (github.com/tonytranrp/Pipeline-c-),
# intended to be customized in-tree for this game specifically, so it is
# vendored as source under third_party/pipeline-c/ (checked out at the same
# commit the old CPM pin used) rather than fetched from GitHub on every
# from-scratch configure. Edit it directly there; it is no longer an external
# dependency.
set(PB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PB_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(PB_ENABLE_WARNINGS OFF CACHE BOOL "" FORCE)
set(PB_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(PB_ENABLE_UNITY ON CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/pipeline-c" "${CMAKE_BINARY_DIR}/pipeline-c-build")

# Corrosion - Cargo/Rust integration for the Rapier physics bridge and the
# embedded Bevy renderer. Both crates are members of the rust/ Cargo workspace.
CPMAddPackage(
    NAME Corrosion
    URL https://github.com/corrosion-rs/corrosion/archive/refs/tags/v0.6.1.tar.gz
    URL_HASH SHA256=e9e95b1ee2bad52681f347993fb1a5af5cce458c5ce8a2636c9e476e4babf8e3
    OPTIONS
        "CORROSION_BUILD_TESTS OFF"
)

corrosion_import_crate(
    MANIFEST_PATH "${CMAKE_SOURCE_DIR}/rust/rapier_bridge/Cargo.toml"
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

# biofuel_bevy_bridge is a cdylib, not a second staticlib: two separate
# rustc-produced staticlibs can't be linked into one C++ binary (each vendors
# its own copy of std/cxxbridge symbols). A cdylib is a different, non-
# conflicting linking mechanism, and Windows needs its .dll copied next to the
# exe post-build (see src/CMakeLists.txt).
#
# Gated behind BIOFUEL_ENABLE_BEVY_BRIDGE (see the option() above) -- this
# crate alone is a ~430-member dependency graph, by far the largest single
# build-time cost in the repo, for a debug-only tech-demo screen.
if(BIOFUEL_ENABLE_BEVY_BRIDGE)
    corrosion_import_crate(
        MANIFEST_PATH "${CMAKE_SOURCE_DIR}/rust/bevy_bridge/Cargo.toml"
        LOCKED
        CRATES biofuel_bevy_bridge
        CRATE_TYPES cdylib
    )

    corrosion_add_cxxbridge(
        biofuel_bevy_bridge_cxx
        CRATE biofuel_bevy_bridge
        REGEN_TARGET biofuel_bevy_bridge_cxx_regen
        FILES lib.rs
    )
endif()

# ------------------------------------------------------------------------------
# Source Code
# ------------------------------------------------------------------------------
add_subdirectory(src)
