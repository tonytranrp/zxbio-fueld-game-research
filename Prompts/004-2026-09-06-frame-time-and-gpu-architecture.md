# Prompt 004 — The frame time: find it, then rebuild the GPU architecture that lost it

**To:** the main coding session (`[CC]`), branch `C++-voxel`
**From:** the side session, 2026-09-06
**Read this whole file before touching anything.**

**Depends on Prompt 002** (the harness — every number below is a scenario result). Independent of
003. This is the largest and highest-risk prompt in the arc, and it is the one the owner asked for
most directly.

---

## 0. What this pass is and is not

**The gap, in the owner's words:** the frame rate *"is way too laggy — flying back and forth and
suddenly it's extremely stuttery"*, and *"I think our game is laggy as hell, I think not making it
above 60fps, well below that, even on a release-mode build."* The target is **above 150 fps**, and
the owner explicitly wants the GPU pipeline reworked for cache and event handling, with the
GigaVoxels paper as the reference for what is achievable.

**One cause is already identified, from reading the code, and it is not subtle.**
`app/src/main.cpp`'s `run_svo` loop:

```cpp
if (!world.building() &&
    world.distance_from_build_center(camera.position) > options.svo.lod_radius * 0.5f) {
    world.request_build(camera.position);
}
```

`lod_radius` defaults to **4.0 m**, so a full world rebuild is requested every **2 metres of camera
movement**. A rebuild is a from-scratch resample and re-encode of the entire 512 m region: measured
**0.6–1.3 s** on three quarters of the hardware threads, producing **200–400 MB** which is then
staged to the GPU at 32 MB/frame — about **12 frames of `UpdateBuffer` traffic**. Default fly speed
is `move_speed = 40 m/s`, and `boost_factor = 4.0` makes it 160 m/s. **At 40 m/s the camera crosses
the 2 m trigger every 50 ms, so builds run continuously, back to back, forever, while you move** —
each one starving the render thread (goal 170 already measured this: 12 of 13 slow frames in a
900-frame walk were `present` stalls of 20–30 ms with a build running) and each one followed by a
12-frame upload burst. That is the stutter, and it is architectural, not a tuning problem.

**The literature is unanimous that the architecture is the problem.**
`research/gpu-voxel-streaming-and-profiling-research.md` §7 collects it; the single most relevant
sentence is GigaVoxels' own, from 2009: *"Even if we were able to iteratively fill the GPU memory in
a brute-force manner, the transfer of 512MB each frame ... already prevents real time performance."*
Nanite's stated principle is *"GPU scene representation persists across frames — sparsely updated
where things change."* HashDAG's entire contribution is mutating a compressed voxel structure
without decompressing it. Aokana streams *"only about 5%"* of its scene into VRAM. **Nothing in the
literature supports rebuild-and-reupload.**

**What this pass turns it into.** A GPU-resident cache of node tiles and bricks with GPU-side LRU
eviction, fed by requests the marcher itself writes during traversal, over a structure whose
addressing is *relative within a block* so a piece of it can be replaced without invalidating the
rest — and a marcher that never waits for data, shading with the coarsest resident level and filing
a request instead. Plus the traversal-side work the same literature quantifies: a coarse start-`t`
pre-pass, and the move from a fullscreen pixel shader to compute with an application-managed work
queue.

**Named outcome.** `voxel_harness --scenario fly_transect` flies the length of the region at speed
and reports: **zero frames over 2× the median**, **p99 under the 150 fps budget**, and **no
"building" or "uploading" cause flags at all**, because nothing is being rebuilt or bulk-uploaded —
only the handful of bricks the camera newly needs.

**What this pass is NOT.** Not the aesthetic (Prompt 005 owns what the surface looks like — though
this prompt's structural changes are what make 005's options affordable, and §4/Group AK-F flags the
one shading consequence). Not terrain (006). Not view distance (007 — but this prompt's throughput
measurement is what 007's region size will be derived from). Not editing (goal 160): the structure
built here should make editing *possible* later, and you should say whether it does, but do not
implement it.

---

## 1. Context to read FIRST, in this order

1. **`CLAUDE.md`** — the build non-negotiables, and specifically: **never measure in
   `windows-debug`** (3.4×–80× slower, measured on this codebase); **shaders load at runtime**, so a
   marcher experiment is an edit-and-relaunch, not a rebuild; **FXC forbids writing a
   runtime-indexed vector component (X3500)**, so a shader can pass on Vulkan and fail on D3D12 —
   test both; **Diligent MUTABLE SRB variables bind exactly once per SRB** and a second `Set` is
   silently ignored in Release, so anything replaced at runtime must be DYNAMIC; and the
   **Vulkan first-timestamp fault** workaround.
2. **`docs/progress.md`** — including *"Decisions that survived contact with evidence."* Two entries
   bear on this prompt: **Group T's storage compression was deliberately gated** (goal 135's 343 MiB
   measurement fit comfortably, so the sparse-grid and SVO/SVDAG phases stayed un-started — *"the
   intended good outcome, not a skipped step"*), and **"don't scale a static world's radius toward a
   headline number without profiling first."** The first is reopened here because the gate has now
   been met on the *svo* path (395 MB re-uploaded every 2 m is a different problem from 343 MiB
   sitting still); say so explicitly.
3. **`docs/goals.md`** — read the group notes and the goals for **AB (the lag, measured)**, **X (GPU
   ray-marched renderer)**, **Y (micro-voxel measurements & follow-ups)**, and **T (storage
   compression, phased)**. And read these five open goals in full, because this prompt closes or
   advances all of them: **157 (palette compression)**, **158 (incremental rebuild)**,
   **161 (floor-truncation quirk)**, **162 (build profile)**, **163 (DAG dedup)**.
4. **`research/gpu-voxel-streaming-and-profiling-research.md`** — **mandatory, and it is the
   specification for Groups AK-B through AK-E.** Read the whole file, and these parts twice:
   - **§1.4** — GigaVoxels' ray-guided feedback mechanism, in the authors' own words: usage stamps
     written by every ray during traversal, a flag for "needs refinement or upload", *"a strategy
     that allows us to avoid any atomic operations in this step"*, then **two stream reductions** to
     order the usage list, then a **compact list** (not per-ray data) to the CPU. And the behavioural
     rule that makes it work: *"if LOD not available → Pick next higher available level in
     Mip-map."*
   - **§2.2** — ESVO's block organization: *"All memory references within a block are relative,
     making it easy to reorganize blocks in memory. This facilitates dynamic memory management
     necessary for out-of-core rendering."* Plus the exact 64-bit child descriptor bit fields.
   - **§2.5** — Aila & Laine's Table 2, reproduced there: **persistent threads took the packet
     kernel from 63.6 → 122.1 Mrays/s (1.92×) and while-while from 88.0 → 135.6 Mrays/s (1.54×) on
     identical traversal code**, and their statement that the gap to optimum *"is not explained by
     memory bandwidth, but rather by previously unidentified inefficiencies in hardware work
     distribution."*
   - **§3.4** — Aokana: *"deep data structures connected by pointers ... are not cache-friendly"*,
     fixed with *"multiple shallow SVDAGs"*, **2–4× over HashDAG above 32K**, **~5% of the scene
     resident**.
   - **§4.6** — SER's measured 38 % → 70 % active threads per warp, and its limitation (RT pipeline
     only).
   - **§4.7** — the coarse start-`t` pre-pass under its four names.
   - **§5** in full — every measurement mechanism with its documented overhead, including the two
     unresolved items you must check yourself (§8.4: does this driver expose
     `VK_KHR_performance_query`? and §8.5's note that the first-timestamp fault has no primary
     source).
   - **§7** — the ranked synthesis. It is the plan for this prompt in eight bullets.
   - **§8** — the open gaps. **§8.1 is the one that matters to you: a stretch of the report was lost
     in transit, covering Teardown/Gustafsson and §4.1–§4.4 (compute vs pixel shader in full,
     including the URLs behind the "47 % from thread-group-ID swizzling, L2 63 %→86 %" figures).
     Re-run that topic with one subagent before you rely on those two numbers** — see rule 11.
