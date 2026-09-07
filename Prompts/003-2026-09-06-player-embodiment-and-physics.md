# Prompt 003 — Player embodiment: one body, no spectator, nothing to clip through

**To:** the main coding session (`[CC]`), branch `C++-voxel`
**From:** the side session, 2026-09-06
**Read this whole file before touching anything.**

**Depends on Prompt 002** (the dev harness). Every Check below is written as a harness scenario.
If 002 is not done, do it first — this prompt's whole verification story is `voxel_harness`.

---

## 0. What this pass is and is not

**The gap, in the owner's own words:** *"when I'm trying out the game I'm still in spectator mode
— I don't know why I can fly around and clip through blocks and such, which is not great. We need
to remove all of those. We stay as a player, which is about the size of a player, which can be
seen on the micro-voxels at which it is small."*

Both halves of that are true and both have a specific cause in the code:

1. **You are in spectator mode because that is the shipped default.**
   `world::player::PlayerState::mode` initialises to `MoveMode::Fly`
   (`world/player/include/world/player/player_state.hpp`). `--walk` opts *in* to the body; `G`
   toggles. So the default experience of the game is the free camera, and the body — which exists,
   is fixed-timestep, has gravity, jump with coyote time and buffering, swimming, eye smoothing and
   view polish, all tested — is the thing you have to ask for.
2. **You clip through blocks because collision is a 16 m cache that a flying camera outruns.**
   `world::collision::TerrainColliderParams::cache_extent = 16.0f`,
   `cache_cell = 1.0f/32.0f` — a 513×513 height grid around the last refresh centre, rebuilt on a
   background thread when the body leaves the *inner half* (8 m). Outside the cache,
   `overlaps_solid` falls back to direct per-point height queries, and **the tree-trunk half of
   the query only knows about trunks that were collected into the current cache**. Default fly
   speed is `move_speed = 40.0f` with `boost_factor = 4.0f` = 160 m/s: at 60 fps that is 2.7 m per
   frame, so the body leaves the inner half in about three frames and can outrun a ~6 ms
   background rebuild that is only *requested* once per frame. On top of that, `--noclip` exists as
   a shipped flag that turns collision off entirely.

**What this pass turns it into.** The game has a body, always. Walking is the default and the only
shipped locomotion. Collision is answered by the world the renderer is actually drawing — the
sparse-brick octree — not by a cache with an edge, so there is no distance at which it stops being
true. Fly and noclip survive as *harness* capabilities for capture and debugging, reachable from a
scenario or a dev flag, not from the shipping default. And the body's scale, physics constants and
feel are re-derived from the locomotion research this repo already holds, instead of from the
mesh-era numbers they were first tuned against.

**Named outcome.** `voxel_app` launches, you are standing on the ground at eye height, you walk,
you cannot pass through anything the renderer draws, at any speed, anywhere in the region — and
a harness scenario proves that mechanically at 1×, 4× and 40× walking speed.

**What this pass is NOT.** Not a combat/inventory/interaction pass. Not an animation pass — there
is no player model to animate, and adding one is not in scope. Not a rendering change. Not a
terrain change.

---

## 1. Context to read FIRST, in this order

1. **`CLAUDE.md`** — build non-negotiables (short path, real MSVC of the matching edition, never
   benchmark in Debug, shaders load at runtime, the heredoc backslash trap).
2. **`docs/progress.md`** — including *"Decisions that survived contact with evidence."* Binding.
3. **`docs/goals.md`** — Group AA (body-vs-world collision) in full, including **goal 173:
   octree-backed collision**, which is the goal this prompt closes. Also Group AD (player physics)
   and its goals 177–182, and the AD group note.
4. **`world/player/README.md`** and **`world/collision/README.md`** — the two modules' own rules.
   The `jump_pressed`-is-an-edge convention is documented there and is load-bearing.
5. **`research/gameplay-pass-log.md`** — how the controller was built and verified last pass, and
   §8/§8b's record of what was left. Note the two bugs it found by running things rather than
   reading: a float-round-trip collision escape on a body resting exactly on a surface, and
   `--autofly` not being part of the simulation it tested. Both are directly relevant here.
6. **`research/locomotion-biomechanics-physics.md`** — **mandatory.** The real numbers for human
   locomotion: walking and running speeds, step length and cadence, the inverted-pendulum
   walk-to-run transition, ground reaction forces, jump takeoff velocities and heights, slope
   effects on speed, and the energetics. This is the evidence base for every constant in
   §4/Group AJ-C. Do not tune a constant by feel when this file has the measured value.
