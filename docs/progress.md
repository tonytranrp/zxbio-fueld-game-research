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
water) chosen to evoke the *feeling* of that aesthetic. Since the Voxel Representation Redesign
(`research/voxel-representation-redesign.md`), the terrain mesh itself is genuinely blocky —
per-voxel-face, greedy-merged cubes — rather than a smooth iso-surface merely *lit* to look chunky.

## Current state (2026-09-04, after the Voxel Representation Redesign)

`voxel_app` opens on Vulkan or D3D12, pregenerates a **static, bounded world** once at startup
(a real ImGui loading-progress bar while it does), then drops into a **colorful, lit, blocky
world** with no further generation work happening during play at all: terraced, stacked-cube
terrain (grass caps, dirt/stone banding on steep slopes, sand shorelines) built by per-voxel-face
emission with greedy merging (not the smooth Naive-Surface-Nets mesher this project shipped
through 2026-09-04 morning), fresnel water with baked column-depth shore tinting and HDR sun
glints, three deterministic tree silhouettes with wind sway, real per-face-corner baked AO
(0fps.net's actual scheme — blocky faces finally make it exact instead of an approximation), an
analytic gradient sky with a sun disc, and exp2 height fog converging on the sky gradient. The
frame still runs RGBA16F → DiligentFX Bloom → soft-knee tonemap (`docs/render-pipeline.md`).
Movement: fly, walk with gravity, swimming — unchanged by the redesign, but now running against a
world that never mutates under the camera.

Numbers that back this up: **76/76 tests**, all of Groups P–S (block properties, blocky meshing,
the static-world loader) verified landed together, not just per-group. `--verify-frame`'s
local-contrast metric now reads noticeably HIGHER on blocky terrain than the old smooth mesher's
12–14% — 23–30% observed across several real runs, consistent with sharp cube edges carrying more
local contrast than a rounded surface (still comfortably clear of the 6% threshold; the metric
itself is unchanged). Real, Release-build, two-point-measured world-load timing: **56,454 chunks
(the shipping default, a ~3.1km-per-side world) in 53.6s at ~1.41 GB**; **126,150 chunks (a larger
trial) in 125.8s at ~2.10 GB** — roughly 1ms/chunk, near-linear. `extract_mesh` itself got real,
honest, ~2× more expensive from the redesign (5.4–6.8ms vs. the pre-redesign 3.07ms Release
baseline) — the added per-face-corner AO sampling cost, accepted because extraction runs on
background worker threads and never blocks a frame. Interactive frame rate once loaded is high
(136–151fps observed at small test-radius worlds) with zero per-frame streaming overhead by
construction — there is no per-frame streaming code path left to cost anything.

**CI is real and green**: after fixing the invalid-branch-pattern startup failure (`C++-voxel`'s
`+` is a glob quantifier), Windows/Linux×MSVC/GCC/Clang cores, ASan+UBSan, TSan, clang-tidy, and
the full DiligentEngine Windows renderer (including a WARP smoke run on the GPU-less runner) are
green. The Linux Lavapipe renderer leg is best-effort by design and remains unproven end-to-end
(goal 109, unrelated to the redesign).

Debug/verification workflow, mostly predating this redesign and still current: `VOXEL_DUMP_FRAME`
(PNG), `--dump-every N` (now fires during the loading screen too), F2 screenshots,
`--pos/--yaw/--pitch` camera overrides, `--no-post/--no-bloom/--no-tonemap/--no-sky` kill switches,
the crosshair aim readout, per-shape object counts, `tools/mesh_dump` .obj export. `--radius` now
sets the STATIC WORLD's own half-size (goal 132's decision keeps the default at 48 — do not read
this as a streaming-load radius anymore, that system no longer exists). The standing methodology
(a visual or architectural change is verified by actually running it and, for anything rendering-
affecting, viewing a capture) is what caught this pass's two real bugs: a mesher-adjacent test
sampling the wrong side of a column boundary (not a mesher bug), and an unbounded completion-drain
that let a --frames budget mean nothing at world-loader scale (a real bug, fixed).

## Visual review against the original complaint's own reference images (goal 139)

The redesign was a direct response to a pasted complaint naming two images: "image 1" (this
engine, smooth and rounded) and "image 2" (a reference blocky-voxel aesthetic where small objects
like berries read as individually recognizable cube clusters).
`research/captures/baseline_default.png` (this project's own earliest capture, viewed again for
this comparison) IS image 1's complaint made concrete: uncolored, smoothly rounded Surface-Nets
hills with a continuous organic silhouette — no discrete unit is visible anywhere in the terrain,
and the two-part lollipop trees are the only geometry with any variety at all.

`research/captures/blocky_default.png` and `group_s_static_world.png` (this pass's own captures,
viewed directly, not inferred) are a real, specific improvement on that: terrain silhouettes are
now unambiguously terraced and angular, with visible individual stacked-cube steps up every
mountainside, hard-edged grass/dirt/stone banding at each terrace, and sharp blocky shorelines —
the terrain itself now reads as "made of voxels" the way image 2's world does, not merely lit to
suggest it. This is a real, structural change (a different meshing algorithm), not a shading trick.

