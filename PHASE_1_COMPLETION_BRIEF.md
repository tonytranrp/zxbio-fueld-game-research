# Phase 1 Completion — Master Task List (M1.3 → M1.7 + Streaming)

Companion to `PROJECT_BRIEF.md`, `PHASE_1_BRIEF.md`, and `M1_2_BRIEF.md`. Scope: take the project
from "world generation works, headless" to "a window opens, terrain streams in around a flying
spectator camera, and a debug overlay shows what's actually happening."

This document was originally drafted without direct repository access (its own §0 disclosed this
plainly) — verified against the real repo on 2026-09-03 before any implementation started. See
"Verification against the real repo" below for what was confirmed.

Table of contents: [1. Status recap](#1-status-recap) · [2. New research this pass](#2-new-research-this-pass)
· [3. New modules this phase needs](#3-new-modules-this-phase-needs) · [4. The task list (35 tasks)](#4-the-task-list-35-tasks)
· [5. External libraries](#5-external-libraries--whats-actually-new) · [6. What the code likely has right now](#6-what-the-code-likely-has-right-now)
· [7. Guardrails](#7-guardrails)

## Verification against the real repo (2026-09-03, before implementation started)

- **Branch name**: `C++-voxel`, confirmed — the document's own aside about a `C#-voxel` URL was
  just a copy/autocorrect artifact, not a real discrepancy.
- **M1_2_BRIEF.md §9's patch notes**: confirmed already landed in both `PROJECT_BRIEF.md` and
  `PHASE_1_BRIEF.md` (task 34's doc-sync concern was already satisfied before this document arrived).
- **CI matrix and clang-tidy**: confirmed genuinely not wired up — matches §6's inference exactly.
- **The `FastSIMD::SCALAR`→`SSE2` guard's throw path**: confirmed NOT exercised by any test — matches
  §6's flagged uncertainty. Judged not worth a dedicated test: the guard is a trivial 3-line null
  check, and the actual property it depends on (this build's FastNoise2 lacking `SCALAR`) was
  already verified empirically once, via the real crash-then-fix cycle — testing the defensive
  branch itself would mean adding public API surface or a new link dependency purely for a
  low-risk, already-inspected 3 lines.

## 1. Status recap

Done and verified before this document: M1.1 (engine skeleton) and M1.2 (paletted chunk storage,
FastNoise2 generation). This document covers everything after that: meshing, the render window,
the camera, chunk streaming, and the profiling/debug overlay.

*(§2–§7 below are preserved as originally drafted — the technical content, research, and task list
were sound; only the epistemic framing in §0/§6 needed reconciling against the real repo, done
above. See the original document for the full text: Surface Nets specification, chunk-streaming
hysteresis design, VRAM/Tracy/ImGui research, the 35-task list across Groups A–F, and guardrails.)*

## Progress log

**Group A (M1.3, Surface Nets meshing): DONE (2026-09-03).** All 8 tasks implemented in
`world/meshing`. 5/5 new tests pass (29/29 across the whole suite), including two that caught real
issues during development — not in the meshing algorithm itself, but in the tests' own first-draft
verification methodology:

- The hand-derived per-axis winding order (X/Z axes' natural vertex order matches "lower-endpoint-
  solid"; Y is the odd one out, matching "higher-endpoint-solid" — a real asymmetry from how the
  right-hand-rule cross product interacts with which two axes are held fixed per direction, derived
  concretely by hand rather than assumed symmetric) is verified correct via a signed-volume check
  (divergence theorem: a closed, consistently-outward-oriented mesh has positive signed volume) —
  a first-draft per-quad "is this quad's center on the expected side of the voxel" heuristic gave a
  false failure, because Naive Surface Nets pulls each vertex toward its cell's crossing corner
  rather than placing it at the cell's geometric center, breaking that heuristic's assumption. The
  signed-volume check is standard, robust, and independent of exact vertex placement.
- A real design gap found while implementing task 2 (padded cross-chunk sampling): a chunk must
  emit vertices for its `-1` boundary-layer cells too, not only its own owned `[0,31]` cells — its
  own boundary quads reference those cells as their negative-side neighbor, even though the chunk
  never independently owns *quads* anchored there (that belongs to the neighbor on the other side).
  Missing this would have produced systematically-missing boundary geometry, not caught by any
  single-chunk test — only the cross-chunk seam-continuity test would have caught it, which is
  exactly why that test is one of the four task 8 named explicitly.

**Group B (M1.4, rendering core): DONE (2026-09-03).** All 8 tasks (9–16). `render/diligent` is
now a real STATIC library (device/context/swap-chain init, terrain PSO + HLSL shader pair under
`render/diligent/shaders/`, per-chunk vertex/index upload with the §2.3 allocation-tracking hook,
CPU frustum culling, single-threaded immediate-context draw), `render/interface` gained the
GLM-only `camera.hpp`, and `app/` owns the GLFW window (GLFW_NO_API; render/diligent consumes only
the opaque HWND and never links GLFW). Verified, not assumed:

- **Runtime device enumeration confirmed on both backends** (task 9/16's §0 build-vs-runs
  distinction): `voxel_app --mode vk` and `--mode d3d12` both initialize on the real GPU (NVIDIA
  GeForce RTX 4070 Laptop GPU, Vulkan 1.4.341 / D3D12) and run a windowed draw loop to clean exit.
- **"Terrain actually visible" is checked mechanically, not by eyeball**: `--verify-frame` reads
  the back buffer back through a staging texture and fails the run if every pixel matches the
  top-left (sky) reference pixel. Both backends report an identical 16.8% non-sky fraction on the
  default scene — bit-identical cross-backend output is a much stronger correctness signal than
  either run alone.
- The hand-derived conventions the brief flagged as classic silent failures all came out right on
  the first lit frame: `GLM_FORCE_DEPTH_ZERO_TO_ONE` [0,1] NDC depth (already in
  `engine/core/math.hpp` since M1.1; `gtc/matrix_transform.hpp` include added), explicit
  `column_major` HLSL matrices so raw GLM bytes upload with no transpose, and default
  `FrontCounterClockwise=False` matching Group A's winding under GLM's RH view + viewport y-flip.
- 36/36 tests (7 new: 5 frustum — including a quaternion-orientation case — and 2 allocation-
  tracker, one exercising concurrent allocate/free with 8 threads). Frustum culling and the
  GPU-byte tracker are deliberately headless-testable; device/PSO/draw paths are covered by the
  `--frames N --verify-frame` smoke run instead, so ctest stays GPU-free.
- Known, accepted for now: meshing the startup grid (82 non-empty of 294 meshed coords) takes
  ~40 s in this Debug (/Od /RTC1) build — extract_mesh resolves every one of ~287k padded samples
  per chunk through `ChunkStore::find`. Correct first, per the guardrails; Group E's Tracy zones
  are the measurement this optimization waits for, and Group D moves meshing onto worker threads.

**Group C (M1.5, camera & movement): DONE (2026-09-03).** Tasks 17–20: `CameraLens` component in
`engine/ecs` (pose stays in the existing `Transform`), the camera as an ordinary ECS entity in
`voxel_app`, `engine/input` (GLFW callbacks → semantic `InputState`, GLFW linked PRIVATE so no
consumer compiles against it), and an app-side spectator controller (WASD + Space/Ctrl + Shift
boost, RMB-held mouse-look with cursor capture, Esc quits). Orientation is stored as a quaternion
rebuilt each update from controller yaw/pitch (yaw about world-up × pitch about local-right —
two fixed axes can't gimbal-lock, stays defined at the ±89.9° clamp, and no roll drift). 5 unit
tests cover dt-integration (60×1/60s ≡ 1×1s), yaw-rotates-the-movement-basis, the pitch clamp
still moving at the limit, diagonal normalization + boost, and opposed-input cancellation. The
"feels correct" half of M1.5's done-when is inherently hands-on — flyable now via `voxel_app`.

**Group D (streaming, tasks 21–26): DONE (2026-09-03).** `world/streaming`'s `ChunkStreamer` is
the pure decision logic (Chebyshev desired cube clamped+anchored to the terrain y-band, two-radii
spatial hysteresis, continuous-time unload delay, stale-completion checks) — 6 unit tests including
the continuity-reset and camera-flies-high cases. `app/src/chunk_streaming.cpp` runs its commands:
generation jobs fill standalone chunks handed back through locked completion queues (ChunkStore
stays main-thread-only, zero locks on world data); mesh jobs run against private 27-chunk snapshot
stores; completions are discarded when no longer desired (§2.2's v1, no cancellation); unload
tears down GPU buffers (allocation tracker decrements observed live), the ECS pipeline entity, and
the voxel data, in that order — safe because the ≥2-chunk hysteresis gap keeps unloaded coords out
of every live meshing halo. Verified end to end on both backends: `--verify-frame` streams the
full 196-chunk initial set and passes (identically on Vulkan and D3D12), and `--autofly` (camera
auto-flying at boost speed for 15 s) holds the loaded set bounded at ~528 ≤ 588 with stable
~15–19 MiB tracked GPU memory — the mechanical proof that delayed unload actually unloads.

**Two real bugs found by the Group D smoke runs, both fixed and re-verified:**

1. **Teardown lifetime crash (access violation), caught by the new SEH crash reporter**
   (`app/src/crash_handler.cpp`, added for exactly this): with the worker pool owned outside the
   streaming system, any exit path while jobs were in flight let a mesh job push into an
   already-destroyed completion vector. Fix: `ChunkStreamingSystem` owns its `ThreadPool`,
   declared **last**, so destruction joins (and drains) every worker while the queues/mutexes/
   heightmap the jobs capture are still alive — the same member-order lesson `ThreadPool`'s own
   header documents, one level up.
2. **~80× throughput collapse in concurrent meshing** (initial load visibly stalled: 148 mesh
   jobs in flight, ~1 completion/s on 16 workers): every one of ~295k padded samples per
   extraction did a `ChunkStore::find`, and each find's iterator construct/destroy goes through
   MSVC's _ITERATOR_DEBUG_LEVEL=2 **global** debug lock, serializing all workers. Fix: a
   3×3×3 `NeighborCache` of chunk pointers resolved once per extraction, indexed per sample
   (`world/meshing/src/mesh_extractor.cpp`) — also strictly cheaper than a hash find in release.
   After: the 196-chunk initial load settles within ~8 frames; the 16-thread meshing stress test
   dropped from 268 s to ~3 s. (This also retires Group B's "~40 s debug startup" caveat — most
   of that was the same per-sample find cost, single-threaded.) Two permanent stress tests pin
   both concurrency contracts: shared-`HeightmapGenerator` concurrent generation and concurrent
   `extract_mesh` over snapshot stores, each also asserting determinism.

**Group E (profiling & debug overlay, tasks 27–32): DONE (2026-09-03), one flagged deferral.**
Tracy v0.14.1 added via CPM (latest tag, verified live; `TRACY_ON_DEMAND` so the client is dormant
until a profiler connects). Task 27: `ZoneScopedN` on chunk generate / chunk mesh / chunk upload /
terrain render / streaming update, `FrameMark` per frame. Task 28: the Tracy Vulkan context is
created through the research report's exact `IRenderDeviceVk`/`ICommandQueueVk`/`IDeviceContextVk`
QueryInterface chain, in `TRACY_VK_USE_SYMBOL_TABLE` mode with `vulkan-1.dll` loaded at runtime —
no Vulkan SDK import library dependency — and the command buffer is re-fetched from Diligent at
every use per the header's own warning; runtime-confirmed ("Tracy Vulkan GPU-zone context
attached"). Task 29 was built in Group B; the overlay now displays it. Task 30:
`VK_EXT_memory_budget` requested at device creation (with a fallback retry without it), polled on
a 2-second timer, values confirmed real (7180 MiB device-local budget / 79 MiB machine-wide usage
on the RTX 4070). Task 31: ImGui overlay via DiligentTools' vendored renderer + the vendored GLFW
platform backend — zero new dependencies, chain-installed after `engine/input` so both see events;
its presence is even mechanically visible (the frame-verification non-sky fraction rose from 8.6%
to 13.4% = overlay pixels). **Task 32 deferred, flagged**: the in-app RenderDoc trigger requires
`renderdoc_app.h`, which is not vendored anywhere in the local dependency set, and hand-declaring
that API's function-pointer-table layout from memory is exactly the kind of silent-corruption risk
not worth taking — launching `voxel_app` through RenderDoc's UI already provides F12 captures
(with every GPU object debug-named) at zero integration cost.

---

**Engine Hardening pass (ENGINE_HARDENING_BRIEF.md Groups G–P): DONE (2026-09-04).** Full
per-task decision log with evidence in `research/engine-hardening-log.md`; headline results:

- **3 research subagents** ran (§2's verbatim prompts); every decision-driving claim was
  re-verified locally before adoption (Group G): live tag/license checks, isolated CPM+MSVC smoke
  tests of all candidates, and a source-level confirmation of the one claim that changed a
  decision (`unordered_dense::map`'s iterator IS `std::vector`'s → MSVC IDL=2-exposed).
- **Compressed GPU vertex format (Group K)**: 28B→12B per vertex + 32→16-bit indices, decoded by
  fixed-function normalized fetch + arithmetic only (no shader bit ops — chosen specifically
  because glslang's HLSL front-end is deprecated and Diligent's GL converter lacks `f16tof32`).
  Dyadic 1/1024-voxel position quantization keeps chunk seams lattice-aligned; 16-bit octahedral
  normals (round-trip max error <1.5° asserted by test). **Measured: autofly VRAM 17.0→7.1 MiB
  (2.39x), peak 18.1→8.3 MiB; `--verify-frame` passes on Vulkan (13.4%) AND D3D12 (13.1%).** The
  per-backend check caught a real bug: DXGI has no 3-component 16-bit vertex format — 3x
  VT_UINT16 asserted on D3D12 while working on Vulkan; fixed with a 4-component position.
- **Coordinate containers (Group H)**: all ChunkCoord-keyed maps/sets now go through
  `CoordMap`/`CoordSet` aliases backed by `boost::unordered_flat_map/_flat_set` — picked by THIS
  machine's own MSVC benchmark (no independent MSVC numbers exist anywhere): at the realistic
  558-chunk scale boost_flat beat both std and `ankerl::unordered_dense` on every workload
  (build+teardown 17.3µs vs 38.0/30.3µs; find-hit 3.0µs vs 3.7/6.2µs), and its custom iterators
  are immune to the IDL=2 checked-iterator locking class behind Group D's Debug collapse. The
  existing `h*31` ChunkCoord hash was avalanche-TESTED (statistically uniform on clustered keys
  under both extraction schemes; identity-style packing confirmed catastrophic).
- **ThreadPool interior (Group I)**: mutex+`std::queue`+condvar replaced by
  `moodycamel::BlockingConcurrentQueue` v1.0.5 (already shipping inside the binary via Tracy);
  `std::jthread`+`stop_token` shell, public API, and future-returning `submit()` unchanged;
  sentinel-wake shutdown preserves drain-on-destruction. No-FIFO-reliance verified (single
  producer + explicit state machine).
- **Crash handler (Group J)**: kept in-house (research showed backward-cpp is the same
  architecture and does NOT chain SEH filters), extended with `std::terminate`/SIGABRT/purecall/
  invalid-parameter hooks + debug-only `--crash-test av|abort|terminate`. The deliberate-crash
  check caught two real bugs: a by-value `CONTEXT` losing its 16-byte alignment (StackWalk64
  unwound garbage) and Tracy's dbghelp init making our `SymInitialize` fail (error 87 → now
  `SymRefreshModuleList`). All three modes print traces naming the exact crash site. Release
  builds now keep full PDBs (deliberate decision, root CMakeLists).
- **Events (Group L)**: `engine/events` (`entt::dispatcher` alias, zero new deps) + chunk
  lifecycle events; the overlay's ready-count is event-derived with a poll-consistency check that
  stayed silent through verify+autofly runs. Event-vs-polling convention documented in the
  header.
- **Group M/N**: MSVC `/Qvec-report:2` shows NOTHING in Surface Nets auto-vectorizes (reasons
  1106/1200/1305/503 quoted in the log — honest outcome, no change); Google Benchmark v1.9.5
  harnesses for map ops / octahedral / `extract_mesh` (3.97ms, 869 verts baseline) with
  `benchmarks/baselines/` JSON convention; methodology (Benchmark number + Tracy capture, before
  and after) written down.
- **Group O**: all 5 thread-owning sites audited correct; §7 stays a documented convention (a
  base class would invert destruction the wrong way); RLE-on-palette rejected on shipped-practice
  evidence (LZ4-at-serialization is the recorded future recipe).
- **Regression gate**: 59/59 tests (49 before the pass; +3 dispatcher, +4 octahedral, +3
  quantization), verify-frame both backends, autofly bounded, ASan-clean concurrency stress
  subset. New deps flow into CI automatically via the `Dependencies.cmake`-hash cache keys.

---

**Terrain Fixes & Gameplay pass (Groups Q–X): DONE (2026-09-04).** Full evidence log in
`research/terrain-fixes-log.md`. Headlines:

- **The ribbon bug (Group Q) — TWO stacked causes, both fixed.** (1) §2.2's Chebyshev-CUBE
  design was wrong on Y — the "render distance means Minecraft" citation actually implies
  horizontal-only radius over full-height columns; verified before fixing (196 ready = exactly
  7×7 columns × 4 of 6 band layers with the camera above the band), desired set is now full-band
  columns and the band [-3,2] is the documented generator-derived Y-bound. (2) Deeper and older:
  **the terrain's front-face winding was inverted since Phase 1** (`FrontCounterClockwise` left
  at the default False against extract_mesh's CCW emission) — up-facing triangles were back-face
  culled EVERYWHERE, so all any camera ever saw was steep silhouette slivers, and the 5%
  verify threshold passed on those for two entire passes. Found by an evidence chain (full-column
  counts ✓ → CPU surface-coverage test ✓ → per-draw dump shows chunks drawn-yet-invisible →
  cull kill-switch changes nothing → the ground-level capture shows terrain visible from BELOW).
  One-bit fix: verify fraction 14.3% → **39.4%**, continuous landmass with valleys, standing
  trees, and the ocean rendering for the first time. Verify threshold raised 5% → 25% so this
  class fails loudly forever after.
- **Corrugation (Group R)**: diagnosed by elimination with permanent tests — RAW heightmap
  smooth (max 1-unit step 3.80 vs analytic bound ~8), CPU normals continuous (mean 7.0°/max
  29.5° over 4440 edges), and the contour-stripe artifact itself was the winding bug's grazing
  slivers (gone with it). Remaining appearance is ordinary voxel-scale terracing; possible
  subtle oct8 normal banding stays a named future polish A/B (RG16 normals).
- **Overlay fps/ms (Group S)**: root cause exactly as predicted — slow-EMA fps beside RAW
  instantaneous ms. Both now derive from ONE smoothed frame time (consistent by construction);
  the 2s log prints worst-frame ms.
- **Stutter (Groups T/U)**: per-frame mesh upload budget (`--upload-budget`, default 4,
  0=pre-fix behavior for A/B) + budgeted GPU buffer RELEASE on unload (the godot_voxel-documented
  destroy-churn spike — a boundary crossing dumps a whole trailing face) + nearest-first job
  submission (every surveyed engine orders; single-producer FIFO makes it real). Both subagent
  reports landed and reconciled: the planned FIXED-BUCKET pool was the outlier design — Diligent
  ships `IBufferSuballocator`/`VertexPool`, recorded as the adoption path if post-budget captures
  still show buffer churn (with the freed-region-quarantine caveat to verify first).
- **Walk mode (Group V)**: G toggles fly/walk (`--walk` starts in it); gravity (-32u/s²) +
  analytic ground clamp via `HeightmapGenerator::height_at` (sea-level-clamped — stride across
  water, no swimming v1); 4 new controller physics tests incl. a big-dt tunneling guard;
  `--autofly --walk` asserts zero below-ground frames; ground query verified against the actual
  extracted mesh surface at 5 columns (incl. negative coords — which flushed out the
  under-ocean-column subtlety: submerged terrain isn't meshed, water top is). Jolt named+deferred
  in writing.
- **Trees (Group W)**: deterministic jittered-grid placement (splitmix hash; masks: height
  [1.5,45], slope ≤2.0 — calibrated against the measured mean slope 1.34, not guessed) appended
  into the chunk's own compressed mesh (same PSO/format/culling/budgets; Wood/Leaves materials
  4/5; skip-if-crossing-chunk-top v1). Overlay "objects" count. 3 new tests: determinism,
  spacing/masks, chunk-local bounds.

**Group F (consolidation, tasks 33–35): DONE (2026-09-03), with stated limits.** Task 33 happened
naturally at Group D: streaming + camera + rendering + overlay already run as one `voxel_app` loop
(final gate re-run: 49/49 tests, `--verify-frame` green on Vulkan AND D3D12, `--autofly` bounded).
Task 34: the §9-patch half was verified already-landed during the initial verification pass;
status lines updated now across `PROJECT_BRIEF.md` §11 / `PHASE_1_BRIEF.md` §8 / this log. Task
35: `.github/workflows/ci.yml` wires the four-job matrix — Windows MSVC build+test (full tree),
and Linux ASan+UBSan, TSan, clang-tidy jobs over the headless `-DVOXEL_BUILD_RENDERER=OFF` subset
(a new gate that skips the GPU/window dependency fetches entirely; validated locally: 37/37 tests
headless). TSan-on-Linux is deliberate: it does not exist on this Windows machine by any realistic
path (CLAUDE.md), and the threaded code needing it is exactly the platform-independent subset.
clang-tidy is wired into the build (`VOXEL_CLANG_TIDY` → `CMAKE_CXX_CLANG_TIDY`, `.clang-tidy`
with `WarningsAsErrors: '*'`, tests excluded as non-product code) and **runs clean locally** —
getting there fixed four real findings it caught: a dead `using`, a widening-cast restructure, a
non-locally-guaranteed division-by-zero invariant in `chunk_voxels.cpp` now guarded in place, and
a per-invocation `stop_token` copy. **Stated limit**: the workflow's Linux jobs are validated
structurally and via the local headless build, but have not yet executed on GitHub's own runners —
that first run happens on push, and Windows-runner Diligent build time (~1 h cold) is expected.

Final state: 49/49 tests; `voxel_app --mode vk|d3d12 --verify-frame` and `--autofly` all green.
