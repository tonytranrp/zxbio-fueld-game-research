# Player embodiment — decision log

Prompt 003, Group AJ (goals 225–243). **Partial**: AJ-A's collision spine (225–228) and AJ-B's
first goal (231) are done; 229/229a/230 and 232–243 are not. What is here is written the same way
the rest of this repo's logs are — every measurement, every "decided against", and the things that
turned out to be wrong.

---

## 1. Where the CPU-side truth lives (goal 225)

Three options were on the table. I ranked them the same way the prompt did, and for the same
reason, so this section is short:

**(a) A `shared_ptr<const BrickTree>` shared by the renderer's staged upload and the simulation —
CHOSEN.** It is the only option under which collision agrees with *what is drawn*, which is
literally the owner's complaint ("I can clip through blocks"). `BrickTree` has been
immutable-after-construction since the micro-voxel pivot, so sharing it is a `shared_ptr` and
nothing else: no copy of 400 MB, no synchronisation, no lifetime question that `const` does not
already answer.

**(b) Query the `TerrainSampler` instead — kept as the tests' reference.** It needs no sharing at
all and it is genuinely *more* accurate near the camera (no LOD). But it re-derives the world rather
than sharing it, so a future divergence between sampler and tree becomes a collision bug that
nothing would catch. It is the right thing to compare *against*, which is exactly what the
cross-check in §2 does with it.

**(c) Keep the analytic `TerrainCollider`, remove its cache edge — loses outright.** A height field
cannot answer for a cave, an overhang, or an edited voxel. Prompt 006 adds caves. And it is the
thing that already failed: a 16 m cache with a background rebuild, whose tree-trunk half only knows
about trunks collected into the *current* cache, is a query a fast camera outruns by construction.

`TerrainCollider` is **kept, as the mesh path's collider and as the tests' reference**, and is no
longer what the svo path's body collides against.

---

## 2. The octree query, and the hole rate (goal 226)

`overlaps_solid(Aabb)` converts the box to integer voxel coordinates (every `TreeGeometry` edge is
a power of two, so this is exact and needs no epsilon) and descends, skipping octants the box does
not touch and stopping at the first solid voxel. `voxel_top(x, z, yStart)` descends +y-first and
takes the first hit, so it is genuinely 3D — **there is no "one surface per column" assumption
anywhere in it**, which is what makes it survive Prompt 006.

Solidity is asked of `world/materials` (`properties_of(id).is_solid()`), never by comparing IDs.
Water is `Phase::Liquid` and leaves are `Phase::Foliage`, so the swimmer swims and the canopy is
walk-through *because their component files say so*.

### The walk is cheap

Measured on a 64 m / 12.5 cm tree with a body-sized box: **7 nodes visited inside solid, 1 in open
air.** The failure mode goal 230 warns about — descending the whole depth on every call — would
show as hundreds or thousands. It is a walk.

### The cross-check, and the direction of disagreement

At **uniform LOD**, over 10,000 voxel-sized boxes in a real generated region: **0 disagreements,
both directions.** The query *is* the sampler. That is the result that validates the traversal.

At the **shipping distance LOD** I expected the disagreement to be merely *conservative* — a coarse
node standing in for a mostly-solid region, reading solid where the sampler says air. **It is not.**
There are genuine holes, where the tree says air and the sampler says solid, because the builder
collapses a sparse solid region far from the LOD centre to an **absent child** rather than to a
coarse solid one. Over the whole region: **0.41%**.

That number alone would be alarming. Binned by distance from the LOD centre it is not:

| distance from LOD centre | samples | holes | rate | conservative |
|---|---|---|---|---|
| 0–4 m | 54 | **0** | 0.000% | 0 |
| 4–8 m | 410 | **0** | 0.000% | 0 |
| 8–12 m | 1068 | 1 | 0.094% | 0 |
| 12–16 m | 1978 | 5 | 0.253% | 2 |
| 16–20 m | 2867 | 5 | 0.174% | 2 |
| 20–24 m | 4084 | 13 | 0.318% | 2 |
| 24–28 m | 5315 | 17 | 0.320% | 3 |
| 28–32 m | 24224 | 116 | 0.479% | 8 |