5. **`research/micro-voxel-pivot-log.md`** — §2.3 (why the flat layout is what it is), §2.6 (*"whole-
   tree rebuild first, measured, with incremental subtree reuse the named follow-up"* — this prompt
   is that follow-up), and the measured numbers the pivot established.
6. **`research/lin-look-log.md`** — the slow-frame attributor's origin, goal 170's measurements
   (a build on every thread starved `present`; `--svo-upload-mb 8` made the stutter *worse*), and
   goal 164's rule that secondary rays judge LOD from **their own origin**, never the eye's.
7. **`research/chunk-generation-optimization-log.md`** and **`research/engine-hardening-log.md`** —
   the established benchmark methodology (Google Benchmark, `tools/compare.py` Mann-Whitney,
   baselines under `benchmarks/baselines/`, non-const lvalues to `DoNotOptimize`, no LTCG on
   benchmark targets). Extend it; do not contradict it.
8. **The C++ skill reference files** (`~/.claude/skills/cpp-heavy-templates/references/`) —
   **mandatory. This prompt is where the skill earns its place:**
   - **`memory-and-performance.md`** in full. The node/brick pools are the textbook case: read the
     SoA-vs-AoS section, the cache-friendliness section, the `std::pmr` section (rule 30: reach for
     `pmr` before hand-rolling an allocator, and hand-roll a pool only after measuring), and the
     false-sharing note (`hardware_destructive_interference_size`) before putting an atomic anywhere
     near the request queue.
   - **`release-codegen-and-tradeoffs.md`** — §1's four buckets (free / pay-to-win / pay-to-save /
     neutral-to-tooling). **Classify every optimisation in this prompt into a bucket and say which,
     in the log.** Also §5 on `-O2` vs `-O3` (rule 18: promote only after measuring that it does not
     regress the hot path) and the LTO/PGO section — goal 162 ("build profile") is exactly this, and
     rules 17 and 19 tell you where the line is between the dev loop and the shipping build.
   - **`concurrency-and-parallelism.md`** — the request/response path between the marcher, the
     readback and the producer threads. Rule 34 (default to `seq_cst` until you can name the race),
     rule 35 (**never hand-roll an MPMC lock-free structure** — moodycamel is already a dependency),
     rule 33 (`jthread` + `stop_token`).
   - **`templates-and-metaprogramming.md`** §1, §3 (policy-based design — the traversal is a policy
     over "how do I fetch a node", which is exactly what changes when nodes become paged), §5 (type
     erasure).
   - **`modular-architecture.md`** §1–§2 — where the cache lives. It straddles `world/svo` and
     `render/diligent`, which is a dependency-inversion decision, not a file-placement one.
   - **`compile-time-performance.md`** — rule 13, explicit instantiation at the choke points.
   This is the owner's explicit instruction for this arc: *"the prompts must follow strictly and
   read the C++ skill files on how things act and work in order to create a balanced code
   structure."*

---

## 2. The systems you are building on — verified file map

Every claim checked against the file on 2026-09-06.

### The world side

- **`app/src/svo_world.{hpp,cpp}`** — `SvoWorldOptions { seed, voxel_size_log2 = -7,
  root_size_log2 = 9, lod_radius = 4.0f, trees, worker_threads = 0 }`. `request_build(camera)`
  returns false while a build runs; one `jthread` worker at a time; `take_finished()` hands over the
  finished `BrickTree` **by move, once**. `default_build_threads()` is `hw * 3 / 4` with goal 170's
  measurement in the comment. `build_job` constructs a `TerrainSampler`, calls
  `sampler.set_focus(camera, 4.0f * lod_radius)` — a **1/16 m fine height field over a 16 m radius,
  rebuilt every time** — then `build_tree(sampler, geometry, params, &pool, &stats)`.
- **`world/svo/include/world/svo/brick_tree.hpp`** — `BrickTree { TreeGeometry geometry;
  std::vector<uint32> nodes; std::vector<uint32> bricks; uint32 root; }`. Its own comment:
  *"This IS the GPU representation — SvoRenderer uploads `nodes` and `bricks` verbatim into two
  StructuredBuffer<uint>s"* and *"Immutable after construction by design ... the tree is rebuilt
  from the analytic sampler when the camera moves; live editing is the HashDAG-shaped follow-up."*
- **`world/svo/include/world/svo/tree_layout.hpp`** — the encoding, read in full. Header word: bits
  0–7 child mask, 8–9 kind (0 internal / 1 brick / 2 solid), 16–23 material. Layout v2:
  `internal: [header][attributes][child pointer per set mask bit...]`,
  `brick: [header][brick index][attributes]`, `solid: [header]`. **A child slot holds the *word
  offset of the child's header inside the same node array*** — i.e. a **flat global index**, which
  is precisely what ESVO's relative-within-block addressing exists to avoid (research §2.2, §7 item
  2). `node_child_slot(header, octant) = 2 + popcount(mask & ((1<<octant)-1))`. The attribute word
  is int8×3 snorm normal + uint8 coverage. `TreeGeometry` with `voxel_bits() = root - voxel` = **16**
  at the defaults, `max_brick_level() = voxel_bits() - 3` = 13, and **`kMaxVoxelBits = 24`** because
  *"Float has a 24-bit mantissa: integer voxel coordinates and root-normalized positions stay exact
  only while V ≤ 24, and the traversal's fixed stack is sized from this too."*
- **`world/svo/include/world/svo/brick.hpp`** and `kBrickWords = 144u` / `kBrickMaskWords = 16u` in
  the shader — **an 8³ brick is 144 words = 576 bytes: 16 mask words + 128 material words (one
  uint8 per voxel).** 512 voxels in 576 bytes = 1.125 B/voxel, uncompressed, with no palette and no
  deduplication. Teardown ships one byte per voxel *plus a palette* (research §7 item 8).
