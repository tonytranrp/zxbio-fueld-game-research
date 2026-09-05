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

# Deliberate decision (engine-hardening brief Group J task 21), not the CMake default: Release
# keeps full PDBs. MSVC's default Release flags carry no /Zi, so a Release crash would hand the
# crash handler a symbol-less stack -- exactly the situation a crash handler exists to prevent.
# /Zi + /DEBUG cost nothing at runtime (debug info lives in the .pdb beside the .exe, not in the
# image); the only cost is link time and disk. Debug already gets /Zi from CMake's defaults.
if(MSVC)
  string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Zi")
  string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
  string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")

  # RelWithDebInfo preset (chunk-generation profiling pass, 2026-09-05): added so launching via
  # Visual Studio's own F5/debugger has a fast option, instead of only Debug (which links the
  # debug CRT, defaulting _ITERATOR_DEBUG_LEVEL=2 -- MSVC's own documented behavior, confirmed
  # against this project's own already-measured cost of that specific flag: 16 concurrent mesh
  # jobs ran ~80x slower than one, see mesh_extractor.cpp's NeighborCache comment). CMake's own
  # RelWithDebInfo default (/MD /Zi /O2 /Ob1 /DNDEBUG) already lands on the release CRT + NDEBUG
  # side of the IDL default rule, so this does NOT reintroduce the Debug-CRT slowdown -- verified
  # against MSVC's own documented IDL-default table, not assumed. CMake's default DOES leave one
  # real gap versus Release, though: /Ob1 (inlines only explicitly-marked-inline functions) instead
  # of /Ob2 (the compiler's own inlining judgment, what Release gets via /O2 alone) -- real-world
  # precedent (OBS Studio's own RelWithDebInfo hardening) found this a measurable, not
  # theoretical, gap. Since this preset exists specifically so a VS user gets fast chunk generation
  # without hunting for a config-dropdown change, bump it to /Ob2 deliberately -- the honest cost
  # is a debugger step occasionally jumping through an inlined call, acceptable for a preset whose
  # whole purpose is speed.
  string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " /Ob2")
  string(APPEND CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO " /OPT:REF /OPT:ICF")
  string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO " /OPT:REF /OPT:ICF")
endif()

include(CTest)

# ON: the full Windows build (DiligentEngine, GLFW, Tracy, the render modules, and voxel_app).
# OFF: only the platform-independent core -- engine/{core,ecs,jobs}, all of world/, and their
# tests -- which is what the Linux CI sanitizer/clang-tidy jobs build (TSan does not exist on
# this Windows dev machine by any realistic path; see CLAUDE.md), skipping the heavyweight GPU
# dependency fetches entirely.
option(VOXEL_BUILD_RENDERER "Build the GPU renderer, windowing, input, and voxel_app" ON)

# Micro-benchmark harnesses (engine-hardening brief Group N). OFF by default: benchmark numbers
# from a Debug build are meaningless, so these are built deliberately, in a Release configure
# (`cmake --preset windows-release -DVOXEL_BUILD_BENCHMARKS=ON`), not as part of every dev build.
option(VOXEL_BUILD_BENCHMARKS "Build Google Benchmark harnesses (use a Release configure)" OFF)

include(cmake/CompilerWarnings.cmake)
include(cmake/Dependencies.cmake)

# Static analysis wired into the build itself (tooling-testing-and-ci.md §2), not a manual step.
# Deliberately set AFTER Dependencies.cmake so third-party targets declared above are never
# analyzed; test directories clear it locally (Catch2's assertion macros are structurally noisy
# under several bugprone checks, and tests are not product code).
option(VOXEL_CLANG_TIDY "Compile every project TU under clang-tidy (the CI static-analysis job)" OFF)
if(VOXEL_CLANG_TIDY)
  find_program(VOXEL_CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY "${VOXEL_CLANG_TIDY_EXE}")
  if(MSVC)
    # tidy's cl driver mode loses /EHsc from the reproduced command line on this setup, then
    # hard-errors on every `throw` -- restate it so local MSVC runs match the Linux CI job.
    list(APPEND CMAKE_CXX_CLANG_TIDY "--extra-arg=/EHsc")
  endif()
endif()

add_subdirectory(engine)
add_subdirectory(world)
if(VOXEL_BUILD_RENDERER)
  add_subdirectory(render)
  add_subdirectory(app)
endif()
add_subdirectory(tools)
if(VOXEL_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