**Zero holes inside 8 m of the LOD centre** — twice `lod_radius`. The body is always deep inside
that: the LOD centre *is* the camera, and `run_svo` requests a rebuild once the camera has moved
2 m. So the property collision depends on holds, and the test asserts exactly it (`holesIn[0] == 0`)
rather than the region-wide claim, which is false.

**The consequence, recorded rather than left implicit: the LOD centre must track the body.** It does
today. If a future change ever separates them — a detached debug camera, a spectator following
someone else, a body that outruns a rebuild by more than 8 m — this guarantee is gone and collision
has to fall back to the sampler. There is a residual window today: at 40 m/s during a 1.3 s rebuild
the camera can be ~50 m from the *previous* tree's LOD centre, where the hole rate is ~0.5%.
**Prompt 004 removing the rebuild storm removes that window**, which is one more reason it is next.

---

## 3. Removing the backstop found three bugs in a row (goal 228)

`step_player` ended with a clamp: `eyePosition.y >= sense.ground_height + eye_height`, commented
"should never fire" with collision on, and "the one thing standing between a query gap and a fall
through the world."

Both halves were true, and together they made it a liability. **A save that should never fire,
firing silently, is a bug detector wired to a mute button.** With the old cached collider, a fast
camera left the cache, the query said air, and the clamp quietly put the body back — so the query
gap that *is* the "I clip through blocks" complaint never appeared as anything at all.

It is gone. What replaced it is a counter: a tick that ends with the body's box overlapping solid
is counted, and the first one is logged with its position. The clamp survives only for a query that
declares itself an open world (`OpenWorld::open_world_tag`) — `--noclip` and the collision-free
tests, which would otherwise fall forever.

**It fired on the first run, on the simplest possible scenario, and then three times more as each
cause was fixed.** `spawn_stand` — a scenario in which nothing moves — reported **181 of 181 ticks
inside solid.**

1. **`pose_ground` resolved against the ANALYTIC height.** The sampler's rule is "a voxel is solid
   iff its bottom is at or below the column's surface height", so the voxel *containing* the
   analytic height is solid and its top is *above* it. Feet placed at the analytic height are inside
   the top solid voxel. Fixed by snapping up to the voxel grid. **Still 181.**
2. **A point height cannot place a BOX.** The body is 0.6 m wide; at (48, 0) the terrain falls
   ~0.6 m per metre, so the box's uphill corner sits ~0.19 m above the centre column. Fixed by
   taking the maximum over the footprint's four corners and centre. **Still 181** — but the spawn
   had moved up 0.375 m, which is how I knew the remaining cause was different in kind.
3. **Even the footprint maximum misses by exactly one voxel.** Instrumenting the counter to print
   `voxel_top` under each corner gave it away: feet at **66.3438**, uphill corner voxel top
   **66.3516**, voxel edge **0.0078** — one voxel, exactly. The sampler samples each voxel's *own*
   min corner, so **no point sample of `height_at` can predict a neighbouring column's voxel top.**
   Fixed by snapping up plus **two** voxels of clearance (one for the sampling rule, one for float)
   and letting the body settle the last centimetre under gravity — which is what walk mode is for,
   and is why this and goal 231 landed together.

`spawn_stand`: **181 → 0.** `fly_transect`, 1483 frames of continuous motion across several
rebuilds: **0**.

### And one bug it found that is NOT fixed (goal 229a)

`walk_hillside` reports **253** and `walk_shoreline` **236** inside-solid ticks, *while walking into
slopes*. `spawn_stand` (still) and `fly_transect` (moving continuously) are clean, so it is specific
to walking into a slope — not to motion, and not to rebuilds.