- **`world/svo/include/world/svo/tree_builder.hpp`** — `BuildParams { lod_center, lod_radius = 4.0,
  uniform_lod, parallel_split_level = 5 }`, and the LOD rule verbatim in the header comment:
  *"A box is refined until its brick voxel edge is <= the target voxel edge at the box's distance
  from `lod_center`: target(d) = max(finest, d * finest / lod_radius)"*. `BuildStats` already carries
  `boxes_classified`, `bricks_sampled`, `bricks_kept`, `solid_leaves`, `padding_words`, `seconds`,
  `classify_seconds`, `fill_seconds`, and per-level histograms. **You already have the build
  instrumentation; use it rather than adding more.**
- **`world/svo/src/ray_trace.cpp`** (369 lines) — the CPU reference. **The 7,000-ray brute-force
  oracle gates every traversal change.** `tools/svo_render` renders with it.

### The render side

- **`render/diligent/include/render/diligent/svo_renderer.hpp`** — `Settings` with `shadows`, `ao`,
  `lod_march`, `sky`, `grain`, `taa`, `lod_quality = 1.0`, `shadow_lod = 4.0`, `ao_lod = 8.0`,
  `ao_radius_px = 32.0`, `smooth_pixels = 6.0`, `grain_amplitude = 0.10`, `taa_blend = 0.125`,
  `debug_view`, `wind`, and `upload_bytes_per_frame = 32 MB` with the comment: *"A whole tree is
  200-400 MB at the shipping default; one synchronous CreateBuffer of that size was the 45-61 ms
  worst frame in every run."* Staged upload API: `begin_upload(BrickTree)` /
  `pump_upload()` / `upload_pending()` / `last_upload_ms()` / `last_upload_frames()`. Already has
  `last_gpu_ms()` (one timestamp pair around march+resolve).
- **`render/diligent/src/svo_renderer.cpp`** (695 lines) — verified details:
  - Node/brick buffers: `USAGE_DEFAULT`, `BIND_SHADER_RESOURCE`, created with `nullptr` initial
    data (*"so creating a 400 MB buffer costs no CPU copy"*), filled by `upload_slice` →
    `ctx->UpdateBuffer(buffer, offset, size, data, RESOURCE_STATE_TRANSITION_MODE_TRANSITION)`.
  - `g_Nodes` and `g_Bricks` are `SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC` **on purpose** — the
    comment records the MUTABLE-binds-once bug that forced it.
  - A **spare buffer pair is kept across swaps** *"so a steady-state swap allocates nothing"*.
  - The draw is `ctx->Draw({3, DRAW_FLAG_VERIFY_ALL, 1})` — **one fullscreen triangle, a pixel
    shader**. Same for the TAA resolve.
- **`render/diligent/shaders/svo_march.psh.hlsl`** (698 lines), read in full. The cost structure:
  - `kMaxIterations = 2048u`, `kMaxLevels = 22u`, a **fixed traversal stack** `uint stack[kMaxLevels]`.
  - **Per pixel: one primary `TraceRay`, plus — on a hit — one shadow `TraceRay` and four AO
    `TraceRay`s.** That is **six traversals per shaded pixel**, and the AO loop is `[unroll]`ed.
    Secondary rays use `kSecondaryCoverage = 0.35` and LOD multipliers `shadow_lod`/`ao_lod` judged
    from their own origin (goal 164).
  - `ReadAttributes` walks *up* the stack from the deepest attribute-carrying entry while the
    ancestor spans less than `t * smoothPixelAngle` — an extra dependent load chain per hit.
  - `Comp`/`CompI`/`AxisMask` exist because **FXC rejects a runtime-indexed vector component write
    (X3500)**; every CPU-reference `v[axis] = ...` became a masked write here.
  - Two `ValueNoise` calls for the albedo mottle, plus `Hash3` for the grain, plus `ShadeWater`'s
    Gerstner sum and two more `ValueNoise` calls on water.
- **`render/diligent/shaders/svo_taa.psh.hlsl`** — distance-reprojected TAA, `taa_blend = 0.125`
  (eight-frame history). Research §3.5 notes NAADF 2026 uses a **32-frame** history for exactly
  this content class.
- **`render/diligent/src/post_process.cpp`** — RGBA16F → DiligentFX bloom → soft-knee tonemap, with
  two documented, empirically-found constraints (construct `PostProcessor` **before** the renderers;
  bloom needs one warm-up `PostFXContext::Execute`).

### The frame loop and what it already tells you