7. **`research/player-movement-in-games.md`** — **mandatory.** What shipped games actually use, and
   why game gravity is conventionally 2–3× Earth's. Read this *with* the biomechanics file: they
   disagree on purpose, and the disagreement is the design decision goal 231 asks you to make
   explicitly instead of inheriting.
8. **`research/human-movement-and-perception-research.md`** — the perception side: what the player
   can actually detect about their own motion, camera-height and eye-height effects, and the
   thresholds for detecting view lag. This bounds how much smoothing and polish is honest.
9. **`research/human-eye-and-vision-research.md`** — Part 5 §5.3 (**motion blur: why the eye does
   not need it**), §5.5 (focus/DOF and why cinematic bokeh reads as *camera*, never as eye), §5.7
   (the explicit engineering recommendations for this engine, marked as recommendation: ACES
   default, crosshair-weighted auto-exposure metering pooled over ~6°, veiling-luminance-aware
   bloom, motion blur OFF for first-person purism, and *"vignette is a poor man's acuity falloff —
   and it's wrong"*). Group AJ-D implements the parts of §5.7 that belong to the player's head
   rather than to the renderer's LOD.
10. **The C++ skill reference files** (`~/.claude/skills/cpp-heavy-templates/references/`) —
    **mandatory, not optional:**
    - `templates-and-metaprogramming.md` §1 (concepts) and §5 (type erasure). `SolidQuery` is
      already a concept; the new octree query must satisfy it without widening it.
    - `modular-architecture.md` §1–§2 — where the new query lives and what it may depend on.
    - `memory-and-performance.md` — for the query's data layout. A collision query called several
      times per sub-step, per tick, is a hot path; read the SoA-vs-AoS and cache-friendliness
      section before choosing a representation.
    - `concurrency-and-parallelism.md` — if any part of the query touches the background build.
      The existing pattern is a worker + completion handoff; do not invent a second one.

---

## 2. The systems you are building on — verified file map

Checked against the files on 2026-09-06.

- **`world/player/include/world/player/player_state.hpp`** — `enum class MoveMode { Fly, Walk }`,
  **`PlayerState::mode = MoveMode::Fly` is the default**; `enum class Stance { Grounded, Airborne,
  Swimming }`; `PlayerState` carries `vertical_velocity`, `coyote_remaining`,
  `jump_buffer_remaining`, `eye_smooth_offset`, `bob_distance`, `landing_dip`, `fov_kick`.
  `PlayerIntent` has `forward/back/left/right/up/down/boost/jump_pressed` — and the comment on
  `up`/`down` says *"fly: rise; swim: kick for the surface. Inert while walking on land."*
  `WorldSense { ground_height, water_surface_y }`. `StepResult { landed, jumped, stepped_up,
  shore_popped, impact_speed }`.
- **`world/player/include/world/player/controller.hpp`** — `step_player<SolidQuery Q>()`, one
  fixed tick, with its order documented in the header comment (horizontal wish → water sense →
  vertical integration → jump → sweep → stance → eye smoothing). **The `MoveMode::Fly` branch
  returns early** after a sweep with `step_height = 0`, zeroing stance, velocity, coyote, buffer
  and eye offset. **The analytic backstop at the end** clamps `eyePosition.y` to
  `sense.ground_height + tuning.eye_height` — the header itself says with collision on this
  *should never fire*, and calls it "the one thing standing between a query gap and a fall through
  the world." That backstop is a symptom of the cache-edge problem, and this prompt is where it
  stops being needed.
- **`world/player/include/world/player/tuning.hpp`** — every constant in one struct:
  `eye_height 1.7`, `body_half_width 0.3`, `body_height 1.75`, **`gravity -32.0` (explicitly
  commented "voxel-scale (not Earth's 9.81)")**, `walk_speed_factor 0.25`, `boost_factor 4.0`,
  `step_height 0.55` (a mesh-era relic; `kSvoStepHeight = 0.04` is the svo path's smoothing
  budget), `jump_speed 8.5` (apex 1.129 m against -32, pinned by a test), `coyote_time 0.10`,
  `jump_buffer_time 0.10`, `swim_speed 3.5`, `shore_pop_impulse 5.0`, `shore_pop_probe 0.6`,
  `eye_smooth_tau 0.10`, `eye_smooth_max_lag 0.25`, bob/dip/fov-kick terms, and
  `polish_max_offset 0.05` as the enforced budget. `kSeaLevelWorld = 0.0`.
  Note: **`move_speed` is not in here** — it lives on `app::SpectatorCameraState` at `40.0f`, which
  is why "walking speed" is currently `40 * 0.25 = 10 m/s`. That is ~2.5× a brisk human walk and
  ~1.3× world-record sprint pace. It is a number nobody chose; it fell out of a spectator default
  times a factor.
- **`world/collision/include/world/collision/terrain_collider.hpp`** — the 16 m / 3.1 cm cached
  height grid + trunk boxes, async background rebuild, `overlaps_solid`, `voxel_top`,
  `ground_height`. Its own comment states the design intent plainly: *"Not the octree itself: that
  is the follow-up once editing (goal 160) can make the analytic world stale."* This prompt brings
  that follow-up forward, for a different reason: the analytic query has an *edge*, and a fast
  camera crosses it.
- **`world/collision/include/world/collision/{solid_query.hpp, aabb_sweep.hpp}`** — the
  `SolidQuery` concept (`bool overlaps_solid(const Aabb&)`), and `move_and_slide` with y/x/z
  bisection, 0.25 m sub-steps and step-up. **The 0.25 m sub-step is the CCD granularity**, and it
  is the other half of the clipping story: at 160 m/s a frame's 2.7 m move is 11 sub-steps of a
  0.6 m-wide body — fine — but the sub-step size is a constant, not derived from the body size, so
  it is only correct by coincidence. Read `aabb_sweep.hpp` and check this yourself before changing
  anything.
- **`app/src/spectator_camera.hpp`** — `SpectatorCameraState { yaw, pitch, move_speed 40,
  look_sensitivity 0.0025, physics, tuning }`; `apply_look` (render cadence, deliberately not
  fixed-step, so a 144 Hz mouse is not sampled at 60 Hz); `to_intent`; `step_camera`;
  **`struct OpenWorld`** (the `--noclip` "nothing is solid" query, `static_assert`ed against the
  concept — a good pattern, keep it); and `update_spectator_camera`, explicitly labelled the
  legacy/`--noclip` entry point.
- **`app/src/main.cpp`** — `run_svo` at line 841. It constructs the `TerrainCollider` with
  `voxel_edge = geometry_for(spawn).finest_voxel_edge()` and calls `collider.refresh(spawn)`
  synchronously once before the loop. `update_camera_phase` (line ~333) takes
  `options.noclip ? nullptr : &collider`. `--noclip`, `--walk`, `--step-height`,
  `--no-view-polish`, `--autofly` are all in the flag set.
- **`world/svo/include/world/svo/brick_tree.hpp`** — `BrickTree::material_at(worldPos)` and
  `leaf_level_at(worldPos)` already exist and are exactly what an octree-backed query needs. The
  tree is **immutable after construction** and lives on the render side; `SvoWorld::take_finished`
  hands it to `SvoRenderer::begin_upload` which **takes ownership** — so today, after upload, the
  app has no CPU-side copy of the tree to query. **That is the central design problem of Group
  AJ-A. Read `svo_renderer.hpp`'s `begin_upload` contract and `svo_world.hpp` before choosing an
  answer.**
- **`world/svo/include/world/svo/terrain_sampler.hpp`** — the occupancy rule, verbatim: *"a voxel
  is solid iff its BOTTOM face height <= the column's surface height (sampled at the voxel's
  min-corner (x,z))"*, plus the band rules and tree voxelization. `TerrainCollider` reimplements
  this rule; the octree *is* this rule, already evaluated.
