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

## Current state (2026-09-06, after the micro-voxel pivot, the Lin-look/collision/lag pass, and materials as components)

**The world is now sub-centimeter.** `voxel_app`'s default path (`--renderer svo`,
`research/micro-voxel-pivot-log.md`) builds a **sparse-brick octree** around the camera — 8³
bricks of **7.8 mm voxels** within 4 m, halving in resolution per doubling of distance out to a
512 m region — on a background thread in **0.56 s**, uploads it (~200–400 MB depending on how much
surface is in range) and **ray-marches it on the GPU** in one fullscreen pass: 155–159 fps at
1280x720 (vsync-capped) panoramic, 76 fps ground-level with a traced sun-shadow ray and 4-ray AO
per pixel. The same generator, banding rules and trees as before (proven byte-identical to
`fill_terrain` at 1 m), so this is the same world at 128x the resolution, not a new one: islands,
grass caps, conifers, sun-glinting water, fog, all composed through the unchanged bloom/tonemap
chain. The camera moving 2 m triggers a whole-tree rebuild (0.6–1.3 s, background, staged onto
the GPU over ~7 frames and swapped in whole). Mesh-world everything below stays available behind
`--renderer mesh`.

**The Lin-look, collision & lag pass (2026-09-05, `research/lin-look-log.md`, goals 164–175)**
answered the user's second round on that world — shadow "circles" after a rebuild, no collision,
lag near mountains, a "swirly" artifact on every slope, and the look itself ("fine grains and
smooth ... still a voxel but blends"). Each complaint was measured before it was touched. The rings
were secondary rays judging their LOD by distance from the EYE, so the early-out fired on the node
containing their own origin whenever the camera was >2.4x farther from a surface than the tree's
build center — fixed by judging from their own origin (provably self-hit-free), plus a one-cube
lift of the shadow origin along the averaged normal against the scale-free staircase self-shadow
(47% of a 45° slope under this sun, at ANY step size) and a 35% coverage threshold so a mostly-air
LOD node is descended rather than hit. The swirl was the raw face-normal staircase at pixel-sized
cubes: **tree layout v2** stores an area-weighted average normal and a volume coverage per node,
and shading blends the hit cube's face toward that average by projected cube size (cubes above
4.5 px stay cubes — the John Lin close-up — cubes near a pixel take the ~6 px ancestor's normal),
with a per-cube brightness grain that fades out below 1.5 px. The sub-pixel shimmer got a
Halton-jittered, distance-reprojected **TAA** (`svo_taa.psh.hlsl`; no motion vectors — the world is
static). The water checkerboard (also in the user's own screenshot) was the sun glint alone, found
by bisecting one shader return line and sampling one pixel row after every debug view had come back
uniform. The lag was NOT rendering — a GPU timestamp put march+resolve at 3.2–6.3 ms everywhere —
but a tree build on every hardware thread starving `present`: the pool defaults to 3/4 of the
threads and the previous tree's GPU buffers are reused as a spare pair instead of created per swap.
And the camera got a body: `world/collision`'s `move_and_slide` over a `SolidQuery` concept, with
`TerrainCollider` applying the tree's own voxelization rule to the same height function and tree
placements, so it agrees with what is drawn and never depends on the renderer's LOD (`--noclip`
restores the old spectator).

Numbers: **113/113 tests** (27 `world/svo` including the 7,000-ray brute-force traversal oracle
and layout v2's attribute tests, 9 `world/collision`, plus the CPU-rendered smoke frame in the
GPU-less CI jobs); `--verify-frame` reads **34%** local contrast on both Vulkan and D3D12 (the
pre-TAA 48% counted the moiré as contrast); fly `--autofly`, 900 frames: **0 frames over 20 ms**,
worst 16.9 ms (38 ms before the pass); GPU march+resolve **3.2–6.3 ms**; `--autofly --walk`: 0
frames below the ground surface. Captures viewed on both backends: `research/captures/lin_*.png`.
Open: whole-rebuild cost on movement (goal 158), material compression (157), editing (160), the
one-time growth-swap hitch (175, 43 ms when a tree outgrows its spare buffers), and colliding
against the octree once editing exists (173).

**Materials as components (2026-09-06, `research/materials-as-components.md`, goal 176)** closed
the one part of the user's second round the Lin-look pass left unbuilt: every material is now its
own `world/materials/defs/<name>.hpp` component file (name, albedo, phase, shading model, liquid
physics, the two tree-voxelization flags, and the terrain band it `fills()`), composed into
`RegistryOf<Defs...>` — `MaterialID`'s enumerators are DERIVED from the pack's own order, so there
is no enum-order, table-order, or count to keep in sync anywhere. This replaced the three
hand-mirrored copies of the terrain band constants (`terrain_fill.cpp`, `terrain_sampler.hpp`,
`aim_query.cpp`) with one `TerrainBands`/`terrain_material()`, the two GPU color palettes and two
`3u`/`5u` shader literals with one record per material and `MATERIAL_COUNT`/`MAT_SHADING_*` shader
macros (both renderers and the CPU tool read the same table), and the two display-name switch
statements with one `name_of()`. Still exactly what goal 113 asked for — a constexpr table, zero
runtime dispatch — just composed from per-material files instead of one hand-written array.
**119/119 tests** (up from 113: `world_materials_tests`' 10 cases prove the registry reproduces
both retired band rules byte-for-byte over a grid and that exactly one component claims every
terrain voxel; `test_block_type.cpp` and the table it tested are gone), clean incremental build, no
behavioral fix needed after the first full build+test pass.

### The mesh path (still shipped behind `--renderer mesh`, unchanged)

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
itself is unchanged). Real, Release-build, two-point-measured world-load timing (redesign-pass
baseline): **56,454 chunks (the shipping default, a ~3.1km-per-side world) in 53.6s at ~1.41 GB**;
**126,150 chunks (a larger trial) in 125.8s at ~2.10 GB** — roughly 1ms/chunk, near-linear.
`extract_mesh` itself got real, honest, ~2× more expensive from the redesign (5.4–6.8ms vs. the
pre-redesign 3.07ms Release baseline) — the added per-face-corner AO sampling cost. Interactive
frame rate once loaded is high (136–151fps observed at small test-radius worlds) with zero
per-frame streaming overhead by construction — there is no per-frame streaming code path left to
cost anything.

