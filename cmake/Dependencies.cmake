# Every third-party dependency lives here, pinned to a specific tag/commit so the whole
# dependency graph is visible and auditable in one place. Never track a branch unpinned.
# Every tag below was verified live against `git ls-remote --tags` on 2026-09-02, not guessed
# from memory.

# CMake >= 4.0 hard-errors on a subproject whose own cmake_minimum_required predates 3.5
# (that compatibility range was removed outright, not just deprecated). Several dependencies
# below are older projects that haven't bumped their own floor — this is CMake's documented
# escape hatch, not a version hack.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# --- CPM.cmake bootstrap -------------------------------------------------------------------
set(CPM_DOWNLOAD_VERSION 0.42.3)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
  message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}")
  file(DOWNLOAD
       "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
       ${CPM_DOWNLOAD_LOCATION}
  )
endif()
include(${CPM_DOWNLOAD_LOCATION})

# Our own small, independent dependencies are declared BEFORE DiligentEngine deliberately.
# CMake's FetchContent gives the *first* Declare call for a given name priority across the
# whole build (CMake >= 3.24) — DiligentEngine's own build (at least as of the commit pinned
# below) turns out to also pull in EnTT internally, and when DiligentEngine was declared
# first, its internal fetch claimed `_deps/entt-build` for its own EnTT source, so our own
# later, differently-tagged entt package collided on that same binary directory ("The binary
# directory ... is already used to build a source directory"). Declaring ours first instead
# means DiligentEngine's own internal fetch reuses our already-populated v3.16.0 rather than
# adding a second, conflicting one.

# --- ECS: EnTT ------------------------------------------------------------------------------
# Explicit GIT_TAG (not the "gh:owner/repo@version" shorthand) deliberately — that shorthand's
# version parsing auto-prepends "v", which double-prefixed this tag into the invalid
# "vv3.16.0" the first time. Explicit GIT_TAG is passed straight through, unmangled.
#
# UPDATE_DISCONNECTED TRUE is set on every package below, not just as a speed optimization: every
# pinned tag is immutable by definition, so FetchContent's separate git "update" step (re-checking
# a pinned ref for upstream changes on every reconfigure) has nothing real to do — and on this
# machine that step is also where the HEAD^0/GIT_EXECUTABLE bug documented in CLAUDE.md and at the
# top of the root CMakeLists.txt actually manifests. The root CMakeLists.txt's GIT_EXECUTABLE fix
# closes the bug at its real source (so this isn't strictly load-bearing anymore), but skipping a
# genuinely pointless network round-trip on every configure is worth doing regardless.
CPMAddPackage(
  NAME entt
  GITHUB_REPOSITORY skypjack/entt
  GIT_TAG v3.16.0
  UPDATE_DISCONNECTED TRUE
)

# --- Math: GLM ------------------------------------------------------------------------------
CPMAddPackage(
  NAME glm
  GITHUB_REPOSITORY g-truc/glm
  GIT_TAG 1.0.3
  UPDATE_DISCONNECTED TRUE
)

# --- Windowing: GLFW ------------------------------------------------------------------------
if(VOXEL_BUILD_RENDERER)
CPMAddPackage(
  NAME glfw
  GITHUB_REPOSITORY glfw/glfw
  GIT_TAG 3.5.1
  UPDATE_DISCONNECTED TRUE
  OPTIONS
    "GLFW_BUILD_EXAMPLES OFF"
    "GLFW_BUILD_TESTS OFF"
    "GLFW_BUILD_DOCS OFF"
)
endif()