- **177 tests currently pass.** `world/collision` has 9, `world/player` has 3 files' worth.

---

## 3. Standing rules for this pass

All of Prompt 002 §3 applies unchanged. Restated, plus the ones specific here:

1. **A visual/behavioural change is verified by a harness scenario and a viewed capture**, both
   backends where shader-adjacent. Numbers alone are not "done."
2. **CPU reference first, oracle always.** If you touch `ray_trace.cpp` or `svo_march.psh.hlsl` —
   you should not need to in this prompt — the 7,000-ray oracle gates it.
3. **Performance measured**, RelWithDebInfo or Release, before/after in the log. A collision query
   in the fixed step is a per-tick cost: budget it explicitly (see 228).
4. **Tests green; new systems get new tests.** 177 is the floor.
5. **Determinism**: the same seed and the same input script produce the same trajectory, bit for
   bit, on both backends. This is now testable because 002 exists — use it.
6. **Materials are components.** Whether a material is solid comes from
   `world/materials` (`Phase::Solid`), never from an ID literal.
7. **Main-thread-only events**; background work follows the `SvoWorld` pattern.
8. **No new dependencies** without a written case.
9. **Read the goals.md group notes** (AA and AD) before closing 173 or anything in AD.
10. **Use your skills** — the reference files in §1.10, by name.
11. **Delegate web research to one or two read-only subagents**, never inline in the main session.
    Likely candidates here: character-controller CCD practice at very small voxel sizes, and
    swept-AABB-vs-voxel-grid traversal algorithms. Give the subagent a specific question, keep
    coding while it runs, persist anything worth keeping into `research/` and cite it.