**Chunk-generation optimization pass (2026-09-05, `research/chunk-generation-optimization-log.md`)**:
the 53.6s number above was real but left real waste on the table, surfaced by an actual user
launching via Visual Studio (Debug config) and hitting a far worse load time than that baseline —
exactly the profiling work goal 132 had flagged as undone. Two real fixes, both measured before
and after at the identical radius-48/56,454-chunk world: eliminating `consider_mesh_candidate`'s
per-chunk deep-copy `ChunkStore` snapshot (carried over unchanged from the old per-tick streaming
system, where it was genuinely necessary; provably unnecessary once chunk voxel data is frozen
write-once, which it already was) and fixing `ChunkVoxels`' incremental palette-promotion
thrashing during fill (pre-widening the index buffer once instead of re-packing the whole chunk
from scratch at every bit-width boundary crossed). **Real result: 53.6s → 29.9s, a 44% reduction**,
memory cost negligible (343 → 347.85 MiB, +1.4%). New per-phase instrumentation
(`WorldLoader::log_timings()`) answered goal 132's own question for the first time: generation is
54.22s CPU-time/78,408 chunks (0.692ms/chunk) but meshing is 422.83s CPU-time/56,454 chunks
(7.490ms/chunk) — meshing, not generation, is now the dominant cost by ~7.8x, a genuinely new
finding (goal 147). A `windows-relwithdebinfo` CMake preset was also added so Visual Studio's own
configuration dropdown has a fast option — Debug's `_ITERATOR_DEBUG_LEVEL=2` was independently
re-confirmed (via this project's own prior measurement, cited in `mesh_extractor.cpp`) to cost
~80x for concurrent hash-map-heavy code, which is very likely what made the original complaint's
Debug-launch experience so much worse than the Release numbers ever suggested. Measured directly,
isolating the build-config effect alone (both configs already carrying the two algorithmic fixes,
identical radius-10/2,646-chunk world for both): **Debug 32.1s → RelWithDebInfo 9.4s, a 3.4x
reduction** — the direct fix for "launching via the app is slow."

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
world/{chunk,generation,meshing,streaming,svo,collision}
  svo/         -- MICRO-VOXEL PIVOT (2026-09-05): sparse-brick octree -- 8^3 bricks (material
                  bytes + occupancy mask) under an SVDAG-layout node array with distance LOD built
                  in; layout v2 (Group Z) adds one attribute word per node -- int8 x3 area-weighted
                  average normal + uint8 volume coverage, built bottom-up; TerrainSampler (the
                  generator generalized to meters + implicit trees); HeightField (sound min/max
                  pyramid); parallel build_tree; trace_ray (TraceParams: secondary rays judge LOD
                  from their own origin, coverage-aware early-out), the CPU reference the HLSL
                  marcher mirrors. README.md states the folder's rules.
  collision/   -- Group AA: SolidQuery concept ("does this box overlap solid?"), move_and_slide
                  (y/x/z, bisected, 0.25 m sub-steps, ledge step-up in walk mode, never traps a
                  body that starts inside solid), TerrainCollider (the tree's own voxelization rule
                  over a cached 16 m / 3.1 cm height grid rebuilt on a background thread + trunk
                  boxes; never reads the rendered tree). README.md states the folder's rules.
  chunk/       -- paletted voxel storage; CoordMap/CoordSet over boost::unordered_flat_map;
                  8 materials (Air,Stone,Dirt,Water,Wood,Leaves,Sand,Grass); block_type.hpp's
                  kBlockTable is the single source of truth for every material's real properties
                  (color, is_solid, is_liquid, supports_growth, hardness) -- a constexpr table, not
                  a runtime registry, since the material set is small and compile-time-known
  generation/  -- FastNoise2 heightfield; banded surface materials from (height, depth, slope)
                  with a seam-exact 34x34 margin grid; height_at() analytic query; the spaced-grid
                  variant the svo sampler uses; tree_placement (moved here from app/ so the mesh
                  emitter and the voxelizer share one implicit-shape definition)
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
                  (DebugOverlay::render_loading is the new one-time load progress bar); SvoRenderer
                  + shaders/svo_march.psh.hlsl (the fullscreen GPU ray march writing SV_Depth and,
                  since Group Z, the hit distance for TAA; 12 --debug-view terms) +
                  shaders/svo_taa.psh.hlsl (distance-reprojected temporal resolve, 1/8 blend) + a
                  GPU timestamp pair ("gpu march+resolve" in the overlay); spare GPU buffer pair
                  reused across tree swaps
app/          -- main.cpp: Session (shared setup) + run_svo (SvoWorld background rebuilds on 3/4
                  of the hardware threads -> SvoRenderer; the camera body moves through
                  world/collision; a slow-frame attributor logs every frame > 20 ms by phase and
                  cause) / run_mesh (the loop below); svo_world.{hpp,cpp};
                  the mesh loop branches on WorldLoader::finished() -- a loading-screen
                  phase, then the interactive phase with NO per-frame streaming call at all;
                  world_loader.{hpp,cpp} (replaced chunk_streaming.*) does one-time parallel
                  generate->mesh->upload, budgeted (with a hard per-call ceiling, not just the old
                  proportional floor -- needed at this scale) the same way the old streaming
                  system paced GPU uploads; spectator camera (fly/walk/swim), tree_decoration
                  (3 shapes), aim_query, crash_handler
tools/mesh_dump (.obj export), tools/svo_render (CPU reference frames of the octree world to PNG,
                  a ctest; --lod-center builds the LOD around a point other than the eye, --view
                  renders one shading term), benchmarks/ (Google Benchmark + dated baselines + measure_world_memory's
                  one-shot storage report)
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

### The chunk-generation optimization pass's own additions (2026-09-05)

- **A Release-only measurement can hide a real, much worse Debug-config experience** — the
  redesign pass's own 53.6s/125.8s numbers were real, but a user launching via Visual Studio's own
  F5 (defaulting to the `windows-debug` preset) hit something far worse, because this project had
  already independently measured `_ITERATOR_DEBUG_LEVEL=2`'s cost for concurrent hash-map-heavy
  code at ~80x — a fact documented in a code comment (`mesh_extractor.cpp`) but never connected to
  "what does launching via the IDE actually feel like" until a real user hit it. Lesson: a
  benchmark methodology that only ever measures the shipping config can be blind to the config
  most contributors actually run day to day.
- **A safety mechanism correct for one architecture can become pure overhead in the next one, and
  the redesign's own explicit "carried over unchanged, not the problem being solved" note about
  `consider_mesh_candidate`'s snapshot copy turned out to be wrong** — not because the redesign was
  careless, but because a one-time pregeneration pass has a strictly stronger invariant (voxel data
  frozen forever, immediately) than the per-tick streaming system the copy was designed to protect
  against (a live store under continuous mutation). The lesson isn't "always re-derive every
  carried-over mechanism" (that doesn't scale); it's that a change which strengthens an invariant
  is worth explicitly re-checking every mechanism that invariant makes unnecessary, not just the
  ones the change was aimed at.
- **A palette-compression scheme's own incremental-growth cost is a real, standard, accepted
  trade-off in mainstream prior art (Minecraft's own format, Glowstone, oxidized-chunks all do the
  same thing) — but "standard" doesn't mean "unavoidable for this codebase specifically."**
  Research confirmed no fundamentally different general-purpose algorithm exists; what made a
  pre-widen viable here anyway is that this project's fill happens once, during generation, from a
  small, fully-enumerable, known-in-advance material set — a strictly easier problem than the
  arbitrary-future-edit case Minecraft's format actually has to solve. Worth checking whether a
  "found everywhere" cost is fundamental to the problem or just to a harder version of the problem
  nobody here actually has.
- **A `web-researcher` pass run to sanity-check an already-designed fix is worth doing even when
  confident** — it didn't change either of the two implemented fixes, but it turned "I think
  RelWithDebInfo will keep `_ITERATOR_DEBUG_LEVEL` at 0" into "confirmed against MSVC's own
  documented default-value rule and CMake's own documented flag string," and surfaced a real,
  specific trap (`/fp:fast` risking this project's own already-hard-won FastNoise2 SIMD-determinism
  guarantee) that wouldn't have been found by only reading this codebase's own files.
- Decided to defer, in writing, rather than chase every finding in one pass: **per-column heightmap
  caching** (goal 146 — a real, standard pattern confirmed by research, but generation was already
  the smaller of the two load-time costs by ~7.8x) and **optimizing meshing/AO-sampling itself**
  (goal 147 — now the dominant cost, but a bigger, riskier change than this pass's two fixes,
  deserving its own dedicated profiling pass rather than a rushed addition here).

### The micro-voxel pivot's own additions (2026-09-05, `research/micro-voxel-pivot-log.md`)

- **Write the CPU reference and its oracle before the shader, then use them to debug the GPU.**
  The traversal was proven against a brute-force voxel DDA over 7,000 random rays before one line
  of HLSL existed; when the first GPU frame came out sky-only, `tools/svo_render` at the same pose
  showed terrain in seconds, which said "the data binding, not the algorithm" — and it was
  (Diligent's MUTABLE SRB variables bind once; the tree buffers are replaced on every rebuild).
- **Measure the oversampling per level before optimizing anything.** Three plausible build-time
  culprits (noise calls, trees, cache misses) were each measured and found irrelevant; a per-level
  sampled-vs-kept histogram found the real one in one run — 800K of 1.04M finest-level bricks were
  homogeneous *dirt*, because the classifier could only prove "stone" 3 m down. A sound
  classification rule for the soil band halved the build; no micro-optimization came close.
- **A research brief for a different stack is still worth the read, as long as the stack mismatch
  is named first.** Every technique in the pasted micro-voxel brief transferred (bricked SVO, SVDAG
  node layout, brick size, LOD criterion, the DAG-dedup-vs-editability tension); every library
  recommendation did not. Treating "keep VoxelHex" as a technique instead of a dependency is what
  made it usable.
- **Keep the equivalence with the shipped world as a test, not a hope.** `TerrainSampler` at 1 m is
  byte-identical to `fill_terrain` over 117,600 voxels, which is why the sub-cm world is the SAME
  world; the one column class it skips (negative surface heights) exposed a real pre-existing
  truncation quirk in `fill_terrain` (goal 161) instead of being papered over with a tolerance.
- Decided against, in writing: HashDAG (no editing exists to make it pay yet, goal 160), hardware
  ray queries (portable traversal is already vsync-bound), 4-bit materials (fits the card; goal
  157 with a number), TAA (real shimmer, real goal 159, not this pass).

### The Lin-look, collision & lag pass's own additions (2026-09-05, `research/lin-look-log.md`)

- **Reproduce a moving-camera artifact deterministically before theorizing about it.** The shadow
  rings only appeared "when the rebuild finishes" and slid with the camera; `svo_render
  --lod-center` builds the LOD around a point other than the eye, which made the disc appear on
  the CPU at will, and the `lit` view named the term. The fix (secondary rays judge LOD from their
  own origin) is then a one-line invariant with a proof — the threshold is 0 at the origin, so
  the descent reaches the real leaf — not a tuned bias.
- **One shading term per debug view, and the view that shows the bug IS the explanation.** The
  `facenormal` view is the swirl; the `smooth` view is not. Every view of the water came back
  uniform, which is exactly what forced the shader-return-line bisection that found the glint.
  A frame that names its cause beats staring at a composite.
- **"The lag isn't the rendering" was right, and only a GPU timestamp plus a per-phase attributor
  could say so.** March+resolve was 3.2–6.3 ms the whole time; 12 of 13 slow frames were `present`
  stalls while a build ran on all 16 threads. The obvious fix (smaller upload slices) made it
  WORSE — measured, not assumed — and the real one was two lines (3/4 of the threads; reuse the
  previous tree's buffers). The palette-compression goal (157) stays open with its number because
  the hitch was never about bytes.
- **Staircase self-shadowing is scale-free, so no per-voxel fix can exist**: a slope of tangent s
  puts s/tan(sun elevation) of every tread in its own riser's shadow at any step size — terracing
  at coarse LOD, moiré at pixel-sized steps. Offsetting the shadow origin along the averaged normal
  is the standard answer (Gustafsson); the exception (solid leaves get no lift — a water top IS
  its surface) was found by a D3D12 capture, not by reasoning.
- **Collide against the function, not the render.** The renderer's tree is correct only near its
  build center and lags the camera by one rebuild; the analytic height function agrees with what is
  drawn everywhere and costs nothing per tree. The `SolidQuery` concept is the seam where an
  octree-backed query goes once editing (goal 160) makes the analytic world stale (goal 173).
- Decided against, in writing: per-voxel stored normals (3–4x the brick bytes; the per-node average
  blended by projected size gives the same distant smoothness and keeps the close-up cubes the user
  asked for), cone-traced soft shadows (a look change nobody asked for), DiligentFX's own TAA
  (needs motion vectors and its PostFXContext machinery for a static world; ours is 80 lines over
  the distance the march already writes).

## Honest "what problems does the code have now" (goal 103, re-examined after the redesign)

- **The svo path rebuilds the whole tree and re-uploads 200–540 MB whenever the camera moves 2 m**
  (0.6–1.3 s in the background, staged onto the GPU at 32 MB/frame, swapped into a reused spare
  buffer pair). Fly autofly now has 0 frames over 20 ms, but the far rings barely change between
  rebuilds and the finest ring lags a fast-moving camera; goal 158's incremental reuse is the real
  fix, deliberately deferred until measured. A tree that outgrows its spare buffers still pays a
  synchronous creation (43 ms at 540 MB, one-time growth — goal 175).
- **TAA hides the sub-pixel shimmer rather than removing its source**: the marcher still stops at
  nodes, not at the 8 mm voxels inside the finest bricks; history is rejected on a 2% + 5 cm
  distance mismatch and clamped to the 3x3 neighborhood, so a hard camera cut shows one frame of
  the raw moiré, and `--no-taa` shows the residual any time. The lighting half of the moiré is
  genuinely gone (the averaged-normal blend), the geometric half is filtered.
- **Collision is analytic, not against the octree**: `TerrainCollider` agrees with the drawn
  surface because both come from the same height function and tree placements. The moment editing
  mutates the tree (goal 160) the collider is stale — goal 173's octree-backed `SolidQuery` is the
  planned swap, behind a boundary that already exists.
- **The svo world cannot be edited** — it is rebuilt from the analytic sampler; there is no
  mutable structure (goal 160).

### The materials-as-components pass's own additions (2026-09-06, `research/materials-as-components.md`)

- **Derive the enum from the composition, don't declare it beside it.** `MaterialID`'s enumerators
  read `Registry::index_of<defs::X>()`, so a material's id IS its position in the
  `RegistryOf<Defs...>` pack — there is no second declaration that could drift from the table, and
  `RegistryOf`'s own `static_assert` that component 0 is the Gas phase turns "Air is 0" from a
  convention several files independently trusted into a build failure to violate.
- **A property with zero consumers doesn't get carried into the new schema on the chance it gains
  one.** The survey found `supports_growth` and `hardness` declared and unused in the old table;
  neither is a `MaterialDef` field now. Re-adding either is a new member in the struct and all
  eight `defs/*.hpp` files when something real reads it.
- **Prove the refactor against oracles, not just against itself.** `test_registry.cpp` keeps the
  two retired band rules verbatim as `old_chunk_rule`/`old_sampler_rule` functions and checks the
  new `terrain_material()` against both over grids covering every voxel edge from 1 m down to the
  finest sparse-brick leaf — the byte-for-byte equivalence is a checked fact, not an inspection
  claim.
- **A field becoming a method is a feature, not a wart.** `is_liquid`/`is_solid`/`is_occupied` went
  from stored bools to methods derived from one `Phase` enum; every old call site failed to compile
  until fixed, which is what surfaced all of them instead of trusting the survey's grep to be
  exhaustive.
- **"Which shading model" moved to data; "how it looks" deliberately did not.** The shaders read
  `MAT_SHADING_WATER`/`MAT_SHADING_FOLIAGE` macros instead of material-ID literals, but the three
  renderers' three independently-tuned water body colors were left alone — unifying them was never
  the ask, and each was judged against its own renderer's viewed captures.

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
- `extract_mesh` is a real, honest ~2x more expensive since the redesign (added AO sampling), and
  the chunk-generation optimization pass (2026-09-05) found that meshing throughput HAS become a
  real bottleneck, just not the one this note originally anticipated: "background-threaded, never
  blocks a frame" is true for per-frame streaming, but during bulk upfront loading the aggregate
  meshing cost across every worker thread directly gates how long the loading screen lasts. Real
  numbers: meshing is now ~7.8x generation's total CPU-time (422.83s vs. 54.22s at the radius-48
  world). Goal 147 names this as open, deliberately not attempted in that pass (a bigger, riskier
  change than eliminating the snapshot-copy/palette-thrashing waste that pass did fix).
- Per-column heightmap data is recomputed independently for every chunk stacked at the same (x,z)
  column across the world's Y-range (up to 8x redundant) — a real, standard pattern elsewhere
  (Veloren's `SimChunk`, Cuberite's `cHeiGenCache`) that goal 146 names as open. Not implemented in
  the 2026-09-05 pass because generation (the phase this would improve) was already the smaller of
  the two costs by ~7.8x — a real, evidenced deprioritization, not an oversight.
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

`research/materials-as-components.md` (materials as components: the schema, the registry
mechanism, what closed each of the survey's hardcoded sites, what was deliberately not carried
forward, and the verification).
`research/lin-look-log.md` (the Lin-look, collision & lag pass: what each of the user's five
complaints measured as, the bisection tables, the 900-frame A/B, and what was decided against).
`research/micro-voxel-pivot-log.md` (the sub-cm pivot: every design decision, the build-time
optimization table, and the real numbers).
`research/chunk-generation-optimization-log.md` (the prior pass's own full record — root causes, the
research cross-check, and every before/after number). `research/voxel-representation-redesign.md`
(the prior pass's own full design record — meshing, world topology, storage, block properties,
sequencing, and citations) and `research/micro-voxel-object-design.md` (Group R's design). From
the earlier visual/gameplay/CI
pass: `research/visual-stage-log.md` (the entire visual arc, stage by stage, with viewed-capture
records), `research/baked-ao-design.md`, `research/water-foliage-design.md`,
`research/terrain-fixes-log.md`, `research/engine-hardening-log.md`, `docs/render-pipeline.md`,
plus the earlier subagent reports (`research/diligent-core-api-surface.md`,
`research/radient-tessera-investigation.md`, `research/gpu-driven-voxel-rendering-survey.md`,
`research/profiling-tooling-integration.md`).