# --- Job queue: moodycamel::ConcurrentQueue ---------------------------------------------------
# BlockingConcurrentQueue is ThreadPool's interior queue since the Group I hardening pass
# (ENGINE_HARDENING_BRIEF.md; decision log in research/engine-hardening-log.md). v1.0.5 verified
# live via git ls-remote on 2026-09-04 and smoke-tested standalone under real MSVC (jthread +
# stop_token worker shape) before adoption. Licenses: dual Simplified-BSD/BSL, plus zlib for the
# embedded lightweight semaphore. A modified copy of this queue already ships inside this binary
# via Tracy's client, so this adds a second, pinned, unmodified instance -- not a new class of
# dependency. Header-only: consumed via include dir, no CMake targets needed (its own CMakeLists
# would add install/export noise), hence DOWNLOAD_ONLY.
CPMAddPackage(
  NAME concurrentqueue
  GITHUB_REPOSITORY cameron314/concurrentqueue
  GIT_TAG v1.0.5
  UPDATE_DISCONNECTED TRUE
  DOWNLOAD_ONLY TRUE
)

# --- Testing: Catch2 -------------------------------------------------------------------------
CPMAddPackage(
  NAME Catch2
  GITHUB_REPOSITORY catchorg/Catch2
  GIT_TAG v3.9.1
  UPDATE_DISCONNECTED TRUE
)
if(Catch2_SOURCE_DIR)
  list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
endif()

# --- Terrain noise: FastNoise2 ----------------------------------------------------------------
CPMAddPackage(
  NAME FastNoise2
  GITHUB_REPOSITORY Auburn/FastNoise2
  GIT_TAG v1.1.1
  UPDATE_DISCONNECTED TRUE
  OPTIONS
    "FASTNOISE2_NOISETOOL OFF"
    "FASTNOISE2_TESTS OFF"
)

# FastNoise2's own FastSIMD dependency (dispatch/cmake/ClassSIMD.cmake) unconditionally adds
# -Wa,-muse-unaligned-vector-move whenever CMake's MINGW variable is set — true for any
# MinGW-ABI-targeting toolchain, not just real GCC. Under llvm-mingw's clang this flag isn't
# recognized ("unsupported argument ... to option '-Wa,'") and fails all four SIMD dispatch
# translation units. Strip just that one flag post-hoc rather than patch the vendored file
# (which a cache refresh would silently undo) or disable feature sets (which would defeat
# FastSIMD's whole point: runtime CPU-dispatch across them). No-op under real MSVC (MINGW is
# unset there), which is why this build now uses real MSVC in the first place — see below.
if(MINGW AND TARGET FastSIMD_FastNoise)
  get_target_property(_fastsimd_opts FastSIMD_FastNoise COMPILE_OPTIONS)
  if(_fastsimd_opts)
    list(REMOVE_ITEM _fastsimd_opts "-Wa,-muse-unaligned-vector-move")
    set_target_properties(FastSIMD_FastNoise PROPERTIES COMPILE_OPTIONS "${_fastsimd_opts}")
  endif()
  unset(_fastsimd_opts)
endif()

# --- Benchmarking: Google Benchmark + comparison candidates (benchmark builds only) -----------
if(VOXEL_BUILD_BENCHMARKS)
# v1.9.5 and v4.9.2 verified live against `git ls-remote --tags` on 2026-09-04, and both
# smoke-tested standalone under this machine's real MSVC before being wired in here
# (ENGINE_HARDENING_BRIEF.md Group G task 3). Apache-2.0 / MIT respectively, read from the
# fetched LICENSE files, not assumed.
CPMAddPackage(
  NAME benchmark
  GITHUB_REPOSITORY google/benchmark
  GIT_TAG v1.9.5
  UPDATE_DISCONNECTED TRUE
  OPTIONS
    "BENCHMARK_ENABLE_TESTING OFF"
    "BENCHMARK_ENABLE_GTEST_TESTS OFF"
    "BENCHMARK_ENABLE_INSTALL OFF"
)
# unordered_dense is fetched for the Group H task-12 comparison harness (std::unordered_map vs
# ankerl::unordered_dense::{map,segmented_map} on the real ChunkCoord workload). It is
# deliberately NOT linked into any engine module yet -- adopting it inside ChunkStore is Group H
# task 7's decision, taken on this harness's numbers, and the ChunkMap alias makes that a
# one-line change when it happens.
CPMAddPackage(
  NAME unordered_dense
  GITHUB_REPOSITORY martinus/unordered_dense
  GIT_TAG v4.9.2
  UPDATE_DISCONNECTED TRUE
)
endif()