- **`app/src/main.cpp`'s `run_svo`** — the rebuild trigger quoted in §0; `FramePhases { upload,
  camera, render, post, overlay, present }` plus cause flags `{ swapped, uploading, building,
  refreshed }`; the slow-frame attributor at `kSlowFrameMs = 20.0` logging the full breakdown; an
  exit summary counting slow frames by cause (`total`, `whileUploading`, `onSwap`, `whileBuilding`,
  `other`). **Prompt 002 lifts this into `dev/telemetry`; if 002 is done, use that.**
- **`adopt_finished()`** takes the finished tree and calls `begin_upload`, then pumps one slice per
  frame; logs `svo tree #N: ... bricks, ... internal, ... solid leaves, ... MB, build ...s (sampler
  ...s, ... classified, ... bricks sampled), staged upload ... ms over ... frames, ... trees`.

### The measured baseline (from `docs/progress.md` and the viewed capture `svo_ground_hilltop.png`)

| | value |
|---|---|
| ground level, shadows + AO | **76.0 fps (13.15 ms)** |
| vsync panoramic | 155–159 fps (**panel-capped at 165 Hz FIFO_RELAXED — fps is useless as a regression signal**) |
| GPU march + resolve | **3.2–6.3 ms** |
| bricks / MB / levels | **657,034 / 395.2 MB / 14** |
| internal / solid nodes | 246,046 / 804,157 |
| build | **1.30 s** |
| staged upload | 60.6 ms, 1 upload |
| tree GPU memory | 376.9 MiB (peak 376.9) of 7180 MiB VRAM |
| world ready (svo) | 0.56 s |
| trees placed | 264 |
| `--verify-frame` contrast | 34.7 % (vk) / 34.6 % (d3d12), threshold 6 % |
| tests | **177** |

**Read that table twice.** 76 fps at 13.15 ms with only 3.2–6.3 ms of GPU march+resolve means
**roughly half the frame is not the marcher.** Before you optimise the marcher, find out what the
other 7–10 ms is. That is Group AK-A, and it comes first for exactly this reason.

---

## 3. Standing rules for this pass

Prompt 002 §3 in full. The ones that bite hardest here:

1. **Measure before you change anything, and measure after.** Every task in this prompt has a
   number. A change with no before/after pair is not done. RelWithDebInfo or Release only.
2. **CPU reference first, GPU mirror second, oracle always.** `ray_trace.cpp` and
   `svo_march.psh.hlsl` change **together**, and the **7,000-ray brute-force oracle must stay at
   0/7,000 failures** through every traversal change in this prompt. This is the single hardest
   constraint here and it is non-negotiable: a traversal that is fast and wrong is worthless, and
   this repo has the tooling (`tools/svo_render`, `--lod-center`, the per-level histogram, the
   column probe) to catch it in minutes on the CPU instead of hours on the GPU.
3. **A visual change is verified by a viewed capture on both backends.** Restructuring the data is a
   visual change: an addressing bug shows up as corrupt geometry, not as a compile error.
4. **Both backends, every time.** FXC's X3500 and Diligent's MUTABLE-binds-once are the two traps
   this file already documents; a paged/cached structure will replace resources at runtime, which is
   exactly the MUTABLE trap's territory.
5. **Tests green; new systems get new tests.** 177 is the floor.
6. **Determinism**: same seed → same world. A GPU-resident cache makes the *resident set*
   view-dependent, which is fine — but the *content* of any given brick must remain a pure function
   of (seed, position, level). Assert that.
7. **Materials are components.** Palette compression (goal 157) touches material encoding; it goes
   through the registry.
8. **No new dependencies without a written case.** Candidates here: a GPU stream-compaction
   primitive, and FLIP for image comparison (Prompt 002's territory). `concurrentqueue` is already a
   dependency — use it rather than hand-rolling (skill rule 35).
9. **Read the goals.md group notes** (AB, X, Y, T) before closing 157, 158, 161, 162 or 163.
10. **Use your skills** — §1.8's reference files by name, and classify every optimisation into
    `release-codegen-and-tradeoffs.md` §1's four buckets.
11. **Delegate web research to one or two read-only subagents, never inline.** There is one
    *specific, named* gap to close before you rely on it:
    **`research/gpu-voxel-streaming-and-profiling-research.md` §8.1** — a stretch of the source
    report was lost, covering (a) Teardown / Dennis Gustafsson's shipped voxel renderer and (b) the
    full compute-shader-vs-pixel-shader comparison including the primary URLs behind
    *"thread-group-ID swizzling: 47 % on a fullscreen pass, L2 63 % → 86 %"*. Spawn **one** subagent
    for that, and optionally **one** for §8.6 (the 2026 Teardown talks — note that YouTube returns
    boilerplate to fetchers, so it must recover text via search-indexed mirrors, video descriptions,
    or Reddit crossposts). Keep coding while they run; append their findings to the same research
    file with the same CONFIRMED/INFERENCE labels.
12. **Commit and push per group** — eight or nine commits here. MSVC has no UBSan, and this prompt
    is *dense* with index arithmetic over large arrays: exactly the defect class the Linux CI leg
    caught last pass (the tree-species hash overflow, invisible locally) and this machine cannot.
    **Push early. Do not batch.**
13. **`git add` explicit paths, never `git add -A`.**

---

## 4. Task groups

### Group AK-A — Find the frame time before optimising it (goals 244–248)

76 fps at 13.15 ms with 3.2–6.3 ms of GPU march. **Half the frame is unaccounted for.** Everything
downstream is guesswork until this group is done.

**244. Account for every millisecond of the worst frame.**
Using Prompt 002's per-pass GPU timing (goal 220) and the frame report (215), produce a full
attribution for `stress_pose` and `walk_hillside`: CPU per phase, GPU per pass, and the gap. The
phases must sum to the wall time within 1% (Prompt 002 goal 215 already asserts this); if they
don't, the gap is the finding.
Then answer, with numbers, in this order: **is the frame CPU-bound, GPU-bound, present-bound, or
driver-bound?** Note that vsync at 165 Hz FIFO_RELAXED will *manufacture* `present` time — run the
measurement with vsync off (or in a mode where it is off) and say so.
**Check**: the attribution table for both scenarios, both backends, in the log, with the verdict
stated in one sentence. If the answer is "present, because of vsync", say that and re-measure
unlocked — a 13.15 ms frame at a 165 Hz panel is 2.2 vsync intervals, which is suspicious on its
face and must be ruled in or out before anything else.

**245. Count what the marcher actually does.**
Add to the debug surface: mean and max primary-ray steps, and the **total traversals per frame**
(primary + shadow + AO). The `steps` debug view already exists; read it back rather than adding a
second mechanism. On D3D12 also read `D3D12_QUERY_TYPE_PIPELINE_STATISTICS`'s **`PSInvocations`**
(research §5(c)) — for a fullscreen marcher it is the ground truth for how many pixels ran the
traversal, and it is nearly free.
**Check**: for `stress_pose`, state: pixels shaded, mean/max primary steps, traversals per frame,
and derived **traversals per second**. Compare that number to the literature's Mrays/s figures
(research §2.1: ESVO 60.9 M primary rays/s at 5 mm on a GTX 285; §3.1: SVDAG 170/240 MRays/s on a
GTX 680). **If this engine is far below those on a 4070, the marcher is the problem; if it is
comparable, the frame time is elsewhere.** That comparison is the single most informative number in
this group — make it.

**246. Price the secondary rays.**
One shadow + four AO traversals per shaded pixel is **6× the primary ray count**. Measure the GPU
time of `stress_pose` with: shadows+AO on (default), `--no-ao`, `--no-shadows`, and both off.
**Check**: four GPU ms numbers, both backends, and the derived per-ray-class cost. State what
fraction of the marcher's time is secondary rays. If it is the majority — which the arithmetic
suggests — that reframes the whole optimisation problem and should be said plainly.

**247. Price the rebuild storm.**
Run `fly_transect` and report: rebuilds triggered, total seconds spent building, MB uploaded, frames
with a `building` flag, frames with an `uploading` flag, slow frames by cause, and the frame-time
p50/p95/p99/max. Then run it again with the trigger artificially disabled (a debug flag that
suppresses rebuilds entirely — the world will go stale, which is the point) and report the same
numbers.
**Check**: both columns in the log. **This is the measurement that proves the diagnosis in §0**, and
it is the number Group AK-B is judged against. If suppressing rebuilds does *not* substantially fix
the stutter, the diagnosis is wrong and you should say so loudly before building anything.

**248. Establish the yardstick and the budget.**
From Prompt 002's `throughput_ramp` (goal 219) plus the above, state: the current
voxels/bricks/traversals this GPU sustains at 150 fps and at 60 fps, and a **frame budget** for
150 fps at 1080p — a millisecond allocation across march, secondary rays, TAA, post, overlay,
present, CPU sim and upload. Every later task spends from it.
Add a **variance target**, because the owner's complaint is stutter, not average frame rate:
**no frame over 2× the median**, and p99 within the budget. Average fps is not the goal; a smooth
frame is.
**Check**: the budget table and the variance target in the log and in `docs/`. Every subsequent task
in this prompt reports against it.

---

### Group AK-B — Stop the rebuild storm (goals 249–253)

The cheap, immediate, large win. Do this before the architecture work: it is a day's work and it is
what the owner will feel first.

**249. Decouple the rebuild trigger from `lod_radius`.**
`> lod_radius * 0.5f` is a coincidence, not a design: it ties rebuild frequency to a *detail*
parameter. Replace it with an explicit, named trigger distance with **hysteresis** (rebuild when the
camera is more than R_out from the build centre; do not re-trigger until it settles), and a
**minimum interval** so a build cannot start while the previous one's upload is still in flight.
**Check**: `fly_transect`'s rebuild count before and after, and the frame-time percentiles before
and after. State the chosen R_out and interval and why. This is a stopgap and should be labelled as
one in the log — AK-C/D replace it.

**250. Don't rebuild while the camera is moving fast.**
A rebuild centred on where the camera *was* is worthless by the time it lands. Either predict the
centre from velocity, or defer the rebuild until the camera slows. Both are defensible; pick one and
say why.
**Check**: `fly_transect` shows zero rebuilds completing more than one trigger-distance behind the
camera (log the distance between build centre and camera position at adopt time, per rebuild, and
report the distribution).

**251. Stop rebuilding the fine height field every time.**
`build_job` calls `sampler.set_focus(camera, 4.0f * lod_radius)`, which builds a **1/16 m** height
field over a 16 m radius from scratch on every build. `set_focus`'s own comment explains why it
exists (the region-wide 0.5 m field's margin was ~24 brick layers thick at 6.25 cm bricks, making
the builder sample ~7 bricks for every one it kept). Cache and reuse it across builds when the focus
region overlaps.
**Check**: `sampler_seconds` from the `svo tree #N` log line, before and after, over five
consecutive rebuilds along a flight path. State the reuse hit rate.