12. **Commit and push per group.** MSVC has no UBSan; CI is the only place this defect class is
    visible, as last pass proved with the tree-species hash.
13. **`git add` explicit paths, never `git add -A`** — the side session is writing into `research/`
    concurrently.

---

## 4. Task groups

### Group AJ-A — Collision that has no edge (goals 225–230)

This group closes **goal 173**. It is the one that actually fixes "I clip through blocks."

**225. Decide where the CPU-side truth lives, and write the decision down.**
The octree is built on a worker, moved into the renderer, and uploaded. To query it on the
simulation thread you need one of:
 (a) keep a shared immutable copy (`std::shared_ptr<const BrickTree>`) that both the renderer and
     the simulation hold, swapped atomically on adoption;
 (b) query the *sampler* instead of the tree — `TerrainSampler::material_at(voxelMin, voxelEdge)`
     is the same rule at exact resolution and needs no tree at all;
 (c) keep the analytic `TerrainCollider` but remove its cache edge.
Rank them. My read, for you to check rather than accept: **(a) is the winner** — it is the only
option that makes collision agree with *what is drawn*, which is the property the owner is asking
for ("cannot pass through anything the renderer draws"), and `BrickTree` is already immutable, so
sharing it is a `shared_ptr` and nothing else. (b) is a strong second and is *more* accurate than
the tree near the camera (no LOD), but it re-derives rather than shares, so a future divergence
between sampler and tree becomes a collision bug; keep it as the reference the tests compare
against. (c) loses: an analytic height field cannot answer for a cave, an overhang, or an edited
voxel, and it is the thing that already failed.
**Check**: the ranking, the reasoning, and the losers' specific reasons are in the pass log. The
chosen option is implemented. If you rank differently than I did, say why in one paragraph — a
different answer with a reason is fine; the same answer without one is not.

**226. `world/collision/octree_collider`: a `SolidQuery` over the brick tree.**
A new type satisfying `world::collision::SolidQuery` that answers `overlaps_solid(Aabb)` from a
`BrickTree`. Semantics: a box overlaps solid if any voxel the tree reports at that point has a
material whose `Phase` is `Solid` (ask `world/materials`, do not compare IDs). Walk the octree once
for the box rather than point-sampling a grid: descend from the root, skip children the box does
not touch, stop at the first solid leaf or brick voxel that overlaps. That is an O(depth) walk with
early-out, not `n³` point queries.
Also provide `voxel_top(x, z)` (the surface the body rests on) with the same traversal.
**Cave/overhang correctness matters even though the current terrain has none** — Prompt 006 adds
caves, and a query that assumes "one surface per column" will be a bug then. Write it for a
genuinely 3D world now.
**Check**: unit tests against a hand-built tiny tree (a single solid cube, a 2-level tree, a tree
with a hole/overhang) asserting exact overlap answers on boxes that touch the face, share the face
exactly, and miss by one voxel. Plus a cross-check: over 10,000 random boxes in a real generated
region, `OctreeCollider` and `TerrainSampler`-derived truth agree — and where they disagree, the
disagreement is *only* where the tree's LOD is coarser than the sampler, in the conservative
direction (solid where the sampler says air, never the reverse). **State the measured
disagreement rate and its direction.** If the tree is ever *less* solid than truth, that is a
clipping bug and you have found it.

**227. Share the tree with the simulation.**
Implement whatever 225 chose. If (a): `SvoWorld::take_finished` yields a `shared_ptr<const
BrickTree>`; the renderer's `begin_upload` takes a copy of the handle rather than the object; the
app holds the current handle and swaps it on adoption. The swap must be safe against a
simulation tick reading it — the fixed step runs on the main thread, so a plain member swap at a
known point in the frame is sufficient and correct; say so rather than reaching for an atomic you
do not need.
**Watch the lifetime**: `SvoRenderer` currently keeps the CPU tree alive until every upload slice
has landed. Two owners of one immutable object is exactly what `shared_ptr` is for; two owners of a
*mutable* one is a bug. Assert immutability (`const`).
**Check**: a harness scenario that walks continuously for 60 s across several rebuilds reports zero
frames in which the simulation's tree handle was null or stale-by-more-than-one-generation. Memory
does not grow across rebuilds — measure resident MB at 0 s and 60 s and report both (an accidental
`shared_ptr` cycle or a retained old tree shows up here).

