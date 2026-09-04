# Terrain fixes & gameplay — decision log

Execution log for the Terrain Fixes & Gameplay master task list (Groups Q–X), run 2026-09-04.
Same standard as [`engine-hardening-log.md`](engine-hardening-log.md): every task's check stated
and performed; two research subagents ran (§3's verbatim prompts) with findings verified before
adoption.

## Group Q — the ribbon bug

- **Task 1 — hypothesis verified before the fix, two independent ways.** (a) Arithmetic on the
  pre-fix runs' own logs: with the camera above the terrain band, "196 chunks streamed in" =
  exactly 7×7 columns × **4** of the band's 6 Y layers — the radius-on-Y crop the brief's §0
  predicted (`anchor.y` clamped to the band top ⇒ y ∈ [-1..2]; the two valley layers y=-3,-2
  never loaded, so valley surfaces — which sit in chunk y=-2 when heights reach -64 — were
  simply absent, and the visible remainder was a camera-altitude-following ribbon). (b) A
  `loaded_y_range()` diagnostic (now permanent: printed in the 2s stats line as `chunk-Y a..b`)
  confirms live ranges. Post-fix the same scene loads 294 = 7×7×6 with Y always -3..2.
- **Task 2 — fix**: `ChunkStreamer::tick` now builds the desired set as full-band COLUMNS within
  a horizontal-only Chebyshev radius; unload distance and the app's generation-margin sweep are
  horizontal-only too (a high camera must not sweep away low halo chunks). Camera altitude no
  longer appears anywhere in streaming decisions.
- **Task 3 — Y-bound decision, written**: bounded by config `[y_min, y_max] = [-3, 2]`, which is
  the GENERATOR's vertical extent (kAmplitude = ±64 world-Y ⇒ surface chunks span [-3, 1]) plus
  one chunk of headroom — not unbounded (generation itself is height-bounded so unbounded is
  meaningless) and not a leftover radius. Documented at the config site with an update-together
  note pointing at `kAmplitude`.
- **Task 4 — count sanity**: R=3: 196→294 desired (×1.5 — the two restored layers); R=5 autofly:
  the loaded bound formula was already `(2R+1)²×6×2` (it always assumed 6 band layers), so the
  autofly assertion needed no change and passes with the full band actually loaded now.
- **Task 7 — frustum spot-check**: the existing unit suite already hand-checks
  in-front/behind/near/far/90°-boundary/enclosing/quaternion-orientation cases
  (`render/diligent/tests/test_frustum.cpp`, 5 cases — more than the 3 required); re-run green in
  this pass's suite. §0's reframing stands: the ~half visible-after-culling ratio was healthy.

## Group R — the corrugation bug

- **Task 8 — the deciding diagnostic**: `test_heightmap_smoothness.cpp` (permanent regression
  test) dumps a 512-sample raw height profile to `heightmap_profile.csv` and asserts the max
  1-unit step against the analytic fBm slope bound (stated BEFORE running: ≈8 world units/step
  worst case for kFeatureScale=200, 4 octaves, lacunarity 2, gain 0.5, amplitude 64; a
  per-voxel-frequency bug would swing tens of units). FastNoise2 v1.1.1 API semantics verified
  from the fetched source first: the two 1.0f args in `GenUniformGrid2D` are STEP SIZES (1
  sample/voxel — correct), `SetScale(200)` is feature scale. Verdict recorded below with the
  run.

## Group S — the overlay fps/ms mismatch

- **Task 12 — root cause exactly as §0 guessed**: `stats.fps` was a slow EMA (α=0.05) of
  instantaneous fps while `stats.frame_ms` was the RAW current dt — two different estimators of
  the same quantity, wildly divergent during any hiccup (275 fps beside 31.14 ms = a smoothed
  average beside one stutter frame). Fix: ONE smoothed frame-time is the source of truth;
  `fps = 1000/smoothed_ms` by construction, so the pair can never disagree again. The 2s stats
  line additionally prints the window's WORST frame ms (also Group T task 17's metric).

## Group T — load stutter

- **Task 16 — per-frame upload budget, implemented first** (`--upload-budget N`, default 4,
  0 = the pre-fix unlimited behavior kept selectable so before/after is an A/B on one binary):
  `drain_mesh_completions` commits at most N meshes per tick and leaves the rest IN the locked
  queue — `settled()`, the stats counters, and `mesh_in_flight_` stay truthful with zero extra
  bookkeeping. Default 4/frame ⇒ ≥240 chunks/s floor (godot_voxel's docs record a real
  starvation pathology from a time-denominated budget; a fixed count floor cannot starve).