**252. Goal 161 — the floor-truncation quirk.**
It is an open goal in the region-snapping arithmetic (`geometry_for` floors to 8 m). Read the goal's
own note, fix it or state why not, and make sure whatever you do here does not make it worse — the
rebuild trigger and the region snap interact.
**Check**: as filed in `docs/goals.md`.

**253. Report the stopgap honestly.**
State what AK-B bought (frame-time percentiles, rebuild count, MB/s of upload traffic) and what it
did **not** fix: the world still goes stale between rebuilds, and the fundamental "whole tree, all
at once" cost is still there.
**Check**: the numbers and the limitation both in the log, and a viewed capture showing the staleness
if it is visible (fly 20 m and capture — is the near-camera detail visibly coarser than after a
rebuild lands? Answer with the image).

---

### Group AK-C — Make the structure streamable (goals 254–259)

**This is the prerequisite for everything in AK-D**, and research §7 item 2 puts it first for that
reason. It is also the riskiest group, because it changes the encoding the oracle guards.

**254. Design the paged, relative-addressed layout, on paper, in the log, before writing code.**
Per ESVO (research §2.2): *"All memory references within a block are relative, making it easy to
reorganize blocks in memory. This facilitates dynamic memory management necessary for out-of-core
rendering."* Today's child slot is a flat global word offset into one `std::vector<uint32>`, so
nothing can move.
Design: page size, what a page contains (child descriptors + attributes, or those plus bricks), how
a reference inside a page is encoded, how a reference *across* pages is encoded (ESVO's `far` bit +
a per-block far-pointer table is the published answer), and how a page is relocated. Write the
encoding down with bit fields, the way `tree_layout.hpp` already documents its own.
**Also decide the shallow-tree question here** (research §3.4, §7 item 3): Aokana's answer to a deep
pointer-chasing tree is *"multiple shallow SVDAGs"*. This engine's tree is **16 levels**; every
primary ray pays up to 16 dependent, cache-missing loads before reaching a brick. A grid of shallow
trees (e.g. 32 m regions × 12 levels) collapses that chain **and** makes each region independently
streamable and rebuildable — which also solves AK-B's staleness properly. **Ranking these two
(paged single tree vs grid of shallow trees) is the central design decision of this prompt.** My
read, for you to check rather than accept: **do both, shallow-grid first**, because the grid is what
turns "rebuild the world" into "rebuild one 32 m cell" — a ~256× reduction in rebuild cost for the
same camera motion — and it is simpler to get right than paging a single 16-level tree. Paging then
applies within a cell.
**Check**: the design, the bit fields, the ranking with the losers' reasons, and the arithmetic
(levels per cell, cells per region, V per cell against `kMaxVoxelBits = 24`) all in the log before
any code. If you rank differently, one paragraph of why.

**255. Implement it in the CPU reference first, and pass the oracle.**
`ray_trace.cpp` gets the new addressing. `tools/svo_render` renders with it. The **7,000-ray
brute-force oracle must be 0/7,000** before the shader is touched.
**Check**: oracle 0/7,000. `tools/svo_render` produces a frame that is *visually identical* to the
pre-change frame from the same pose and seed (compare with Prompt 002's image metric; state the
distance and the noise floor). The per-level brick histogram is unchanged, or the difference is
explained.

**256. Mirror it in the shader, both backends.**
Then and only then. Remember X3500 and the fixed stack (`kMaxLevels = 22`, sized from
`kMaxVoxelBits`) — a shallower tree makes the stack smaller, which is free register pressure relief;
say how much.
**Check**: viewed captures from three poses, both backends, compared to the pre-change goldens.
`--verify-frame` unchanged within the noise floor. GPU march ms before and after — **a shallower
tree should be measurably faster on its own, from the shortened dependent-load chain; report the
number** (this is the first direct test of research §3.4's claim on this hardware).

**257. Per-cell rebuild replaces whole-world rebuild (closes goal 158).**
With a grid of cells, a camera move dirties a handful of cells, not the world. Rebuild those, upload
those. This is goal 158 ("incremental rebuild"), open since the pivot.
**Check**: `fly_transect` reports **MB uploaded per second of flight**, before and after — the
before column is ~400 MB per 2 m; state the after. Zero frames with a `building` cause flag lasting
longer than one frame. The frame-time percentiles and the variance target from 248.

**258. Shrink the payload (advances goals 157 and 163).**
A brick is **576 bytes for 512 voxels**, uncompressed. Two published levers, both open goals here:
- **157, palette compression**: most bricks contain two or three materials. Teardown ships one byte
  per voxel *plus a palette* (research §7 item 8). A per-brick palette with 2/4-bit indices is a
  2–4× cut on the dominant term.
- **163, DAG dedup**: SVDAG gets *"19 billion voxels in 945 MB"* where an SVO needs 5.1 GB, and
  SSVDAG *"100 billion voxels in <575 MB at 0.048 bits/voxel"* with *"a tracing overhead of less than
  15%"* (research §3.1, §3.2). Terrain is highly self-similar — this is exactly its use case.
**Also: do not add an apron.** Research §1.3: a one-voxel apron on an 8³ brick is a **1.95× memory
blow-up** (395 MB → ~770 MB here), and Kämpe et al. measured *"nearly 1024 MB for a 512³ sparse
voxel octree with materials"* from exactly that duplication. If Prompt 005 wants filtered brick
sampling, the corner-centred 3³ trick or explicit neighbour fetches are the published alternatives.
**Check**: MB for the default pose before and after each lever, separately, plus the GPU march ms
change (dedup costs tracing time — measure it, don't assume the 15% transfers). Determinism
preserved. The oracle at 0/7,000. **If a lever costs more tracing time than it saves in bandwidth,
say so with both numbers and leave it off** — that is a completed goal with a negative result.

**259. Goal 162 — the build profile.**
An open goal, and `release-codegen-and-tradeoffs.md` §1 and the LTO/PGO sections are its
specification. Decide the shipping build's codegen: `-O2` vs `-O3` (skill rule 18: promote only
after measuring the hot path), LTO for the release build only (rule 17), PGO if a representative
training workload exists — and a harness scenario now *is* one, which is new since the goal was
filed. Classify each into a bucket.
**Check**: measured frame time and build time for each configuration on `stress_pose`, in a table,
with the chosen configuration and its bucket. `CMakePresets.json` updated if the choice changes.
State explicitly whether PGO was worth it, with the number.

---

### Group AK-D — The resident cache and ray-guided streaming (goals 260–265)

The GigaVoxels architecture, from research §1.4 and §7 item 6. **Only start this after AK-C's
oracle is green**, because it changes what a "missing" node means during traversal.

**260. A fixed-size GPU node pool and brick pool with slot allocation.**
Fixed-capacity pools sized from a budget (state the budget and where it came from — VRAM is 7180 MiB
and the current tree uses 376.9 MiB, so there is room, but the *point* is a bounded footprint
independent of world size). Pages/bricks occupy slots; a free list and an occupancy map live
alongside.
**Check**: the pools allocate once at startup and never grow — assert it. Resident MB is constant
across a full `fly_transect`, and equal to the budget (report both). Memory does not grow across 60 s
of flight.

**261. The marcher marks usage and files requests.**
Per research §1.4, verbatim in the authors' words: during traversal each ray *"activates the usage
stamps of the elements that are visited"* and *"we also set a flag that indicates whether or not a
refinement or data upload is needed"* — and *"It is possible to employ a strategy that allows us to
avoid any atomic operations in this step."* Find that strategy (a write of the current frame index
into a per-slot stamp is idempotent, so concurrent writes of the same value need no atomic — check
that reasoning yourself before relying on it) and use it.
**Check**: a scenario in a deliberately under-resident state (pools shrunk so most of the view is
missing) produces a request set whose size and content are stable across two identical runs. The
GPU cost of the marking, measured: march ms with marking on and off. **If marking costs more than
5% of the march, the encoding is wrong** — say so and fix it rather than accepting it.