**228. Retire the cache edge, keep the fallback honest.**
With 226/227 in, `TerrainCollider` is no longer the body's query. Decide its fate: keep it as the
*reference* for tests and for the mesh path, or delete it. Either is defensible; an unargued answer
is not. If kept, it must no longer be what the svo path's body collides against, and the
`cache_extent`/edge-fallback behaviour must be documented as reference-only.
**Then remove the analytic backstop from `step_player`** — the `eyePosition.y <= standingEyeY`
clamp. Its own comment says it should never fire with collision on; with an edgeless query it
provably cannot be needed, and leaving it in means a real query gap gets silently papered over
instead of reported. Replace it with an *assertion-grade counter*: if the body ends a tick inside
solid, count it and log it once with the position. That converts a hidden save into a visible bug.
**Check**: the harness's `walk_hillside`, `walk_shoreline`, `fly_transect` and a new
`clip_stress` (below) all report **zero** "ended tick inside solid" events. Deliberately break the
query (a test-only mode that always answers "air") and confirm the counter fires — a counter you
have never seen fire is not a counter you can trust.

**229. `clip_stress`: the scenario that would have caught this.**
A harness scenario that drives the body horizontally and diagonally into terrain at escalating
speed — 1×, 4×, 10×, 40× walking speed — and into a tree trunk, and off a cliff, and up a 45°
slope, for a fixed duration each, asserting: zero inside-solid events, zero position
discontinuities larger than the per-tick move, and that the body's final position is on the
correct side of every surface it hit.
Derive the sweep's sub-step size from the **body's smallest half-extent**, not from a constant:
`move_and_slide`'s 0.25 m sub-step against a 0.3 m half-width is only safe by accident. State the
rule you chose and the worst-case speed it is safe to.
**Check**: the scenario passes at every speed rung. The table of (speed, sub-steps per tick,
inside-solid events, max discontinuity) is in the log. **Run it once with the old
`TerrainCollider` and once with the new query and put both columns in the table** — that is the
before/after that proves the fix rather than asserting it.

**230. Collision cost, budgeted.**
Measure the query in the fixed step: mean and p99 microseconds per tick, at 60 Hz, in the worst
scenario (`clip_stress` at 40×, and `stress_pose`). The whole fixed step must stay a rounding error
against a 6.7 ms frame (150 fps): budget **≤ 0.20 ms per tick** for collision, and say what you
measured. If it exceeds that, the octree walk is wrong (probably descending too far, or not
early-outing on the box test) — fix the walk, don't raise the budget.
**Check**: numbers in the log, from the harness report, both backends (they should be identical —
this is CPU work; if they differ, something is wrong and that is a finding).

---

### Group AJ-B — One body, always (goals 231–236)

**231. Walk is the default; fly leaves the shipping surface.**
`PlayerState::mode` defaults to `MoveMode::Walk`. `--walk` becomes a no-op alias (keep it: it is in
`CLAUDE.md` and a dozen research logs) and `--fly` is added as the *dev* opt-in. The `G` toggle
moves behind the same dev gate — decide whether that is a build option, a `--dev` flag, or a
scenario-only capability, and justify it. My recommendation: a single `--dev` flag that unlocks
fly, noclip, `G`, and the debug views as a group, so there is one door rather than four.
`--noclip` stays reachable only through it.
**Rationale to record:** the free camera is a *tool*, and tools belong to the harness. Its presence
as the default is why the owner has been playing a spectator for three passes.
**Check**: `voxel_app` with no flags starts grounded, in `Stance::Grounded`, within one voxel of
`voxel_top` under the spawn column, with `MoveMode::Walk` — asserted by a harness scenario reading
the state, not by looking. `G` does nothing without `--dev`. `--noclip` without `--dev` prints a
one-line message naming `--dev` and exits non-zero rather than silently ignoring it (silent
ignoring of a flag is how you lose an afternoon).