Both scenarios now carry `assert inside_solid == 0` and therefore **FAIL**. That is deliberate, and
it is the prompt's own instruction: *"If removing it makes something fall through, that is the bug
this whole group exists to find — do not put the backstop back."* The backstop is not going back.

Prime suspects, in order, for whoever picks this up:

- **The `started_inside` escape in `move_and_slide`.** Once the body is inside solid, the sweep
  returns the wanted motion *unblocked* — so one bad tick becomes many, which fits 253 events
  clustering rather than scattering.
- **`kSvoStepHeight = 0.04 m` against a slope that rises more than that per tick.** At 10 m/s into a
  31° slope the body climbs ~0.10 m per tick against a 0.04 m allowance. This is also exactly what
  goal 235's slope limit is for, and 229a may simply *be* the missing slope limit.

The instrumentation to chase it is already in place: the counter logs `voxel_top` under all four
corners and the centre at the first offending tick.

---

## 4. Walk is the default (goal 231)

`PlayerState::mode` is `MoveMode::Walk`. `--walk` is a documented no-op alias, kept because it is in
`CLAUDE.md` and a dozen research logs. `--fly`, `--noclip` and the `G` toggle sit behind **one**
`--dev` door — the prompt's own recommendation, and right: four separate gates is four things to
forget.

`voxel_app --fly` without `--dev` **refuses by name and exits non-zero**. Silently ignoring a flag
is how you lose an afternoon.

The harness sets `dev` itself: a scenario is a developer context by definition, and making every
`.scn` repeat `--dev` would be noise.

**Six `test_spectator_camera` cases failed the moment the default changed.** They test the fly
camera's movement basis and pitch clamp and had been inheriting the default; they now say
`physics.mode = Fly` explicitly. That is the change being *visible* rather than silent, which is
the whole argument for making it.

---

## 5. A bug in the harness this pass found in its own previous output

`assert walk_violations == 0` **passed on a run that logged 1861 of them.**
`RunResult::walk_violations` was declared, reported in the JSON, and never written by anything.

An assertion that cannot fail is worse than no assertion, because it reads as evidence. The
counters are now wired through a `RunHooks::on_invariants` callback, and there is a second metric
(`inside_solid`) beside it. This is the second time in two prompts that a check passed vacuously
(the first was the JSON writer's brace-balance test); the pattern is worth naming: **a check written
at the same time as the thing it checks tends to share its blind spot.**

---

## 6. Research read but not yet applied (goals 232–235)

`research/locomotion-biomechanics-physics.md` and `research/player-movement-in-games.md` were read
in full for this pass. The bands, for whoever does 232–235:

| | measured human | shipped games |
|---|---|---|
| preferred walk | **1.39 m/s** (1.3–1.5) | Minecraft 4.32, Source 5.72, Overwatch 5.5 |
| walk→run transition | **1.89–2.16 m/s** (Froude ≈ 0.5) | — |
| endurance run | 3–4 m/s | — |
| sprint (fit human) | 6–8 m/s | Skyrim 7.1, TF2 Scout 7.62, GTA V ~7.0 |
| Bolt, top | 12.32 m/s | — |
| ground acceleration | ~10 m/s² peak, 5+ s to top speed | **10–60 m/s² (1–6 g), instant** |

The engine's current walking speed is `move_speed 40 × walk_speed_factor 0.25 = **10 m/s**` — about
7× a preferred human walk, faster than every shipped game in the table, and, as the prompt says,
a number nobody chose: it fell out of a spectator default times a factor.

`player-movement-in-games.md` §5.7(a)'s own recommendation for *this* engine is walk 1.4 / jog 3.5 /
sprint 7 m/s with a critically-damped velocity spring at a 0.25–0.5 s time constant, ARMA's shipped
×0.75 backpedal and ×0.8 lateral penalties, and the slope relationship from Minetti's polynomials
(`C_w(i) = 280.5i⁵ − 58.7i⁴ − 76.8i³ + 51.9i² − 19.6i + 2.5`) for uphill speed. That is the decision
232–235 has to make explicitly rather than inherit.