**262. GPU-side LRU with stream compaction.**
Per research §1.4: after all rays finish, loop over the usage stamps and perform *"two stream
reductions, to separate all elements in the usage list that were used in the current frame from the
others"*, then concatenate so the least-recently-used land at the front. Then transfer a **compact
list** to the CPU — *"we reduce the throughput of information from the GPU to the CPU which is
crucial"* — not per-ray data.
This needs a compute shader and a stream-compaction primitive. Write the case if you add a
dependency for it (rule 8); a work-efficient prefix sum is a well-understood ~100-line compute
shader and this repo's culture is to write down the choice.
**Check**: the compaction is correct — a unit test against a CPU reference over 10,000 random usage
patterns. The readback size per frame, measured, in KB (compare it to the 400 MB/2 m of the old
architecture — that ratio is the headline number of this whole prompt). The compaction's GPU cost,
measured.

**263. Never stall a ray: shade with the coarsest resident level.**
The behavioural rule that makes the whole design work, verbatim from research §1.4: *"if LOD not
available → Pick next higher available level in Mip-map."* The renderer never waits and never shows
a hole; it degrades and files a request. This engine's LOD-early-out already knows how to shade an
internal node from its attribute word — that is the same operation.
**Check**: a scenario that teleports the camera to a completely un-resident region and captures the
next 30 frames as a sequence. **Viewed.** Frame 1 must be coarse-but-complete (no holes, no black,
no sky where terrain should be), and detail must converge over the following frames. Report how
many frames convergence took, and whether any frame exceeded the budget. **This capture sequence is
the single most convincing artefact this prompt can produce; make it good.**

**264. The producer side, and the starvation failure mode.**
The CPU (or a compute pass) fills requested slots. Research §1.9(a) is a warning from the original
author, eight years later: GigaVoxels DP (HPG 2024) reports a **2× gain** purely from fixing
*"synchronization and starvation of GPU cores"* in on-demand production. Design for that from the
start: bound the requests serviced per frame, keep the producer off the render thread's critical
path, and follow the existing `SvoWorld` worker+pool pattern rather than inventing a second one
(skill rules 33–35; `concurrentqueue` is already a dependency).
**Check**: `fly_transect` with **zero frames whose `present` phase exceeds the budget while
production is running** — that is the exact failure goal 170 measured (12 of 13 slow frames were
`present` stalls with a build running). Report the requests-per-frame distribution and the service
latency (frames from request to resident).

**265. Retire the staged bulk upload.**
`begin_upload`/`pump_upload`/`upload_bytes_per_frame` and the spare-buffer-pair machinery exist to
manage a 400 MB transfer that should no longer happen. Remove or repurpose them. Keep
`--upload-budget` as a knob on the *new* path if it still means something; delete it if it doesn't,
and say so.
**Check**: `grep` finds no path that uploads a whole tree. `fly_transect`'s slow-frame summary
reports **zero** `onSwap` and **zero** `whileUploading` frames. The MB/s of PCIe traffic during
flight, measured, against the 400 MB-per-2 m baseline.

---

### Group AK-E — The marcher itself (goals 266–270)

**266. A coarse start-`t` pre-pass.**
Research §4.7: four independent shipped or published systems do this under different names — ESVO's
beam optimization, Teardown's per-object linear-depth manual early-out with front-to-back ordering,
Aokana's Hi-Z + visibility buffer, GigaVoxels' proxy geometry with Early-Z. Render a low-resolution
pass whose per-tile conservative depth seeds a starting `t` for the full-resolution rays.
**Note the honest gap** (research §2.4/§8.2): ESVO's own beam resolution and measured speedup could
**not** be confirmed from primary sources, and two contradictory numbers circulate. **Do not cite a
speedup figure you did not measure.** Pick a tile size, measure, and report *your* number.
The marcher already writes hit distance to a second target and `SV_Depth` — you have the machinery.
`TraceRay` already takes a `tOffset` parameter (currently 0 for every ray) *"added to the distance
before the LOD test"* — read that code before designing, it may be exactly the hook you need.
**Check**: mean primary-ray steps and GPU march ms before and after, on three poses (panoramic,
ground, macro), both backends. **Correctness is the risk**: a non-conservative seed skips real
geometry. The oracle at 0/7,000, plus a viewed capture at a grazing angle (the pose class where a
conservative bound is tightest) compared against the golden.

**267. Move the march to compute, and measure it honestly.**
Research §7 item 4 and §2.5. Two things a fullscreen pixel shader cannot do:
- **Thread-group-ID swizzling** for L2 locality — reported at **47 % on a fullscreen pass with L2
  hit rate 63 % → 86 %** (research §7 item 4; **the primary URL for this was in the lost section —
  re-verify it via the subagent in rule 11 before citing it**).
- **Persistent threads** with an application-managed work queue — Aila & Laine's Table 2, reproduced
  in research §2.5: **63.6 → 122.1 Mrays/s (1.92×)** and **88.0 → 135.6 Mrays/s (1.54×)** on
  *identical traversal code*, because *"most of the gap is not explained by memory bandwidth, but
  rather by previously unidentified inefficiencies in hardware work distribution."*