**232. Walking speed becomes a real number from the research, not a leftover product.**
Today walking is `move_speed 40 × walk_speed_factor 0.25 = 10 m/s`. Read
`research/locomotion-biomechanics-physics.md` for the measured bands (comfortable walk, brisk walk,
jog, run, sprint; and the inverted-pendulum walk-to-run transition speed) and
`research/player-movement-in-games.md` for what shipped games use and why they exceed reality.
Then **choose, with the numbers written down**: a walk speed, a sprint speed (Shift), and whether
this game is realistic-scale or game-scale. Move `walk_speed` and `sprint_speed` into
`PlayerTuning` as first-class fields in m/s and delete the `move_speed × factor` product from the
walking path (fly keeps `move_speed`).
**Check**: the chosen numbers, the research band each sits in, and the reason for any deliberate
departure from reality are in the log. A test asserts the tuning's walk speed is inside the band
the log names, so a later "just bump it" cannot silently leave the evidence behind.

**233. Gravity and jump: one coherent choice.**
`gravity = -32.0` is 3.26× Earth, with `jump_speed 8.5` giving a 1.129 m apex. That is a
game-convention choice (fast falls, snappy jumps) and it is *fine* — but it is currently
undocumented as a choice, and it interacts with the biomechanics research in a way worth being
explicit about: a real standing vertical jump is ~0.4–0.5 m with a takeoff velocity near 3 m/s, so
this body jumps ~2.5× a human's height with ~2.8× a human's takeoff speed under ~3.3× a human's
gravity. Decide and record: keep game-scale (and say why: fall time, platforming feel, the
research's own account of why games do this), or move to Earth-scale (and re-derive `jump_speed`
for the apex you want). Do not leave it implicit.
**Check**: the decision, the arithmetic, and the affected constants are in the log. The existing
test that pins `apex = v0²/(2|g|)` still passes with whatever numbers you chose. Add a test that
pins the *fall* time from 5 m, so a future gravity change is visible in two places.

**234. Sprint, and the stance machine's missing state.**
`Stance` is `{Grounded, Airborne, Swimming}` and `PlayerIntent::boost` currently multiplies speed
by 4. With walking as the default, Shift should be a *sprint*, which the biomechanics research
gives real numbers for (and a real cost: cadence and step length both change, and sprinting up a
slope is slower — the research has the slope-speed relationship). Implement sprint as a speed
change with an acceleration ramp rather than an instant multiply, and make the FOV kick (already
in `PlayerTuning`) respond to actual speed rather than to the boost flag.
Do **not** add stamina, crouch, or prone — out of scope, and each is a design decision the owner
has not asked for.
**Check**: a test asserts the acceleration ramp reaches sprint speed in the time the tuning
declares, and that releasing Shift decelerates over the declared time; a scenario captures the FOV
at walk and at sprint and the difference is inside `fov_kick_radians`. Uphill sprint speed follows
the research's slope relationship — assert it at 0°, 15° and 30° against the band the research
gives.

**235. Slope limits, from the ground up.**
Nothing currently refuses a slope. At 7.8 mm voxels, a 60° hillside is climbable simply because the
sub-centimetre staircase always offers a 4 cm step (`kSvoStepHeight`). Real terrain has a
walkable-slope limit and the biomechanics research has the number (and the energetic reason for
it). Implement a maximum walkable slope: above it, the body slides rather than climbs.
**Check**: a scenario walks into slopes at 20/30/40/50/60° and asserts climb-vs-slide matches the
declared limit at each; the transition is not jittery (no oscillation between climb and slide
within one tick — assert the state is stable over 60 ticks at 2° above and below the limit).
Viewed capture of the body being refused by a cliff.

**236. Swimming, revisited against the water that now moves.**
`step_player` already swims, with buoyancy, dive, and the shore-pop assist, and `WorldSense::
water_surface_y` is now the live Gerstner sum (goal 197). Two things to check rather than assume:
the `inWater` predicate is `feetY < surface && ground_height < surface` — with a *moving* surface
this can flicker at the waterline within one wave period; and the shore-pop probe (0.6 m) was tuned
against a flat surface.
**Check**: a scenario stands at the waterline for 30 s of wave motion and asserts the stance does
not flicker more than once per wave crest (state the measured count and the wave period). Swim out
and back in at three wind speeds (0.5, 3, 8 m/s) with zero inside-solid events and zero stuck
frames. Viewed capture of the body at the waterline mid-crest.

---

### Group AJ-C — The head, per the vision research (goals 237–240)

These are the parts of `human-eye-and-vision-research.md` §5.7 that belong to the player rather
than to the renderer. Each is small; together they are most of what makes a first-person view feel
like eyes rather than a camera.

