# Progress

Replaces `PROJECT_BRIEF.md`, `PHASE_1_BRIEF.md`, `M1_2_BRIEF.md`, `PHASE_1_COMPLETION_BRIEF.md`,
`ENGINE_HARDENING_BRIEF.md` — those narrated the *process* of getting here; this file states where
"here" actually is. Delete those five after this file and `docs/goals.md` are in place (§0 of
`goals.md` makes that the first goal, explicitly). **Not touched by this consolidation**:
`CLAUDE.md` (machine-specific operational facts — paths, toolchain quirks, exact bugs and fixes —
still the single most load-bearing file in this repo) and `research/*.md` (primary-source subagent
findings, cited by number/detail elsewhere in this file and in `goals.md` — evidence, not narrative).

## What this is

A C++20 voxel terrain engine, DiligentEngine on Vulkan (primary) and D3D12, EnTT, GLM, GLFW,
FastNoise2, CMake + CPM.cmake, Windows/MSVC primary target. Visually and architecturally inspired by
John Lin's voxel work (`voxely.net`, `@johnlin9665`) — not a literal reproduction of it. His actual
technique (per direct research) is a real-time GPU path tracer with 5-bounce GI over per-voxel
material attributes and photogrammetry-converted vegetation — not something a rasterized, 6-material
flat-shaded renderer can copy. `docs/goals.md`'s visual-richness groups are cheap, real techniques
(baked AO, bloom, fresnel water, fog, instanced foliage) chosen to evoke the *feeling* of that
aesthetic, not to match its pipeline.

## Current state (2026-09-04)

A window opens on Vulkan or D3D12, generates and streams Naive-Surface-Nets terrain around a
spectator camera (fly or gravity/walk mode, `G` to toggle), places procedural trees, profiles itself
with Tracy + an ImGui overlay, and reports VRAM via `VK_EXT_memory_budget`. 70/70 tests pass. CI
workflow exists (`.github/workflows/ci.yml`) but has never actually run on a GitHub runner —
`goals.md` group I is that first real run, not assumed to already work because the YAML exists.

**Assessment of the code itself, since it was asked for directly**: genuinely good, on direct
reading, not just from status reports. Specific, not generic — every non-obvious decision is
commented with *why*, citing the exact bug or task that motivated it (e.g. `pso_terrain.cpp`'s
winding-order comment explains the actual visual symptom that proved the on-paper derivation wrong,
not just what the final value is); binary contracts are frozen with `static_assert` so a layout
change breaks the build instead of the rendered image; missing-resource and creation-failure paths
throw instead of drawing with undefined state; the test suite specifically targets boundary cases
(chunk-boundary seams, promotion boundaries, concurrent access) rather than only the convenient
common case; performance claims are backed by real before/after numbers, not assumed. The Radient
investigation in `research/radient-tessera-investigation.md` is genuinely exceptional — it reads
every relevant header and several `.cpp` bodies rather than inferring from names, and reaches a
correct, well-evidenced "not now, here's exactly why" verdict. This is a real, working engine at this
point, not a scaffold.

## Architecture, as actually built

```
engine/{core,ecs,jobs,input,events}   -- core loop/log/config, EnTT wrapper, ThreadPool (moodycamel-
                                          backed), GLFW input, entt::dispatcher event bus
world/{chunk,generation,meshing,streaming}
  chunk/       -- paletted voxel storage; CoordMap/CoordSet type aliases over boost::unordered_flat_map
  generation/  -- FastNoise2 (Simplex -> FractalFBm -> Remap), height_at() query, pinned SSE2
  meshing/     -- Naive Surface Nets, compressed 12B vertex (quantized position + 16-bit octahedral
                  normal + material ID), padded cross-chunk sampling
  streaming/   -- ChunkStreamer: horizontal-only Chebyshev radius over full-height columns (see the
                  ribbon-bug lesson below), two-radii + time hysteresis, delayed unload
render/{interface,diligent}  -- render/interface has no DiligentCore types; render/diligent owns the
                                  device/context/PSO/shaders/frustum-cull/GPU-memory-tracking/debug-
                                  overlay/crash-symbolication
app/          -- composition root: main.cpp's run(), spectator_camera (fly+walk), chunk_streaming
                  (job orchestration), tree_decoration, crash_handler, glfw_window
tools/mesh_dump, benchmarks/ (Google Benchmark, gated behind VOXEL_BUILD_BENCHMARKS)
```

## Decisions that survived contact with evidence (the ones worth remembering, not the whole log)

Every one of these started as a reasoned lean and was **overturned or confirmed by an actual
measurement or a real bug**, not asserted and left unchecked — that pattern is this project's real
methodology and `docs/goals.md` keeps applying it:

- **Hash map**: the reasoned lean was `ankerl::unordered_dense`. The actual local benchmark at
  realistic scale said otherwise — `boost::unordered_flat_map` won on this machine's MSVC; ankerl
  lost even to plain `std::unordered_map` on lookups. Shipped: boost, behind `CoordMap`/`CoordSet`
  aliases (a swap stays a one-line change if evidence changes again).
- **Task queue**: `moodycamel::ConcurrentQueue` (`BlockingConcurrentQueue`), confirmed no code
  anywhere relied on the old queue's FIFO ordering before making the swap — verified, not assumed.
- **Crash handler**: kept the custom SEH handler rather than adopting backward-cpp, because
  backward-cpp doesn't chain an existing SEH filter (source-verified) — it would have replaced, not
  augmented. The custom handler gained the failure classes backward-cpp would have covered instead.
- **The ribbon bug had two independent causes, and the visual evidence pointed at the wrong one
  first.** Cause 1 (streaming applied its radius to Y, loading only a camera-altitude-relative band —
  confirmed against how Minecraft-style streaming actually works, horizontal-only radius over
  full-height columns) was fixed first and *did not fix the ribbons* — proof the bisection continued
  rather than declaring victory on the first plausible fix. Cause 2, found by comparing a real frame
  capture from both sides: terrain winding was inverted since Phase 1, so every up-facing triangle
  was back-face culled and every earlier "verified" frame had been silhouette slivers passing a too-
  loose 5% threshold. Fixed (`FrontCounterClockwise = True`); the threshold is now 25% specifically
  so this class of bug can't slip through quietly again. **The standing lesson, generalized into
  `docs/goals.md`'s methodology note**: a rendering fix isn't verified by a numeric threshold alone
  or by reasoning about a derivation on paper — it's verified by actually looking at a real capture.
- **Radient**: read in full, not inferred from its name — a scene-graph/asset/PBR-application
  framework with zero culling of any kind (verified by an exhaustive grep, not just a docs read).
  Correct call: not adopted for terrain. Its independently-usable pieces (DiligentFX's Bloom/SSAO/
  SSR/TAA/DoF PostProcess modules, confirmed usable standalone without Radient via
  `DiligentSamples/Tutorial27_PostProcessing`) are exactly what `docs/goals.md`'s visual-richness
  groups now build on.

## Sources this file compresses rather than repeats

`research/diligent-core-api-surface.md`, `research/radient-tessera-investigation.md`,
`research/gpu-driven-voxel-rendering-survey.md`, `research/profiling-tooling-integration.md`,
`research/engine-hardening-log.md`, `research/terrain-fixes-log.md` — all primary evidence, still
worth reading directly for the actual derivations; this file is the summary, not a replacement.
