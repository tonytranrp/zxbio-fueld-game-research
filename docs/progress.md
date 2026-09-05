# Progress

[![ci](https://github.com/tonytranrp/zxbio-fueld-game-research/actions/workflows/ci.yml/badge.svg?branch=C%2B%2B-voxel)](https://github.com/tonytranrp/zxbio-fueld-game-research/actions/workflows/ci.yml)

The current-state summary. `docs/goals.md` is the living backlog; `CLAUDE.md` stays the
machine-specific operational reference; `research/*.md` are primary evidence (decision logs,
design notes, viewed-capture records). This file replaced the five root narrative briefs
(deleted 2026-09-04; retrievable from git history — code comments citing them by section number
are historical citations, renamed to plain-text names).

## What this is

A C++20 voxel terrain engine, DiligentEngine on Vulkan (primary) and D3D12, EnTT, GLM, GLFW,
FastNoise2, CMake + CPM.cmake, Windows/MSVC primary target. Visually and architecturally inspired
by John Lin's voxel work — not a literal reproduction: his actual technique (per direct research)
is a real-time GPU path tracer with 5-bounce GI; this project's visual arc uses cheap, real
techniques (baked AO, hemisphere ambient, bloom, analytic sky + sun, exp2 height fog, fresnel
water, instanced-free primitive foliage) chosen to evoke the *feeling* of that aesthetic.

## Current state (2026-09-04, end of the visual/gameplay/CI pass)

`voxel_app` opens on Vulkan or D3D12 and streams a **colorful, lit, animated world**: grass-capped
terrain with dirt soil bands and exposed rock on steep slopes, sand shorelines, fresnel water with
baked column-depth shore tinting, scrolling ripples and HDR sun glints, three deterministic tree
silhouettes (round/conifer/shrub) with per-tree brightness jitter and canopy-only wind sway, baked
per-vertex concavity AO, two-color hemisphere ambient under a warm sun, an analytic gradient sky
with a blooming sun disc, and exp2 height fog that converges on the sky gradient per view
direction. The frame runs through an RGBA16F offscreen target → DiligentFX Bloom → soft-knee
tonemap composite (`docs/render-pipeline.md` is the pass-by-pass map; every pass has a kill
switch). Movement: fly (WASD+mouse), walk with gravity (`G`), and **swimming** — buoyancy floats
you at the surface over deep water, the seabed is the hard floor in shallows.

Numbers that back this up: **76/76 tests** (from 70 at the pass's start; every new subsystem got a
boundary test, one of which caught a real off-by-one in the water-depth scan). `--verify-frame`
12–14% on the local-contrast metric (threshold 6%; sky-only measures 0.9%) on BOTH backends.
165 fps settled (debug build, RTX 4070 laptop) — unchanged through the entire visual arc; every
stage's cost was measured and none exceeded run-to-run noise. `extract_mesh` 3.07 ms Release
(3.97 ms pre-pass baseline; new baseline `benchmarks/baselines/2026-09-04-visual-pass.json`).

**CI is real now**: after fixing the invalid-branch-pattern startup failure (`C++-voxel`'s `+` is
a glob quantifier — three zero-job failures before anything ever ran), run 33941021916 is the
first fully green run: Windows/Linux×MSVC/GCC/Clang cores, ASan+UBSan, TSan (with one documented
third-party suppression for FastNoise2's SmartNode allocator — `.tsan-suppressions`), clang-tidy,
AND the full DiligentEngine Windows renderer job including a **successful 5-frame WARP run on the
GPU-less runner**. CPM + sccache/ccache caching verified working (second run restored the cache).
The Linux Lavapipe renderer leg is best-effort by design and first failed on our own MSVC-flag
leak (fixed, unproven at the time of writing).

Debug/verification workflow built this pass and worth knowing: `VOXEL_DUMP_FRAME=x.png` (PNG via
DiligentTools' bundled encoder), `--dump-every N`, F2 in-app screenshots, `--pos/--yaw/--pitch`
camera overrides, `--no-post/--no-bloom/--no-tonemap/--no-sky` per-pass kill switches, the aim
readout on the overlay (analytic crosshair material query), per-shape object counts, and
`tools/mesh_dump` now exporting real `.obj` files. The standing methodology (a visual change is
verified by VIEWING a capture, never a number alone) is what caught the ACES mid-tone washout,
the whole-frame bloom haze, the fog/sky mismatch artifact, and the metric breaks below.

## Architecture, as actually built

```
engine/{core,ecs,jobs,input,events}   -- core loop/log/config, EnTT wrapper, ThreadPool (moodycamel-
                                          backed), GLFW input (+G walk toggle, F2 screenshot),
                                          entt::dispatcher event bus
world/{chunk,generation,meshing,streaming}
  chunk/       -- paletted voxel storage; CoordMap/CoordSet over boost::unordered_flat_map;
                  8 materials (Air,Stone,Dirt,Water,Wood,Leaves,Sand,Grass)
  generation/  -- FastNoise2 heightfield; banded surface materials from (height, depth, slope)
                  with a seam-exact 34x34 margin grid; height_at() analytic query
  meshing/     -- Naive Surface Nets; 12B compressed vertex whose 4th byte is baked AO on land,
                  water-column depth on water, brightness jitter on trees; highest-corner
                  material pick
  streaming/   -- horizontal-radius full-column streaming, hysteresis, budgeted uploads/releases
render/{interface,diligent}  -- interface stays Diligent-free (grep-verified); diligent owns
                  device/PSOs/sky pass/fog constants/post chain (PostProcessor: RGBA16F scene
                  target + DiligentFX Bloom + soft-knee composite)/frame verify+PNG dumps/overlay
app/          -- main.cpp's run() split into named phases; spectator camera (fly/walk/swim),
                  chunk_streaming orchestration, tree_decoration (3 shapes), aim_query,
                  crash_handler
tools/mesh_dump (.obj export), benchmarks/ (Google Benchmark + dated baselines)
```

## Decisions that survived contact with evidence (this pass's additions)

- **The verify metric had to evolve twice in one day, and the final form is better than the
  original**: bytewise compare-to-reference died when bloom made 97.7% of sky pixels "differ"; a
  tolerance band died when the gradient sky swept 100+/255 across the frame. The durable metric is
  LOCAL CONTRAST (terrain texture vs. smooth-by-construction sky/fog/bloom): 12–14% real terrain
  vs 0.9% empty, 13.7× separation, threshold 6%.
- **ACES was tried and rejected by a viewed capture** (mid-tones washed ~30%); the shipped tonemap
  is a soft-knee (identity below 0.75) whose only job is rolling off bloom overshoot.
- **Bloom readiness gates on PostFXContext::Execute at this DiligentFX pin** — the research's
  "standalone Bloom" is true only after ONE warm-up Execute with dummy inputs flips the sticky
  PSO-ready flag (found empirically: bloom returned PENDING forever, contribution measured
  literally zero by masked pixel-diff).
- **Fog must converge on the sky gradient along each view direction, not a flat color** — flat
  fog made fogged ridges vanish against the gradient while their darker trees lingered as
  floating dashes.
- **Water renders opaque, deliberately**: nothing is meshed beneath water to alpha-blend against;
  the depth tint (baked per-vertex column depth) IS the transparency. Revisit only when goal 80's
  answer changes.
- **Sand/Grass banding is a pure function of (height, depth, slope) with a margin grid**, so
  chunk seams cannot disagree; the mesher picks each surface cell's HIGHEST solid corner material.
- Decided AGAINST, in writing (do not re-litigate from scratch): **SSAO/G-buffer** (goal 41's
  gate, in `docs/render-pipeline.md` — reopen with textures or a real G-buffer need);
  **cave/3D-density terrain** (goal 80 — a full future arc touching generation+meshing+streaming,
  not a line item); **grass ground-cover geometry** (goal 40 — needs instancing+textures);
  **RG16 normals** (no visible banding, viewed); **backward-cpp removal** (premise false — never
  declared); **unordered_dense removal** (kept, benchmark-harness-only).

## Honest "what problems does the code have now" (goal 103)

- **One known visual defect**: thin stone sliver curtains visible from rare grazing angles
  (full repro + bisection state in `research/water-foliage-design.md`) — absent from mesh data
  under three offline reproductions, so GPU-side or view-dependent; next tool is RenderDoc pixel
  history (goal 73, still deferred for its vendored header).
- The AO attribute now means three different things by material (occlusion/depth/jitter) — each
  is documented and tested, but it is the kind of clever packing a future reader can trip over;
  a second attribute byte would cost stride 16 and should be weighed if a fourth meaning appears.
- The Lavapipe CI leg has never completed (our MSVC-flag leak fixed but unproven); goal 66's
  Linux half and goal 64's Linux-cache measurement are pending a green best-effort run.
- mesh_dump's .obj is structurally validated but the open-in-Blender check was blocked (addon not
  running) — honest partial on goal 74.
- Debug-build fps mid-streaming (~14–20) is generation-bound and fine for development, but nobody
  has profiled the RELEASE renderer under the new post chain yet (the standing Tracy-session
  deferral).
- The fly-feel question (goal 83): assessed from extensive autofly/capture flying — speeds are
  right for a spectator/debug tool at this world scale; a persistent-player game would want
  acceleration curves, which belongs to whatever future arc adds real player controls.

## Sources this file compresses

`research/visual-stage-log.md` (the entire visual arc, stage by stage, with viewed-capture
records), `research/baked-ao-design.md`, `research/water-foliage-design.md`,
`research/terrain-fixes-log.md`, `research/engine-hardening-log.md`, `docs/render-pipeline.md`,
plus the earlier subagent reports (`research/diligent-core-api-surface.md`,
`research/radient-tessera-investigation.md`, `research/gpu-driven-voxel-rendering-survey.md`,
`research/profiling-tooling-integration.md`).