**237. Auto-exposure metered at the crosshair, pooled over ~6°.**
§5.7(b): the adaptation state that matters perceptually is pooled over roughly 6° around fixation,
and a bright sky at the top of the screen should not crush a dark valley the player is looking
into. Implement a metering mask centred on the crosshair with a ~6° pooling radius (compute the
pixel radius from the actual FOV and viewport — do not hardcode a pixel count), feeding the
existing two-timescale adaptation (fast up, slow down), which the research notes accidentally
mimics the light/dark adaptation asymmetry.
**Check**: a scenario looks from sky to shadowed valley and back; the exposure trace is in the
report and shows the asymmetric time constants; captures at the two extremes on both backends,
viewed. A global-average build and a crosshair-metered build of the same pose are captured
side by side and the difference is described in the log (this is the "is it actually better"
question, and the answer must come from looking).

**238. Bloom that scales with the ratio of source to adaptation luminance.**
§5.7(c): game bloom should behave like veiling luminance — intensity proportional to source
luminance *relative to the adaptation level*, and the kernel widening as adaptation drops, so a
torch in a cave blooms like the sun outdoors. Today the bloom is DiligentFX with a threshold. Feed
it the adaptation level from 237.
**Careful**: `post_process.cpp` has a documented, empirically-found requirement — DiligentFX bloom
needs one warm-up `PostFXContext::Execute` with dummy inputs before it reports ready, and
`PostProcessor` must be constructed **before** the renderers because the scene-target format flows
into the terrain/sky PSOs. Read that file's comments before touching it.
**Check**: captures of the same bright source at two adaptation levels showing the widened kernel
and higher relative energy at the darker one; `--verify-frame` contrast unchanged within the noise
floor measured in Prompt 002 goal 217; both backends.

**239. Motion blur stays off, and that is now a written decision.**
§5.7(e) is unambiguous for a first-person eye simulation: the eye smears during gaze shifts and
*deletes* the smear; the display's own persistence already provides the physically correct pursuit
smear; adding shutter blur simulates a camera the player is not, and the cited study measured no
player-experience benefit. There is no motion blur today. **Do not add one**; add the decision to
`docs/progress.md`'s decided-against list with the citation so it does not get proposed again.
Same for lens ghosts (a camera tell) and for a luminance vignette — §5.7(f) explains that a
vignette darkens exactly the region the periphery is tuned to detect, and reads as tunnel vision
rather than as reduced detail.
**Check**: the three entries are in the decided-against list, each with its research section
reference. No code change. (This is a real task: the value is that the next pass does not spend a
day on it.)

**240. View polish, re-verified against the perception thresholds.**
`eye_smooth_tau 0.10`, `polish_max_offset 0.05`, bob 2.5 cm at 1.9 cycles/m. Prompt 001's own note
says A3 shipped without a capture sequence because a 4 cm effect at a 0.1 s time constant is not
something a still shows honestly. The harness fixes that: a scenario can emit a numbered sequence.
Check the constants against `research/human-movement-and-perception-research.md`'s detection
thresholds for self-motion and view lag, and report whether each is above or below threshold.
**Check**: a captured sequence (10 frames across a landing) committed as a strip, viewed, and the
per-frame eye offset in the report. Each polish constant is listed in the log with the perception
threshold it sits against and a verdict: perceptible-and-intended, or below threshold and
therefore pointless (and if pointless, delete it rather than keeping a term nobody can see).

---

### Group AJ-D — What the player can see of themselves (goals 241–243)

**241. The aim query sees the world, not a model of it.**
`app/src/aim_query.cpp` (148 lines) reports what the crosshair is on, and after Prompt 001 it sees
terrain, water and trees via a `TreeLookup` built from the heightmap and seed. With an octree query
available (226), the aim query should ask the *same* structure collision does, so the crosshair
cannot disagree with the body about what is there.
**Check**: over 2,000 random view rays in a real region, the aim query's reported material matches
the octree's `material_at` at the reported hit point. Where it doesn't, the cause is named. A
viewed capture with the readout over terrain, water, a trunk, and leaves.

**242. Reach and interaction distance, from the eye research.**
The crosshair readout currently reports distance. Bound it with a real number: what a player can
resolve and what they could plausibly reach. `human-eye-and-vision-research.md` Part 1 §8.1 gives
the arithmetic (1 arcmin MAR; a 1 cm detail resolvable to 34 m at 20/20, ~54 m for a 94-ppd young
observer; an 18 cm face to 619 m as a blob). Pick the readout's max range from that rather than
from a round number, and say which criterion you used.
**Check**: the chosen range, the criterion, and the arithmetic are in the log. A scenario asserts
the readout goes blank beyond it.

