# Research Report: RenderDoc + Tracy Integration for Diligent Engine (Vulkan, MSVC, Windows)

> **Historical-citation note (2026-09-04):** brief filenames cited below (PROJECT_BRIEF.md,
> PHASE_1_BRIEF.md, M1_2_BRIEF.md, PHASE_1_COMPLETION_BRIEF.md, ENGINE_HARDENING_BRIEF.md) refer
> to root-level documents deleted in the docs migration to `docs/progress.md` + `docs/goals.md`.
> They remain retrievable from git history; citations kept verbatim as primary-evidence context.

Subagent D from `PHASE_1_BRIEF.md` §9, completed 2026-09-02. Web research (Exa).

## Task 1 — DiligentEngine + RenderDoc integration

**Diligent-specific guidance: essentially none beyond a tool listing.** No dedicated integration
guide, sample, or wiki page found across DiligentEngine/DiligentCore/DiligentSamples issues,
Discussions, or the web. The one real, citable Diligent-authored acknowledgment is in DiligentCore's
own performance doc, which lists RenderDoc alongside Nsight, AMD GPU Profiler, PIX, etc. under
"Profilers," with no wiring instructions beyond a link —
[doc/PerformanceGuide.md](https://github.com/DiligentGraphics/DiligentCore/blob/master/doc/PerformanceGuide.md).
Confirms RenderDoc is a known-good/expected pairing in Diligent's own eyes, but no special glue
code is documented anywhere — use it exactly as with any other Vulkan app. Direct search of GitHub
Discussions/issues on all three Diligent repos for "renderdoc" found no other hits.

**Generic RenderDoc-with-Vulkan workflow (this is what applies):**

- **Launch vs. attach/inject** — both supported, not equally reliable. The primary, first-class
  workflow is launching the target executable *through* RenderDoc's own Capture Dialog, which
  RenderDoc's docs describe as normal —
  [capture_attach.rst](https://github.com/baldurk/renderdoc/blob/v1.45/docs/window/capture_attach.rst).
  "Inject into Process" also exists, but with a hard caution: *"The process must not have invoked
  or initialised the API to be used, as this will be too late for RenderDoc to hook and capture
  it. At best RenderDoc will not capture, at worst it may cause crashes or undefined behaviour."*
  So injection only works if attached before `vkCreateInstance`/`vkCreateDevice` — not useful for
  "attach to an already-running app to debug a bug currently in view." **Launch through RenderDoc**
  is the practical answer for a Diligent/Vulkan dev loop.
- **How Vulkan capture actually attaches**: no invasive process hooking. RenderDoc uses Vulkan's
  own layer mechanism — a capture layer registered system-wide (Windows:
  `HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\Vulkan`, done automatically by the installer), and RenderDoc
  just launches the target with an environment variable so the Vulkan loader activates that layer —
  [Vulkan Support docs](https://renderdoc.org/docs/behind_scenes/vulkan_support.html). Capture is
  orthogonal to how the window/surface was created.
- **GLFW_NO_API**: no RenderDoc-specific handling requirement found, and no reported GLFW+RenderDoc
  attach issue (the one superficially-matching hit,
  [RenderDoc issue #3376](https://github.com/baldurk/renderdoc/issues/3376), is about RenderDoc's
  *own* Qt-based replay UI failing to create its own GLFW window surface — unrelated to capturing a
  target app). Since RenderDoc hooks at the Vulkan loader/layer dispatch-chain level, it doesn't
  care whether the `VkSurfaceKHR` came from a `GLFW_NO_API` window, SDL, or raw Win32. Caveat: the
  exact rationale for why `GLFW_NO_API` itself is required wasn't re-verified this pass (only
  GLFW's Vulkan-loader/header-inclusion doc was fetched,
  [glfw/docs/vulkan.md](https://github.com/glfw/glfw/blob/e7ea71be/docs/vulkan.md)) — treat that
  specific rationale as accurate-but-not-directly-re-verified, and "RenderDoc doesn't care either
  way" as solidly confirmed via the layer-based architecture description.

## Task 2 — Tracy CMake integration for a CPM/FetchContent dependency

Confirmed directly from Tracy's current master `CMakeLists.txt` and `cmake/options.cmake` (fetched
live):

- **Real target name**: `TracyClient` (a library target, static/shared/object depending on
  `TRACY_STATIC`/LTO settings) —
  [CMakeLists.txt](https://github.com/wolfpld/tracy/blob/master/CMakeLists.txt).
- **Namespaced alias**: `add_library(Tracy::TracyClient ALIAS TracyClient)` is a literal line in
  current master.
- **CPM/FetchContent recipe** (originally landed via
  [PR #229](https://github.com/wolfpld/tracy/pull/229)):
  ```cmake
  FetchContent_Declare(tracy GIT_REPOSITORY https://github.com/wolfpld/tracy.git GIT_TAG <tag> GIT_SHALLOW TRUE)
  set(TRACY_ENABLE ON CACHE BOOL "" FORCE)   # must be set BEFORE MakeAvailable
  FetchContent_MakeAvailable(tracy)
  target_link_libraries(your_target PRIVATE TracyClient)   # or Tracy::TracyClient
  ```
  `CPMAddPackage(... OPTIONS "TRACY_ENABLE ON")` is a thin wrapper over `FetchContent`, so this
  recipe applies directly — no CPM-specific Tracy doc page was found, so this is a direct
  functional inference from confirmed FetchContent behavior, not independently verified against a
  CPM+Tracy example.
- **Does `TRACY_ENABLE` need to be a compile definition on the consuming target, or does linking
  handle it?** — **Linking handles it, precisely confirmed from source.** `cmake/options.cmake`:
  ```cmake
  macro(set_option option help value)
    option(${option} ${help} ${value})
    if(${option})
      if(${ARGC} GREATER 3)
        target_compile_definitions(${ARGV3} PUBLIC ${option})
      endif()
    endif()
  endmacro()
  ```
  and `CMakeLists.txt` calls `set_option(TRACY_ENABLE "Enable profiling" OFF TracyClient)`. When
  `TRACY_ENABLE=ON`, Tracy does `target_compile_definitions(TracyClient PUBLIC TRACY_ENABLE)` —
  because it's `PUBLIC`, any target linking `TracyClient` transitively inherits the define; **no
  need to add `TRACY_ENABLE` on the consuming target directly**. Current default is `OFF` — must
  explicitly turn it on before `FetchContent_MakeAvailable`.
  - **Known limitation** ([Issue #494](https://github.com/wolfpld/tracy/issues/494), open, last
    activity 2025-05-27): because the define is `PUBLIC` on `TracyClient` itself, *every* target
    linking it gets `TRACY_ENABLE`, no clean per-target opt-out. For a single-target engine this is
    a non-issue and actually convenient.

**`TracyVkContext` queue-family requirement** — could **not** find this explicitly documented; what's
confirmed vs. inferred:
- Confirmed from source
  ([TracyVulkan.hpp](https://github.com/wolfpld/tracy/blob/753305a7/public/tracy/TracyVulkan.hpp)):
  the `VkCtx` constructor takes `VkPhysicalDevice, VkDevice, VkQueue, VkCommandBuffer` (plus
  calibration function pointers) and records+submits a one-time command buffer on that queue to
  calibrate CPU/GPU clocks.
- Not confirmed anywhere as a hard "must be graphics-capable" restriction. By the Vulkan spec,
  timestamp queries are gated on `VkQueueFamilyProperties::timestampValidBits > 0` for that family
  — a capability graphics, compute, *and* transfer-capable families can all expose
  (driver/hardware-dependent, not limited to graphics by spec). A dedicated transfer or compute
  queue can work with `TracyVkContext`, provided its family reports nonzero `timestampValidBits`.
- Practical implication, not a documented Tracy rule: the context is tied to whichever queue's
  *submitted work* should be timed. For instrumenting chunk-rendering draw calls (graphics
  pipeline), that `VkCtx` must be built from the graphics queue — because that's where the timed
  work runs, not because Tracy forbids other queue types.
- **Flagged as reasoned-from-source, not a directly-quoted doc guarantee** — could not extract far
  enough into Tracy's long manual (`manual/tracy.md`) to find or rule out an explicit statement.

## Task 3 — RenderDoc: in-app API vs. UI "capture on launch"

**The in-application API is real, current, documented, and recommended for fine-grained control**
— [In-application API docs](https://renderdoc.org/docs/in_application_api.html), v1.7.0. Key
mechanics: never link against RenderDoc's library; at runtime check whether it's already loaded
(`GetModuleHandleA("renderdoc.dll")` on Windows — only present if launched through/with RenderDoc),
then `GetProcAddress` the single exported `RENDERDOC_GetAPI` function for a struct of function
pointers including `StartFrameCapture(NULL, NULL)` / `EndFrameCapture(NULL, NULL)`. If RenderDoc
isn't present, the check fails harmlessly and the app runs standalone. RenderDoc's own "Tips &
Tricks" page independently confirms this is sanctioned, current practice —
[tips_tricks.rst](https://renderdoc.org/docs/getting_started/tips_tricks.html) (also documents the
"Enable API validation" launch checkbox, relevant to Task 4).

**Which is more practical for a fast edit-build-inspect loop on a chunk-rendering bug — synthesis:**
- For a **visual** bug eyeballed in real time — RenderDoc's default global hotkey (captures the
  *next* frame on keypress, zero code changes) is the fastest loop: no rebuild needed to retime the
  capture.
- The **in-app API earns its keep** when the trigger moment isn't visually obvious — auto-capturing
  when a specific chunk coordinate/LOD transition is processed, gating a capture on an
  assert/log condition deep in mesh-generation code, scripting a repro. Exactly the shape a
  state-dependent chunk-rendering bug often is.
- **Recommendation**: wire both. Always launch through RenderDoc so the default hotkey is available
  for free; additionally add a thin `RENDERDOC_GetAPI`-based wrapper bound to a debug keybind or a
  conditional check in the chunk code path for cases the hotkey can't reach.

## Task 4 — Vulkan validation layers: Diligent's own toggle vs. Vulkan SDK, and interaction with RenderDoc

**Diligent does surface its own validation toggle — confirmed from real source.** In
DiligentSamples' shared app framework:
```cpp
EngineD3D11CreateInfo EngineCI;
...
#ifdef DILIGENT_DEBUG
EngineCI.SetValidationLevel(VALIDATION_LEVEL_2);
#endif
if (m_ValidationLevel >= 0)
    EngineCI.SetValidationLevel(static_cast<VALIDATION_LEVEL>(m_ValidationLevel));
```
— [SampleApp.cpp](https://github.com/DiligentGraphics/DiligentSamples/blob/master/SampleBase/src/SampleApp.cpp).
Confirms `EngineCreateInfo::SetValidationLevel(VALIDATION_LEVEL)` is a real, first-class API, Debug
builds of the official samples auto-enable the highest level, and there's a command-line override
path too. Could only fetch far enough to see the **D3D11** case block use this pattern before the
fetch was truncated — the literal Vulkan case block wasn't independently re-confirmed identical,
nor was it confirmed whether Diligent's Vulkan backend requests `VK_LAYER_KHRONOS_validation` by
name at `vkCreateInstance` versus just wiring a `VkDebugUtilsMessengerEXT` on top of an externally-
active layer. **Flagged as unconfirmed at that specific low-level mechanism.**
- Strong corroborating evidence it *is* wired through for Vulkan specifically:
  [DiligentEngine issue #353](https://github.com/DiligentGraphics/DiligentEngine/issues/353)
  (Sept 2025) shows a user getting real `VK_LAYER_KHRONOS_validation`-sourced messages
  (`VUID-vkAcquireNextImageKHR-semaphore-01779`) straight out of building
  `Tutorial00_HelloWin32`, logged through Diligent's own diagnostics as `"Diligent Engine: ERROR:
  Vulkan debug message (validation): ..."` — real, working, on by default in Debug builds of the
  official samples.
- **What Diligent's toggle does *not* reach**: granular Khronos validation layer features like
  GPU-Assisted Validation and **Synchronization Validation** are controlled by the layer's *own*
  settings (`khronos_validation.validate_sync`, `khronos_validation.gpuav_enable` — seen
  enumerated in a real config dump in
  [VVL issue #11748](https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/11748)). No
  evidence Diligent's `VALIDATION_LEVEL` enum exposes a knob for synchronization-validation
  specifically — that granularity is configured independently via the Vulkan SDK's `vkconfig` or a
  `vk_layer_settings.txt`/environment-variable override, outside Diligent entirely. **So: basic
  on/off validation via Diligent's `SetValidationLevel`; sync-validation specifically via the
  Vulkan SDK's own layer configuration.**

**RenderDoc + validation layer interaction — real, but not a fundamental blocker:**
1. RenderDoc's own Capture Dialog has a built-in "API Validation" checkbox that itself injects
   `VK_LAYER_KHRONOS_validation` via `VK_INSTANCE_LAYERS`
   ([issue #2813](https://github.com/baldurk/renderdoc/issues/2813)). A reporter found the layer
   name listed twice and correlated it with a crash; the maintainer disputes it's a RenderDoc bug,
   couldn't reproduce, notes duplicate layer entries aren't a spec violation. **Single-source,
   inconclusive** — but the practical takeaway stands regardless: don't enable validation in *both*
   RenderDoc's launcher checkbox *and* the app's own `SetValidationLevel` at once; pick one
   (Diligent's is more convenient since it routes through the app's own log sink).
2. A real, currently-open upstream bug:
   [KhronosGroup/Vulkan-ValidationLayers #11748](https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/11748),
   a segfault inside the validation layers' own queue-retire code, reported "with renderdoc" — but
   Linux/Mesa/Intel, single reporter, not yet triaged. Worth knowing exists; not necessarily
   reproducible on Windows/MSVC.
3. A [UE5.6.1 forum thread](https://forums.unrealengine.com/t/renderdoc-capture-with-vulkan-rhi-fails-due-to-validation-layer-conflict-in-ue-5-6-1/2689097)
   (Nov–Dec 2025) reports "Failed to load Vulkan driver" combining RenderDoc auto-attach with UE's
   Vulkan RHI validation — root cause identified as RenderDoc's *own* capture layer misreporting
   certain feature/format-support queries, tripping Unreal's *own* internal SM6-profile compliance
   gate (workaround: `-SkipVulkanProfileCheck`). Distinct from a validation-layer clash, and
   engine-specific gating a from-scratch Diligent app won't have.
4. Vulkan's layer/dispatch-chain system is explicitly designed for layers to stack cleanly
   ([Brief guide to Vulkan layers](https://renderdoc.org/vulkan-layer-guide.html)), and running
   validation + RenderDoc together is extremely common in practice. **Net: no fundamental
   incompatibility; occasional real, mostly single-report or platform-specific friction; the one
   actionable rule is "enable validation in one place, not two."**

## Sources

1. [DiligentCore PerformanceGuide.md](https://github.com/DiligentGraphics/DiligentCore/blob/master/doc/PerformanceGuide.md)
2. [RenderDoc capture_attach.rst (v1.45)](https://github.com/baldurk/renderdoc/blob/v1.45/docs/window/capture_attach.rst)
3. [RenderDoc Vulkan Support docs](https://renderdoc.org/docs/behind_scenes/vulkan_support.html)
4. [RenderDoc In-application API docs](https://renderdoc.org/docs/in_application_api.html)
5. [RenderDoc Tips & Tricks](https://renderdoc.org/docs/getting_started/tips_tricks.html)
6. [Brief guide to Vulkan layers (renderdoc.org)](https://renderdoc.org/vulkan-layer-guide.html)
7. [RenderDoc issue #2813](https://github.com/baldurk/renderdoc/issues/2813)
8. [RenderDoc issue #3376](https://github.com/baldurk/renderdoc/issues/3376)
9. [KhronosGroup/Vulkan-ValidationLayers issue #11748](https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/11748)
10. [UE forum: RenderDoc + Vulkan RHI validation conflict](https://forums.unrealengine.com/t/renderdoc-capture-with-vulkan-rhi-fails-due-to-validation-layer-conflict-in-ue-5-6-1/2689097)
11. [Tracy CMakeLists.txt (master)](https://github.com/wolfpld/tracy/blob/master/CMakeLists.txt)
12. [Tracy cmake/options.cmake (master)](https://raw.githubusercontent.com/wolfpld/tracy/master/cmake/options.cmake)
13. [Tracy PR #229](https://github.com/wolfpld/tracy/pull/229)
14. [Tracy issue #494](https://github.com/wolfpld/tracy/issues/494)
15. [Tracy TracyVulkan.hpp (753305a7)](https://github.com/wolfpld/tracy/blob/753305a7/public/tracy/TracyVulkan.hpp)
16. [Tracy manual (tracy.md)](https://github.com/wolfpld/tracy/blob/master/manual/tracy.md)
17. [DiligentSamples SampleApp.cpp](https://github.com/DiligentGraphics/DiligentSamples/blob/master/SampleBase/src/SampleApp.cpp)
18. [DiligentEngine issue #353](https://github.com/DiligentGraphics/DiligentEngine/issues/353)
19. [DiligentCore EngineFactoryVk.h](https://raw.githubusercontent.com/DiligentGraphics/DiligentCore/master/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h)
20. [GLFW docs/vulkan.md](https://github.com/glfw/glfw/blob/e7ea71be/docs/vulkan.md)

## Notes / caveats (read before writing code from this)

- **Unconfirmed, worth a 5-minute local check**: (a) whether the Vulkan case block in
  `SampleApp.cpp` uses `SetValidationLevel` identically to the D3D11 case; (b) whether Diligent's
  Vulkan backend requests `VK_LAYER_KHRONOS_validation` by explicit name at `vkCreateInstance` or
  relies on the layer being externally active — determines whether `SetValidationLevel` alone
  suffices with a bare Vulkan SDK install, or needs `vkconfig`/env-var setup too. Cross-reference
  against `research/diligent-core-api-surface.md`, which reads the vendored source tree directly
  and can likely answer both.
- **Tracy's Vulkan queue-family requirement is a source-code inference, not a quoted doc
  guarantee** — if precision matters, grep the vendored `TracyVulkan.hpp` locally, or test
  empirically: pass a compute/transfer queue and check `timestampValidBits` on target hardware.
- **The RenderDoc/validation-layer "conflict" reports are all edge cases, not a rule** — one
  disputed/unreproduced crash report, one open Linux/Mesa-specific segfault, one UE-specific
  capability-gating issue whose root cause isn't actually the validation layer. Don't
  over-engineer around this; the one concrete actionable rule is "don't double-enable validation
  via both RenderDoc's checkbox and the engine's own toggle simultaneously."
- Everything Diligent-specific in this report (PerformanceGuide.md listing, `SetValidationLevel`
  API, issue #353's validation messages) is verified from live, current primary sources (fetched
  2026-09-02 from `master` branches), not training-data memory.