**The honest gap against image 2 specifically**: image 2's own most distinctive detail — small
decorative objects (berries) reading as individually recognizable voxel clusters — is NOT yet
matched. Trees in this engine are still `tree_decoration.cpp`'s original smooth primitive
composition (box trunks, octahedron canopies), untouched by this redesign, because Group Q's
mesher swap only ever targeted TERRAIN. Group R's design for genuinely blocky decorative objects
is written (`research/micro-voxel-object-design.md`) but not implemented (goals 125/126 open) —
so the honest verdict is: **terrain now matches image 2's aesthetic; small decorative objects do
not yet**, and that gap is named, not glossed over.

## Architecture, as actually built

```
engine/{core,ecs,jobs,input,events}   -- core loop/log/config, EnTT wrapper, ThreadPool (moodycamel-
                                          backed), GLFW input (+G walk toggle, F2 screenshot),
                                          entt::dispatcher event bus
world/{chunk,generation,meshing,streaming}
  chunk/       -- paletted voxel storage; CoordMap/CoordSet over boost::unordered_flat_map;
                  8 materials (Air,Stone,Dirt,Water,Wood,Leaves,Sand,Grass); block_type.hpp's
                  kBlockTable is the single source of truth for every material's real properties
                  (color, is_solid, is_liquid, supports_growth, hardness) -- a constexpr table, not
                  a runtime registry, since the material set is small and compile-time-known
  generation/  -- FastNoise2 heightfield; banded surface materials from (height, depth, slope)
                  with a seam-exact 34x34 margin grid; height_at() analytic query -- UNCHANGED by
                  the redesign, the mesher consuming its output is what changed
  meshing/     -- per-voxel-face emission + greedy merging (replaced Naive Surface Nets): 6
                  axis+direction sweeps, each a 2D exposure mask per layer merged into maximal
                  quads; real per-face-corner baked AO (0fps.net's actual scheme); 12B compressed
                  vertex unchanged in shape (4th byte still AO/water-depth/tree-jitter by material)
  streaming/   -- Group S: no more per-tick decision logic. Header-only now (INTERFACE target):
                  world_bounds.hpp (WorldBounds, chunks_in_bounds() pure shape query,
                  kDefaultWorldBounds) and chunk_events.hpp (ChunkLoaded/ChunkMeshReady --
                  ChunkUnloaded removed, nothing can fire it in a static world)
render/{interface,diligent}  -- interface stays Diligent-free (grep-verified); diligent owns
                  device/PSOs/sky pass/fog constants/post chain (PostProcessor: RGBA16F scene
                  target + DiligentFX Bloom + soft-knee composite)/frame verify+PNG dumps/overlay
                  (DebugOverlay::render_loading is the new one-time load progress bar)
app/          -- main.cpp's run() loop branches on WorldLoader::finished() -- a loading-screen
                  phase, then the interactive phase with NO per-frame streaming call at all;
                  world_loader.{hpp,cpp} (replaced chunk_streaming.*) does one-time parallel
                  generate->mesh->upload, budgeted (with a hard per-call ceiling, not just the old
                  proportional floor -- needed at this scale) the same way the old streaming
                  system paced GPU uploads; spectator camera (fly/walk/swim), tree_decoration
                  (3 shapes), aim_query, crash_handler
tools/mesh_dump (.obj export), benchmarks/ (Google Benchmark + dated baselines +
                  measure_world_memory's one-shot storage report)
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

### The Voxel Representation Redesign's own additions (2026-09-04, second pass)

- **A winding derivation is verified computationally, not just on paper, even the second time
  around** — the blocky mesher's winding (cyclic right-handed basis: axisU=(axis+1)%3,
  axisV=(axis+2)%3 always gives cross(u,v)=+axis) was derived algebraically AND checked against
  actual emitted positions in a golden-value test, per the ribbon-bug lesson this project already
  learned once. It held on the first real build.
- **Greedy merging breaks vertex-proximity-based testing, and that's a real methodology change,
  not a workaround**: a flat, unoccluded run merges into a handful of quads with vertices only at
  its own outer corners — "nearest vertex to point P" (the old Surface-Nets-era test idiom) can
  legitimately find nothing nearby. Tests were rebuilt around footprint containment and bounding-
  region existence checks instead, not patched with bigger tolerances.
- **A budget floor tuned at one scale can be nearly unbounded at another** — the old streaming
  system's "drain at least a quarter of the backlog" anti-starvation floor was proven correct at a
  few-hundred-chunk streaming radius; at a ~15,000-chunk pregeneration backlog, "a quarter" is
  itself thousands, and an unbounded generation-completion drain let --frames 60 silently finish an
  entire 68.9-second load. Fixed with a hard per-call ceiling on top of the same floor, applied to
  both the generation and mesh-upload drains.
- **Real per-face-corner AO turned out to be a simplification, not new work, once faces stopped
  sharing vertices** — 0fps.net's original scheme (each face corner gets its own 3-neighbor
  occlusion check) was always the "real" design goal 10 researched; Surface Nets could only
  approximate it because one vertex served several quads. Blocky meshing needed no new AO design,
  just the one already on file.
- **Don't scale a static world's radius toward a headline number without profiling first** — two
  real measurements (56,454 chunks/53.6s; 126,150 chunks/125.8s) show near-linear, not favorable,
  per-chunk cost; the literal 8km ask extrapolates to ~6 minutes of load time even optimistically.
  The shipping default stays at the measured-good size; reaching 8km later is real profiling work
  (where does ~1ms/chunk actually go), not a parameter bump.
- Decided AGAINST speculatively, in writing: **Group T's Phase 2/3 storage compression** (goal
  135's real 343 MiB measurement fits comfortably; the sparse-grid and SVO/SVDAG phases stay
  gated, un-started, because the gate they're behind wasn't met — the intended good outcome, not a
  skipped step); **Group R implementation** (goal 124's design is written; berries/micro-voxel
  ground objects are optional visual polish, not one of the four originally reported complaints,
  so implementation waited for Group S to close out properly first).

## Honest "what problems does the code have now" (goal 103, re-examined after the redesign)

- **The pre-redesign sliver-curtain defect's status is genuinely unknown, not silently carried
  forward or silently declared fixed**: it was root-caused (as far as three offline
  reproductions could show) to something GPU-side or view-dependent in the OLD Surface-Nets
  mesh, never to the mesh data itself. The mesher has since been completely replaced. Nobody has
  gone looking for the same visual symptom under blocky geometry — it may be gone (a different
  algorithm, a different bug surface), unchanged, or replaced by something new. Real answer
  requires actually looking, not inference from the old bisection notes; goals 73/105 (RenderDoc)
  remain the next real tool either way.
- **`WorldLoader` retains halo-only chunk voxel data forever** (goal 135's measurement: 17 MiB at
  the default world size) — the old streaming system reclaimed this once a halo chunk's one real
  use (satisfying a neighbor's meshing precondition) was done; the new one-time loader never calls
  the equivalent cleanup. Not worth fixing at 17 MiB, but real, measured waste, not assumed
  harmless.
- **The literal 8km world-size ask is not reached, and reaching it needs real profiling work**
  goal 132 didn't do: two real measurements show ~1ms/chunk near-linear cost, extrapolating to
  ~6 minutes of load time at 8km-equivalent scale. The shipping default (a real, ~3.1km-per-side
  world, 53.6s load) is a deliberate, evidenced stopping point, not the original number.
- **Group R (micro-voxel decorative objects — berries, per image 2) is designed but not built.**
  `research/micro-voxel-object-design.md` has the format/placement/meshing design; goals 125/126
  (implementation, memory measurement) are open. This was judged optional visual polish against
  the four originally reported complaints, all of which Groups P/Q/S already address.
- The AO attribute still means three different things by material (occlusion/depth/jitter) — each
  documented and tested, and now genuinely EXACT per corner rather than a Surface-Nets
  approximation, but still the kind of packing a future reader can trip over; goal 110's own note
  stands (widen to 16B if a fourth meaning ever appears).
- `extract_mesh` is a real, honest ~2x more expensive since the redesign (added AO sampling) —
  accepted because it's background-threaded and never blocks a frame, but worth knowing if any
  future change makes meshing throughput itself the bottleneck rather than upload/GPU work.
- The Lavapipe CI leg has never completed (our MSVC-flag leak fixed but unproven, unrelated to
  this redesign); goal 66's Linux half and goal 64's Linux-cache measurement are pending a green
  best-effort run.
- mesh_dump's .obj is structurally validated but the open-in-Blender check was blocked (addon not
  running) — honest partial on goal 74/111, unrelated to this redesign.
- Nobody has profiled the RELEASE renderer's steady-state frame time at the actual shipping-scale
  (48-radius) world during real movement — only load time (goal 131) and small-test-radius
  interactive fps (136-151fps) are measured; these are different questions and the gap between
  them is real, not assumed closed.
- The fly-feel question (goal 83, pre-redesign): speeds are right for a spectator/debug tool at
  this world scale; a persistent-player game would want acceleration curves, unrelated to this
  redesign and unaffected by it.

## Sources this file compresses

`research/voxel-representation-redesign.md` (this pass's own full design record — meshing,
world topology, storage, block properties, sequencing, and citations) and
`research/micro-voxel-object-design.md` (Group R's design). From the prior visual/gameplay/CI
pass: `research/visual-stage-log.md` (the entire visual arc, stage by stage, with viewed-capture
records), `research/baked-ao-design.md`, `research/water-foliage-design.md`,
`research/terrain-fixes-log.md`, `research/engine-hardening-log.md`, `docs/render-pipeline.md`,
plus the earlier subagent reports (`research/diligent-core-api-surface.md`,
`research/radient-tessera-investigation.md`, `research/gpu-driven-voxel-rendering-survey.md`,
`research/profiling-tooling-integration.md`).
