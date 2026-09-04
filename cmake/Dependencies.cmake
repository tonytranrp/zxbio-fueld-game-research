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