Port the march to a compute dispatch writing colour + distance + depth, keep the pixel-shader path
behind a flag for A/B, then add swizzling, then add a persistent-thread work queue — **three
separate measurements, in that order**, so you know which one paid.
**Check**: four GPU march ms numbers (pixel shader / compute / compute+swizzle /
compute+swizzle+persistent) on `stress_pose`, both backends, plus warp-occupancy or active-threads
data if goal 271's counter access makes it available. Images identical within the noise floor at
every step (assert it — a compute port that changes the image has a bug). **If compute is not
faster, that is a real and publishable-in-the-log negative result: report it with the numbers and
keep the pixel shader.** Note that `SV_Depth` from compute needs a different composition path with
the post chain — check `post_process.cpp`'s construction-order constraint before assuming it is
free.

**268. Cheapen the secondary rays with what the structure already knows.**
From 246 you will know what fraction of the marcher is the five secondary traversals. Options, to
rank and measure: fewer AO rays with better distribution; AO from the node's own **coverage**
attribute (already stored, already used for the LOD early-out) instead of tracing; a shadow ray
budget that varies with `cubePixels`; reusing one traversal's stack for the next ray. Goal 164's
rule stands: secondary rays judge LOD from **their own** origin.
**Check**: GPU ms and a viewed side-by-side for each option tried, both backends. The shadow rings
(`--debug-view lit`) and the AO term (`--debug-view ao`) must not regress — capture both and compare
to `lin_shadow_rings_after_cpu.png`. State what you kept and what each option cost in image quality,
honestly.

**269. `kMaxIterations`, the stack, and register pressure.**
`kMaxIterations = 2048` and `uint stack[kMaxLevels]` with `kMaxLevels = 22`. With a shallow-grid
tree (254) both shrink. Register pressure and occupancy are the mechanism by which that matters —
research §4.6 quotes NVIDIA reducing live state from **222 → 84 bytes per thread** as half of a
24 % win. Measure the actual iteration distribution (what is the 99th percentile? is 2048 ever
approached?) and size the constants from data.
**Check**: the measured iteration distribution, the new constants and their justification, and the
GPU ms change. If counter access exists (271), report registers per thread and occupancy before and
after.

**270. TAA history length, revisited.**
`taa_blend = 0.125` is an eight-frame history. Research §3.5: NAADF 2026 *"adapt[s] temporal
antialiasing (TAA) to retain the last 32 frames rather than a single history buffer ... since the
discretized voxel structure requires much less memory to store the quantized positions and normals
of ray bounces"*, for exactly this content class and exactly the artefacts this engine has
(*"flickering, blurring, ghosting, and aliasing ... especially important in voxel worlds with sharp
edges"*).
**This is a probe, not a commitment** — Prompt 005 owns the aesthetic. Measure the cost and capture
the result; hand the decision to 005 if it is a quality question rather than a performance one.
**Check**: GPU ms and memory for the longer history, plus a viewed slow-pan sequence assessing
ghosting. A written handoff note for Prompt 005 either way.

---

### Group AK-F — Measurement, counters, and the standing regression gate (goals 271–275)

**271. Resolve the two unverified measurement questions on this machine.**
Research §8.4: **does this NVIDIA driver expose `VK_KHR_performance_query`?** The database page
truncated before the NVIDIA entries and every implementation hit was Mesa. Run `vulkaninfo` and
answer it. Research §8.5: the **first-timestamp fault has no primary source** — the workaround
(skip two frames) stands, but record that it is undocumented driver behaviour so nobody later
"fixes" it.
Then decide the counter path per Prompt 002 goal 222: **NVIDIA Nsight Perf SDK is documented to
collect SM occupancy, shader latency (warp stall) reasons, L1/L2 and VRAM throughput
programmatically from inside the application, with custom triggers, and NVIDIA markets it for CI
perf-regression gating** (research §5(d), verbatim). It needs the `ERR_NVGPUCTRPERM` counter
permission, so it is a dev/CI tool, not a retail feature.
**Check**: the `vulkaninfo` answer in the log. The counter decision with its reasoning. Warp-stall
and L1/L2 hit-rate numbers for `stress_pose` **actually collected** — those are the evidence for
267 and 269, and without them those two tasks are guesswork.

**272. The RenderDoc in-app trigger (closes a deferred item and unblocks goals 73/105).**
`CLAUDE.md` lists "in-app RenderDoc trigger (no vendored `renderdoc_app.h`)" as deferred, and goals
73/105 (the floating sliver curtains) as waiting on RenderDoc. Research §6.7 shows the integration
needs no vendoring in the sense that matters: *"The recommended way to access the RenderDoc API is
to passively check if the module is loaded, and use the API if it is... When your program is
launched independently it will see that the RenderDoc module is not present and safely fall back."*
`GetModuleHandleA("renderdoc.dll")` → `GetProcAddress(..., "RENDERDOC_GetAPI")` →
`StartFrameCapture`/`EndFrameCapture`. Zero cost when RenderDoc is absent.
Wire it to the existing `--dump-every` / capture-point trigger so a harness scenario can capture the
exact frame an artefact appears on, unattended.
**Check**: a scenario run under RenderDoc produces a capture at the requested frame; the same
scenario run without RenderDoc is byte-identical in output and unaffected in frame time. Then
**use it**: capture the sliver-curtain frame (`research/water-foliage-design.md` has the repro) and
report what the capture shows. Even "the capture shows X, which does not explain it" is progress on
a defect that has been open for three passes.

**273. Frame-time regression as a gate.**
Per research §6.8: assert on a **percentile across a fixed camera path** from GPU timestamps, not on
a mean and not on fps — this machine's 165 Hz FIFO_RELAXED panel caps fps at 155–159, which makes
fps useless as a regression signal. Wire the budget from 248 and the variance target into harness
assertions on three scenarios, and commit the baselines.
**Check**: the gate passes at the end of this prompt and fails when the budget is deliberately
tightened by 20%. Registered in `ctest -L scenario`. In CI where a GPU exists; if the runner has no
GPU, say so and gate only the CPU-side numbers.

**274. The throughput answer, restated.**
Re-run Prompt 002's `throughput_ramp` on the finished architecture and answer the owner's actual
question: **how many voxels can this GPU render, at what detail, at 150+ fps?** Report resident
bricks, resident MB, voxels represented, traversals/second, and the rung at which 150 fps breaks —
before and after this whole prompt. Compare traversals/second to the literature (ESVO 60.9 M primary
rays/s at 5 mm on a GTX 285; SVDAG 170/240 MRays/s on a GTX 680; Aokana `[UNVERIFIED]` ~6 ms/frame
at 64K resolution on a 3060 Ti).
**Check**: the table, both columns, in the log and in `docs/progress.md`. **This is the number
Prompt 007's view distance will be derived from**, so state it in the form 007 needs: the maximum
resident detail at 150 fps.

**275. What this pass did not do.**
Record with a reason and a follow-up goal each: editing (goal 160) and whether the new structure
makes it reachable (**answer this explicitly — HashDAG's page + hash-table design is the published
answer and research §3.3 has it**); hardware ray tracing and SER (research §4.6: it needs the RT
pipeline, so it is a bigger change than compute, with a documented 38 %→70 % active-threads prize);
NAADF's in-cell distance-field caches (research §3.5: *"3-5x compared to ... variants of directed
acyclic graphs"*, doubling again to 10× with the AADFs — the highest-leverage unexplored idea found,
with an open-source implementation); anything from research §8's gap list you did not close.
**Check**: the list is in `docs/goals.md` with Checks, and in the log with reasons and the research
sections that specify them.

