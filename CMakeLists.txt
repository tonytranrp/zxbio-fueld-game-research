cmake_minimum_required(VERSION 3.23)
project(voxel_engine LANGUAGES CXX)

# The `git` CMake would otherwise auto-detect (via find_package(Git), which FetchContent calls
# internally) may resolve to a non-standard .cmd shim on this machine (seen at
# AppData/Roaming/npm/git.cmd, likely from a dev-tooling hook) that mangles a literal `^` in
# arguments passed through cmd.exe's batch interpreter, breaking FetchContent's internal
# `git rev-parse HEAD^0` into `HEAD0` ("fatal: ambiguous argument 'HEAD0'") for every CPM package,
# not just one. Setting GIT_EXECUTABLE here, before any FetchContent/CPM call runs, wins the race
# against that internal auto-detection (find_program keeps an already-set cache variable rather
# than re-searching) — this makes every entry point (plain `cmake`, this repo's own scripts,
# Visual Studio's/VS Code's own "Open Folder" CMake integration) work without each one needing to
# pass -DGIT_EXECUTABLE manually. See CLAUDE.md for the full story.
if(WIN32 AND NOT GIT_EXECUTABLE)
  find_program(GIT_EXECUTABLE
    NAMES git.exe
    PATHS "C:/Program Files/Git/cmd" "C:/Program Files/Git/bin"
    DOC "Git command line client (forced to the real git.exe, not a PATH shim)"
  )
endif()
find_package(Git QUIET) # falls back to normal detection on any other platform, or if the above found nothing

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

include(CTest)

include(cmake/CompilerWarnings.cmake)
include(cmake/Dependencies.cmake)

add_subdirectory(engine)
add_subdirectory(world)
add_subdirectory(render)
add_subdirectory(app)
add_subdirectory(tools)