# --- Coordinate containers: Boost.Unordered ---------------------------------------------------
# boost::unordered_flat_map/_flat_set back the CoordMap/CoordSet aliases in
# world/chunk/coord_containers.hpp (ENGINE_HARDENING_BRIEF.md Group H). The pick was made on this
# machine's own MSVC Release benchmark at the realistic 558-chunk scale -- no independent
# MSVC-run numbers exist anywhere for these containers (Subagent 1's finding), so
# benchmarks/bench_chunk_map.cpp decided it: boost_flat beat std::unordered_map AND
# ankerl::unordered_dense on every workload (build+teardown 17.3us vs 38.0/30.3us; find-hit
# 3.0us vs 3.7/6.2us), and unlike unordered_dense its iterators are custom (not std::vector's),
# so MSVC _ITERATOR_DEBUG_LEVEL=2 checked-iterator locking -- the class of the measured Group D
# Debug collapse -- cannot attach to them. Fetched the official CPM way (CMake-enabled release
# archive + BOOST_INCLUDE_LIBRARIES, header-only usage, nothing else of Boost is built).
# boost-1.92.0 verified live against the GitHub release listing on 2026-09-04 (1.91's asset was
# re-tagged 1.91.0-1; 1.92.0 is current stable).
CPMAddPackage(
  NAME Boost
  VERSION 1.92.0
  URL https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.xz
  OPTIONS "BOOST_ENABLE_CMAKE ON" "BOOST_INCLUDE_LIBRARIES unordered"
)

# --- Profiling: Tracy client (renderer builds only) -------------------------------------------
if(VOXEL_BUILD_RENDERER)
# v0.14.1 verified live against git ls-remote on 2026-09-03 (latest tag). TRACY_ON_DEMAND keeps
# the client dormant (no event buffering) until a Tracy server actually connects -- the right
# default for a dev app that is usually run without a profiler attached; the ~15ns/zone cost only
# exists while profiling (PHASE_1_COMPLETION_BRIEF.md §2.4).
CPMAddPackage(
  NAME tracy
  GITHUB_REPOSITORY wolfpld/tracy
  GIT_TAG v0.14.1
  UPDATE_DISCONNECTED TRUE
  OPTIONS
    "TRACY_ENABLE ON"
    "TRACY_ON_DEMAND ON"
)

# --- Rendering: DiligentEngine (Core + Tools + FX) ------------------------------------------
# Switched from the coordinated tag API256015 (2026-03-26) to this fresher master commit:
# under real VS "18"/2026 MSVC (19.50), API256015's vendored SPIRV-Tools snapshot fails with
# error C2220 (warning C5232, a C++20 rewritten-comparison-operator recursion warning in
# ThirdParty/SPIRV-Tools/source/util/small_vector.h, elevated by DiligentCore's own /WX).
# This commit's own message ("Update submodules...") plus master's 2026-08-12 "Windows 2025
# and Visual Studio 2026" CI addition are exactly the fix this predates.
CPMAddPackage(
  NAME DiligentEngine
  GITHUB_REPOSITORY DiligentGraphics/DiligentEngine
  GIT_TAG aca2285
  # The initial clone (incl. all nested submodules) succeeds; FetchContent's separate
  # git "update" step (re-checking a pinned tag for upstream changes on every configure)
  # then fails with a malformed `git rev-parse HEAD0` under this CMake version. A pinned
  # tag has no update to check for anyway, so just skip that step outright.
  UPDATE_DISCONNECTED TRUE
  OPTIONS
    "DILIGENT_BUILD_SAMPLES OFF"
    "DILIGENT_BUILD_TOOLS ON"
    "DILIGENT_BUILD_FX ON"
)
endif() # VOXEL_BUILD_RENDERER (Tracy + DiligentEngine)