- **Task 13/14 — Tracy capture**: `tracy-capture` CLI built from the pinned v0.14.1 source
  (needed the same `GIT_EXECUTABLE` pin as the engine — the vendored CPM fetch hits the git-shim
  bug). Capture results recorded below.
- **Task 15 — pool decision, revised by evidence (this is task 21's reconciliation)**: Subagent 1
  found the planned FIXED-BUCKET pool is the outlier design — every real implementation surveyed
  (Sodium's `GlBufferArena` best-fit segment arena with compacting growth, VMA/D3D12MA virtual
  blocks, Diligent's own tools) is a variable-size free-list suballocator — and, decisively,
  **DiligentEngine already ships the facility** (`Graphics/GraphicsTools/interface/
  BufferSuballocator.h` + `VertexPool.h`, compiled into this project's own build tree — verified
  at `_deps/diligentengine-build/.../BufferSuballocator.cpp.obj`). Decision: do NOT hand-roll
  buckets; adopt `IBufferSuballocator`/`IVertexPool` IF the post-budget capture still shows
  buffer create/destroy as material (godot_voxel documents Vulkan buffer DESTRUCTION churn as a
  real main-thread stutter source when flying fast — the unload side, which a budget alone does
  not touch). One caveat the report flags for that adoption: the header does not document
  whether freed suballocations are GPU-lifetime-quarantined — read `BufferSuballocator.cpp`
  first. Group K's allocation tracker keeps working either way (it counts real GPU allocations).
- **Subagent 2 (scheduling)**: findings recorded below when its report lands; Subagent 1's task 3
  already grounds the cheap upgrades — distance-keyed dequeue (godot_voxel's banded priority,
  Minecraft's ticket levels) and Sodium-style check-cancel-on-dequeue as the v1.5 between
  "run-stale-and-discard" (current, deliberate) and true cancellation.

## Group V — gravity & ground

- **Task 22 — scope, restated where the code lives** (spectator_camera.hpp): the camera gains an
  optional WALK mode beside fly — no player model/mesh, just physical presence. G toggles;
  `--walk` starts in it.
- **Task 23 — ground query**: `HeightmapGenerator::height_at(x,z)` (GenSingle2D, same node tree +
  seed as generation ⇒ agrees with the terrain by construction, works for unloaded columns);
  exposed through `ChunkStreamingSystem::ground_height`. Check: `test_ground_query.cpp` compares
  the query against the actual Surface-Nets mesh surface at 4 columns (topmost vertex within 1
  voxel of the column) — tolerance ±2 voxels.
- **Task 24 — physics**: constant gravity (-32 u/s²), explicit integration, hard ground clamp
  with velocity zeroed on contact; walking follows yaw only (staring at the ground doesn't slow
  you); Space/Ctrl inert in walk mode. Checks: rest-exactly-at-ground+eye over a 10s simulated
  fall (never below at ANY step), a 0.5s single-step tunneling test (a stutter frame must not
  integrate through the floor), yaw-only movement at -80° pitch, and a fly-mode regression test.
- **Task 25 — transition**: toggle keeps position and zeroes vertical velocity — entering walk
  mid-air starts a clean fall, leaving it freezes in place; no snap either way (logged mode
  switch).
- **Task 26 — Jolt Physics: named, deferred, in writing.** The analytic column query solves the
  actual current need (a point camera on a heightfield) with zero dependencies. Jolt (MIT,
  production-proven in Horizon Forbidden West, first-class in Godot) is the recorded upgrade
  path, with Group W-style dynamic objects (falling/pushable props) as the first trigger that
  genuinely needs rigid bodies — adopting it now would be a physics engine for one raycast.
- **Task 27 — seams/slopes, mechanical**: `--walk --autofly` drives the camera along the ground
  across chunk boundaries for the whole run while a per-frame assertion counts any frame ending
  below ground+eye; run result below. (Steep slopes: the clamp is instantaneous by design — v1
  walking has no slope limit; documented as intended.)

## THE ACTUAL ROOT CAUSE — inverted winding, found by looking at real frames

The brief's §0 identified one real bug (the Y-band crop) but the ribbons had a SECOND, older
cause the fraction-based checks never caught. The bisection chain, each step a real artifact:

1. Post-Q verify: 294 chunks (7×7×6 ✓ full columns) but the dumped frame STILL showed ribbons —
   Y-crop fixed, ribbons not.
2. New `test_surface_coverage`: CPU meshing produces complete surface coverage for a full column
   stack — geometry exists.
3. Per-draw dump (`VOXEL_DUMP_DRAWS`): y≤0 chunks are uploaded (10–19k indices), have correct
   world AABBs, and are DRAWN un-culled — yet invisible. `VOXEL_NO_CULL` changed nothing
   (culling exonerated; closes Q task 7 twice over).
4. The ground-level capture was the tell: terrain visible FROM BELOW, invisible from above —
   up-facing triangles were being back-face culled. **Phase 1's on-paper winding derivation
   (FrontCounterClockwise=False) was empirically wrong** for extract_mesh's emission order; the
   "terrain" every prior 13–16% verify pass measured was only steep silhouette slivers.
5. One-bit fix (`FrontCounterClockwise = True` in pso_terrain.cpp): fraction 14.3% → **39.4%**,
   and the capture shows a continuous landmass — mountains with mass, valleys, standing trees,
   and the OCEAN rendering for the first time ever.

Consequences applied: the verify threshold is raised 5% → 25% (this bug passed 5% for two full
passes — the check now fails on anything ribbon-shaped again), and the standing lesson is
written at the PSO site: a winding derivation is not verified until a real capture has been
reviewed from both sides. The corrugation (Group R) resolves as: raw noise smooth ✓, CPU normals
smooth ✓, contour-stripe artifact = the winding bug's grazing slivers ✓ gone; what remains is
ordinary voxel-scale terracing (and possibly subtle oct8 normal banding — future polish item,
A/B RG16 normals if it bothers).

Debug tooling added along the way (all env-gated, zero cost when unset): `VOXEL_DUMP_FRAME=<p>`
(PPM capture from the verify readback), `VOXEL_DUMP_DRAWS`, `VOXEL_NO_CULL`,
`VOXEL_ONLY_CHUNK_Y=<n>`.

## A second real find from the A/B: the discard-stale policy fed back on itself

The clean upload-budget A/B exposed an interaction nobody designed for: with drains budgeted,
completions WAIT in the queue; at autofly speed the camera has moved by the time they drain, so
the completion-time `is_desired` check (Phase 1's task-24 "discard stale results" policy)
rejected them; every discard re-queued a fresh mesh job whose 27-chunk snapshot copy runs on the
MAIN thread; the copies collapsed the frame rate; lower fps made even more completions stale —
measured end state: ~1.4fps, 400–500-deep mesh backlog, **zero chunks ever ready**. Two fixes,
both keeping their checks honest: (a) the budget gained a backlog/4 drain floor (a fixed
per-frame count starves when frame rate collapses — the godot_voxel pathology, hit for real);
(b) the completion-time discard is REMOVED — a finished mesh is now applied even if its chunk
left the desired set, because `mark_loaded` on an undesired coord is safe by construction (the
standard R_unload + delay hysteresis unloads it moments later) and applying done work is
strictly cheaper than redoing it. Submit-time abandonment of undesired pending chunks stays
(that one prevents work, not wastes it).

## Appendix — run results

- Verify (294 chunks, full columns): Vulkan 14.3% / D3D12 14.0% pre-winding-fix; Vulkan 39.4%
  post-fix (threshold now 25%).
- Walk mode: `--autofly --walk`, 900 frames across chunk seams: **0 frames below ground**.
- Autofly (walk variant): 420 chunks at exit ≤ 588 bound, 4.4 MiB GPU.
- Trees: 31 objects on screen in the origin scene; overlay "objects" line live.
- Overlay pair visibly consistent in captures: "127.0 fps (7.87 ms)" (1000/7.87 = 127.06).
- Upload budget — the A/B's real value was three found-and-fixed interactions, not the headline
  number: (1) fixed per-frame drain count starves at collapsed frame rates (1700+ backlog) →
  backlog/4 floor; (2) the completion-time discard-stale policy fed back into a re-mesh storm
  (see the section above) → apply-stale + hysteresis cleanup; (3) the GPU-RELEASE budget starved
  the same way (34k undestroyed meshes, **1.1 GiB** GPU by run end) → same floor. Final clean
  runs, all bounds honest and green: budget4 707 ≤ 840 chunks at 9.7 MiB / peak 12.6; budget0
  621 ≤ 840 at 5.3 MiB / peak 9.8; walk-autofly 420 ≤ 462, **0/900 frames below ground**. The
  worst-frame A/B itself is inconclusive at this stress point (both ~0.9–1.2s: a debug build at
  160u/s is GENERATION-saturated — 24 gen threads + main-thread snapshot copies dominate, so the
  upload side isn't the binding constraint there); the budget's target case (upload bursts on an
  otherwise healthy frame) awaits a Release-build capture session. Tracy captures saved:
  `C:\b\stutter_budget0.tracy` (3.4MB), `C:\b\stutter_budget4.tracy` for UI inspection.