---

## 5. Sequencing and risk

- **AK-A first, completely, before touching anything.** Half the frame is unaccounted for. The
  vsync question in 244 could change this entire prompt's premise, and it costs one measurement.
- **AK-B second** — a day's work, the largest immediate improvement, and it de-risks everything after
  it by making the engine pleasant to iterate on.
- **AK-C third, and it is the riskiest group in the arc.** The addressing change touches the encoding
  the 7,000-ray oracle guards, and a subtle bug presents as corrupt geometry at distance, which is
  very hard to attribute later. The discipline that saves you is the repo's own: **CPU reference,
  oracle, `tools/svo_render` capture, and only then the shader.** Do not shortcut it. If AK-C's
  oracle will not go green, **stop and report** — AK-D on a broken structure is wasted work, and
  AK-A + AK-B alone would already be a large, shippable win.
- **AK-D fourth**, and it depends entirely on AK-C.
- **AK-E is independent of AK-C/D** and can be interleaved. 266 (the pre-pass) is the best
  cost/benefit task in the group and can be done early.
- **AK-F throughout** — 271 (counter access) should be done early, because 267 and 269 are guesswork
  without it.
- **Commit and push per group.** Eight or nine commits. **Push each one**: this prompt writes dense
  index arithmetic over large arrays, and the Linux CI leg's UBSan is the only place that defect
  class is visible.
- **Named legitimate negative results**: 267 (compute may not beat the pixel shader on this
  hardware), 258's DAG dedup (tracing overhead may exceed the bandwidth saving), 268 (a cheaper AO
  may cost too much image quality), 266's pre-pass (may not pay at this resolution). In every case
  the measured negative with both numbers is a completed goal. **What is not acceptable is an
  unmeasured claim in either direction.**
- If a group is blocked, finish the others in full and say precisely what is blocked and why.

---

## 6. Explicitly out of scope this pass

- **The aesthetic.** What the surface *looks like* — the stipple grain, the moiré's appearance, the
  checkerboard target — is Prompt 005. This prompt may change image quality as a side effect; when
  it does, capture it and hand the judgement to 005 rather than tuning the look here. (Research §1.5
  is relevant to both: Crassin's own open question *"How to pre-filter lighting? Pre-filter Normals —
  How to store them? How to interpolate them?"* is 005's problem, and this prompt's attribute-word
  layout decisions constrain 005's options — so **write down what you constrained**.)
- **Terrain generation.** Prompt 006.
- **Player physics and collision.** Prompt 003 — though note that its octree-backed collision reads
  the tree, so a structural change here affects it. Coordinate: if 003 is done, its query must be
  ported; if not, tell 003's author what changed.
- **View distance, region size, fog, LOD-radius derivation.** Prompt 007, which consumes this
  prompt's throughput number.
- **Trees C4–C6 (190–192), grass (193–195).** Prompt 007.
- **Editing (goal 160)**, HashDAG, hardware ray tracing, SER, NAADF. All recorded in 275 with their
  research sections.
- **Multiplayer, save/load, audio.** Not this arc.

---

## 7. When you are done

1. Write **`research/frame-time-and-gpu-architecture-log.md`**: the AK-A attribution with the
   CPU/GPU/present verdict; the traversals-per-second comparison against the literature; the
   secondary-ray fraction; the rebuild-storm before/after; the AK-C design ranking with the losers'
   reasons and the V arithmetic; the oracle results at every step; every before/after GPU ms; the
   payload-shrink table; the four-way compute comparison; the counter data; the readback-size ratio;
   the final throughput table; the four-bucket classification of every optimisation; and every
   decided-against and every honest negative.
2. Add **Group AK** to `docs/goals.md`, goals 244–275 numbered exactly as above, each `[x]` with its
   Check recorded as performed with numbers. **Mark 157, 158, 161, 162, 163 `[x]` in place** where
   this prompt closed them, each pointing at the goal here that did it — and where one was
   *measured and rejected*, mark it closed with the negative result, not left open.
3. Refresh `docs/progress.md`: a new current-state paragraph (the architecture is now a resident
   cache with ray-guided streaming, and here are the numbers); the architecture map updated; this
   pass's entries added to *"Decisions that survived contact with evidence"* — including the
   reopening of Group T's gate and its justification.
4. Add **`docs/gpu-architecture.md`**: the pools, the request path, the LRU, the page encoding with
   its bit fields, the never-stall rule, the frame budget, and how to measure each of them. This is
   the document that makes the next pass possible without re-reading 700 lines of shader.
5. Update `CLAUDE.md` with operational deltas only: the new flags, the new/removed upload knobs, the
   counter-permission requirement, the `vulkaninfo` answer, the RenderDoc trigger, and the new
   baseline numbers.
6. Update `Prompts/README.md`'s index row for 004.
7. Append the subagent findings from rule 11 to
   `research/gpu-voxel-streaming-and-profiling-research.md`, closing §8.1 (and §8.6 if you ran it).
8. Full suite green, both backends, the budget gate passing, and **the 263 convergence sequence
   viewed and committed** — that capture is the proof that the architecture changed.

---

*Provenance: written by the side session on 2026-09-06 after reading `app/src/main.cpp`'s `run_svo`
loop in full (including the rebuild trigger, `FramePhases` and the slow-frame attributor),
`app/src/svo_world.{hpp,cpp}` in full, `render/diligent/include/render/diligent/svo_renderer.hpp` in
full, `render/diligent/src/svo_renderer.cpp`'s buffer creation, upload, PSO and draw sites,
`render/diligent/shaders/svo_march.psh.hlsl` in full (both the traversal and the shading halves),
`world/svo/include/world/svo/{tree_layout,brick_tree,tree_builder,terrain_sampler}.hpp` in full,
`docs/goals.md` Groups T/X/Y/AB and goals 157–163, `docs/progress.md`'s decided-against list, and
the viewed capture `svo_ground_hilltop.png` (which supplied the 76 fps / 657k bricks / 395.2 MB /
1.30 s build / 60.6 ms upload / 376.9 MiB numbers directly from the overlay). The rebuild-storm
diagnosis is arithmetic on read code: a 2 m trigger against a 40–160 m/s fly speed and a 0.6–1.3 s
build. The architectural direction is `research/gpu-voxel-streaming-and-profiling-research.md`,
produced for this prompt by one read-only web-research subagent, whose §8 records honestly what it
could not confirm — including a stretch lost in transit that rule 11 tasks you with recovering.*