**243. A body you can see is out of scope — record that.**
No player model, no shadow of the player, no first-person arms. Each is a real content decision the
owner has not asked for. Add to the decided-against list *for this pass only*, with the note that
the body's collision box exists and is 0.6 × 1.75 m, so a model can be added later without
re-deriving anything.
**Check**: the entry exists. No code.

---

## 5. Sequencing and risk

- **AJ-A is the spine.** 225 → 226 → 227 → 228 → 229 → 230, in order. Do not start AJ-B before 229
  passes: making walk the default while collision still has an edge means the owner's first
  experience of the change is falling through a hill.
- **AJ-B depends on AJ-A. AJ-C and AJ-D are independent of both** and can be done in any order
  after 002.
- **Commit and push per group**, four or five commits.
- **The risky task is 227** (sharing the tree). If the ownership refactor turns out to fight
  `SvoRenderer::begin_upload`'s staged-upload lifetime, the honest fallback is option (b) from 225
  — query the `TerrainSampler`, which needs no sharing at all and is *more* accurate near the
  camera — and record the reason (a): failed. That is a legitimate outcome; a half-shared tree with
  a lifetime bug is not.
- **The subtle task is 228** (removing the backstop). If removing it makes something fall through,
  **that is the bug this whole group exists to find** — do not put the backstop back. Report where
  the query said air and the world said solid.
- If any task is blocked, finish the rest and say exactly what is blocked and why.

---

## 6. Explicitly out of scope this pass

- **Any renderer, shading or LOD change.** Prompts 004 and 005.
- **Any terrain change.** Prompt 006. In particular, do not add caves here — but *do* write the
  octree query so caves cannot break it.
- **Editing the world (goal 160)** and the HashDAG structure it needs. Not this arc.
- **Stamina, crouch, prone, climbing, vehicles, combat, inventory, interaction verbs.** None asked
  for.
- **A player model, first-person arms, player shadow.** Recorded as deferred in 243.
- **Trees C4–C6 (190–192), grass (193–195).** Prompt 007.
- **Multiplayer, netcode, save/load.** Not this arc.

---

## 7. When you are done

1. Write **`research/player-embodiment-log.md`**: the 225 ranking with the losers' reasons; the
   measured `OctreeCollider`-vs-sampler disagreement rate and direction; the `clip_stress`
   before/after table; the collision cost numbers; the speed/gravity/jump decisions with their
   research bands; the slope limit; the exposure and bloom captures with what you saw; the polish
   constants against their perception thresholds; and every decided-against with its reason.
2. Add **Group AJ** to `docs/goals.md`, goals 225–243, numbered exactly as above, each `[x]` with
   its Check recorded as performed. **Mark goal 173 `[x]` in place** and point it at 226/227.
3. Refresh `docs/progress.md`: the current-state paragraph now says the game has a body by default
   and collision is octree-backed; add this pass's entries to *"Decisions that survived contact
   with evidence"* (including the three from 239 and the one from 243).
4. Update `CLAUDE.md`'s operational notes: `--dev` replaces the ungated fly/noclip/`G`, the new
   default is walk, and the new scenario names.
5. Update `world/player/README.md` and `world/collision/README.md` for the new query and the
   removed backstop.
6. Update `Prompts/README.md`'s index row for 003.
7. Full suite green (177 + new), both backends, `clip_stress` passing at every speed rung, captures
   viewed and named in the log.

---

*Provenance: written by the side session on 2026-09-06 after reading
`world/player/include/world/player/{player_state,controller,tuning}.hpp`,
`world/collision/include/world/collision/terrain_collider.hpp`,
`app/src/spectator_camera.hpp`, `app/src/main.cpp` (`run_svo` and the camera phase),
`world/svo/include/world/svo/{brick_tree,terrain_sampler,tree_layout}.hpp`,
`render/diligent/include/render/diligent/svo_renderer.hpp`, `docs/goals.md` Groups AA/AD/AF–AH,
`docs/progress.md`, and `research/human-eye-and-vision-research.md` Part 1 §8 and Part 5
§5.4–§5.7. The clipping diagnosis (a 16 m collision cache with an 8 m rebuild trigger against a
160 m/s boosted fly speed) and the spectator-default diagnosis (`PlayerState::mode = Fly`) were
both read out of the code, not inferred from the report.*
