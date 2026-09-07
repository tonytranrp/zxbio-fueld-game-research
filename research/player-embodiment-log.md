# Player embodiment — decision log

Prompt 003, Group AJ (goals 225–243). **AJ-A, AJ-B and AJ-D are complete** (225–236, 241–243), AJ-C is complete except
237 and 238 (see §14). What is here is written the same way
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

### And one bug it found, now fixed (goal 229a)

`walk_hillside` reported **253** inside-solid ticks and `walk_shoreline` **236**, *while walking
into slopes*. `spawn_stand` (still) and `fly_transect` (1483 frames of continuous motion) were
clean, so it was specific to walking into a slope — not to motion, and not to rebuilds.

Both scenarios carried `assert inside_solid == 0` and therefore **FAILED**, deliberately, per the
prompt's own instruction. The backstop never went back. Here is what was actually wrong, in the
order it came apart, because none of the three causes was the one I named first.

**Step 1 — attribute the events before theorising.** `StepResult` gained `started_inside`,
propagated from `move_and_slide`, and the app grew two more counters (`started_inside`,
`stepped_up`). One instrumented run settled it:

```
collision: 761 ticks ended with the body INSIDE solid (0 required) -- 761 of them the sweep
had ALREADY found embedded (started_inside: it moved unblocked), 0 stepped up
```

**761 of 761.** Not the step-up — the *escape*. The count also moved 253 → 761 between the
un-instrumented and instrumented runs, which is itself a finding: the event count is not stable
run to run, so any conclusion drawn from its magnitude alone would have been noise.

**Step 2 — the escape never escaped.** `move_and_slide`'s "genuinely embedded" branch returned the
wanted motion *unblocked*: the classic never-trap-the-player policy. It does keep the player from
being trapped and it never frees them, because moving unblocked through solid ends every tick still
inside. One bad tick became a permanent state. Replaced with a bounded climb: probe upward in
doubling steps (skin×8, ×2, …) to the body's own height, bisect between the last blocked rung and
the first free one, and lift by the least that works. Upward is where the free space provably is,
since the body walked in from above the surface; the bound means a genuinely buried body still gets
the unblocked move rather than teleporting to the sky, and still *says* so, which is what the
counter reports. **`walk_hillside` 761 → 0, `walk_shoreline` 236 → 0.**

**Step 3 — the ladder had a hole in it.** `macro_ground` then failed with 121 events at a depth of
**1.40 m**: the doubling ladder reaches 1.024 m and the cap is 1.75 m, so a body buried between the
last rung and the cap was declared unrecoverable *by an arithmetic accident*. The cap is now probed
explicitly. There is a test for exactly this gap.

**Step 3b — and `macro_ground`'s pose was never legal.** Its own describe line says "camera 30 cm
above the ground"; a 1.7 m body standing on the ground cannot put its eye there, so under the walk
default the scenario buried the body 1.40 m and reported it every tick — from a scenario in which
nothing moves. It is a *camera* shot, and now says so: `--fly --noclip`. The distinction is worth
stating once, because the scenario set has both kinds: a scenario with a body-height eye is a body
test, and one with an arbitrary eye is a look test, and a look test must not be asked to obey
gravity.

**Step 4 — most of what was left was the counter measuring itself.** `clip_stress` (below) at 4×
speed still reported 358 events with **zero** `started_inside` and, with the step-up disabled
entirely, still 358 — so neither the escape nor the climb. The first-event dump gave it away:

```
feet y 27.4765 ... voxel_top under the body: corners 24.3047 25.8750 25.8750 27.4766
```

Feet **27.4765**, deepest corner voxel top **27.4766**: embedded by **0.0001 m**. One
ten-thousandth of a metre, against a 0.0078 m voxel, and one tenth of the sweep's own 1 mm contact
skin. `SweepParams::skin` already documents exactly this — the caller stores the eye and rebuilds
the feet as `eye - eye_height` every tick, and a voxel world puts surfaces at exact coordinates
constantly — and the sweep already tolerates it. The **counter** did not, so it was measuring the
float round trip rather than the bug. It now asks about a box inset by the same skin.

The number that justifies the tolerance is the ratio: the artefacts are 0.0001 m and the real
embeddings this counter was built to find were **0.048 m** — 480× larger, and the 1 mm inset sits
between them with an order of magnitude of margin on each side.

**Result across the speed sweep, after all four:** inside-solid **0** at 1×, 4×, 10× and 40×.

### What this cost, and the general lesson

Four causes, of which the first suspect named in the previous draft of this section (the escape)
was right, the second (the step height) was wrong, and two more were not on the list at all. The
one that mattered most is the last: **a counter with no tolerance, watching a quantity whose
representation has error, measures the error.** It is the same shape as this pass's other two
vacuous checks — the JSON writer's brace balance, and `walk_violations` that was never written —
and the shape is: *a check written from the same mental model as the thing it checks inherits its
blind spots.* Three for three now.

---

## 3a. The sub-step rule, stated (goal 229)

`SweepParams::max_substep` was `0.25f`, against a 0.3 m body half-width. Correct — and for no stated
reason, and silently wrong the moment anyone shrinks the body. It is now `0` meaning "derive it",
and `substep_for(body)` is the derivation:

> The sweep tests END POSITIONS only, so it sees an obstacle exactly when one of the tested boxes
> overlaps it. Place the boxes a distance `d` apart along the path: if `d` is less than the body's
> extent along that axis, consecutive boxes **intersect**, so their union is a solid connected tube
> with no gap anywhere — and anything inside that tube is inside at least one tested box.
> Tunnelling is then impossible for an obstacle of **any** thickness, down to a single 7.8 mm voxel.
> If `d` exceeds the extent the union develops gaps of width `d − extent`, and an obstacle thinner
> than that fits through one.

So the rule is `d < extent`, per axis; the smallest extent covers every direction of travel at once,
and halving it leaves margin for the step-up path (which moves the box up, sideways and down, so the
tested boxes are not colinear) and for float. For the 0.6 × 1.75 × 0.6 m body: **0.30 m** — *fewer*
sub-steps than the constant it replaces, and now for a reason.

**The worst-case speed it is safe to, as the prompt asks: there isn't one, and that is the point.**
The sub-step count is `ceil(distance / d)`, unbounded, so the guarantee does not weaken with speed —
speed buys sub-steps, not risk. What speed buys instead is *cost*, and that is measured below. At
the 40 m/s fly speed a 1/60 s frame moves 0.67 m (3 sub-steps); at the 160 m/s boost, 2.67 m (9).

A test drives a body into a **one-voxel-thick (7.8 mm) wall** over 1, 5, 40 and 400 m in a single
call and checks it is stopped and outside every time. Under the old constant that test would also
have passed; the difference is that now it is guaranteed rather than lucky, and it stays true if the
body changes.

One thing the derivation broke, worth recording because it is the kind of thing a constant hides:
`1.5 / 0.25` is six binary-exact pieces and `1.5 / 0.3` is not, so an *unblocked* 1.5 m motion
started summing to 1.499999762 and a test that had asserted exact equality since Group A failed.
Sub-stepping is a search strategy, not a change to the answer, so an unblocked axis now snaps to
exactly the wanted delta — guarded by an epsilon, **not** by `!blocked`, because the step-up climbs
on y with `wanted.y == 0` and an unguarded snap silently deleted the 0.4 m climb. That second bug
took one build to find and is the reason the guard is written the way it is.

---

## 3b. `clip_stress`, and what it measured (goals 229, 230)

A new scenario: drive the body head-first into the steepest terrain the world has — the x = 0 → +32
climb at z = +16 that produced the 761 events — from four angles of attack, then run it under
`--ramp speed-scale:1,4,10,40`. `--speed-scale` is a new app flag that multiplies the base move
speed, which exists so this claim can be falsified rather than asserted.

| speed | walking speed | inside-solid | walk violations | collision ms/tick (mean) | worst tick |
|---|---|---|---|---|---|
| 1× | 10 m/s | **0** | 0 | **0.051** | 0.72 ms |
| 4× | 40 m/s | **0** | 0 | **0.059** | 3.41 ms |
| 10× | 100 m/s | **0** | 0 | **0.17 / 0.28** | 7.9 / 17.6 ms |
| 40× | 400 m/s | **0** | **220** | **0.95 / 0.98** | 7.3 / 36.6 ms |

Read honestly:

- **The sub-step rule holds to 400 m/s**, forty times the shipped walking speed. Zero inside-solid
  ticks at every rung. That is the falsifiable claim, and it survived.
- **Goal 230's 0.20 ms/tick budget holds to ~10×.** At the shipped speed collision costs
  **0.051 ms per tick** — four times under budget. At 10× it straddles the budget (two runs, 0.17
  and 0.28); at 40× it is 0.95 ms and the budget is gone. The cost is sub-steps, exactly as the
  derivation predicts: 40× the speed is 40× the sub-steps.
- **The 40× rung fails a different check.** 220 frames ended below the analytic ground surface. That
  is the walk-violation metric, not the collision one, and at 400 m/s the body crosses ~7 m per tick
  — far outside the 8 m radius in which §2 measured the LOD hole rate at zero. It is the residual
  window §2 already names, reached deliberately. Prompt 004 removing the rebuild storm is what
  closes it; nothing here can.
- **Worst-tick cost is dominated by something other than the walk.** 0.72 ms against a 0.051 ms mean
  at 1× is a 14× outlier, and the outliers cluster on tree-swap frames — a cold octree, not a deep
  descent. Recorded rather than chased; goal 230's budget is a per-tick mean and it is met.

`clip_stress` is captured but deliberately **has no golden**: two identical runs differ by 40.7% of
pixels. The pose is deterministic (1505 script-driven ticks either way) but the *terrain* is not —
at 4× and up the body outruns the LOD rebuild, so which tree is resident when it hits a slope
decides where it ends up. A golden here belongs after Prompt 004, not before.

### The stale goldens this pass had to face

Seven scenarios' goldens were failing before any of today's work — CI run 34107703351's Windows
renderer job failed at its Test step for exactly this reason, and the cause was commit `b684b3f`
making walk the default without re-taking them. Every 1.7 m standing pose moved by the same amount
(mean 3.7/255, ~14% of pixels), which is itself the evidence that it is one systematic change and
not seven unrelated ones: the body now settles *on* the surface instead of sinking into it. Both
capture pairs were viewed side by side before promoting — `valley_far`'s golden was a camera that
had fallen 12 m and landed low, `macro_ground`'s was a camera that fell during its own 2 s hold. The
new ones are the physically correct poses. Promoted on both backends.

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

## 6. The research bands, for reference

`research/locomotion-biomechanics-physics.md` and `research/player-movement-in-games.md` were read
in full for this pass. The bands section 7 chooses from:

| | measured human | shipped games |
|---|---|---|
| preferred walk | **1.39 m/s** (1.3–1.5) | Minecraft 4.32, Source 5.72, Overwatch 5.5 |
| walk→run transition | **1.89–2.16 m/s** (Froude ≈ 0.5) | — |
| endurance run | 3–4 m/s | — |
| sprint (fit human) | 6–8 m/s | Skyrim 7.1, TF2 Scout 7.62, GTA V ~7.0 |
| Bolt, top | 12.32 m/s | — |
| ground acceleration | ~10 m/s² peak, 5+ s to top speed | **10–60 m/s² (1–6 g), instant** |
| standing vertical jump | 0.4–0.5 m, ~3 m/s takeoff | — |
| walkable slope (measured span) | ±45% grade = **±24.2°** | Unreal 44.8°, Source 45.6° |

`player-movement-in-games.md` §5.7(a)'s own recommendation for *this* engine is walk 1.4 / jog 3.5 /
sprint 7 m/s, ARMA's ×0.75 backpedal and ×0.8 lateral, and Minetti's slope polynomial
`C_w(i) = 280.5i⁵ − 58.7i⁴ − 76.8i³ + 51.9i² − 19.6i + 2.5` for uphill speed. Section 7 takes the
first two and leaves the third to goal 235.

---

## 7. The scale decision (goals 232, 233, 234)

The prompt asks 232 and 233 as two questions and then says 233 must be "one coherent choice". They
are one question, and the answer is **realistic scale, throughout**.

The argument is the voxels. The entire point of a 7.8 mm voxel world is that you can see individual
cubes — it is what Group Z's whole look pass was for. A body that crosses that world at 10 m/s makes
them a blur and turns the world into a diorama you fly over. And 10 m/s was never chosen by anyone:
it is `move_speed 40 × walk_speed_factor 0.25`, the product of a spectator default and a fudge
factor. `research/player-movement-in-games.md` §5.7(a) makes the same recommendation, for this
engine by name.

| | old | new | research band | source |
|---|---|---|---|---|
| walk | 10 m/s | **1.4 m/s** | 1.39 measured, 1.3–1.5 common | biomechanics §2.1 |
| sprint (Shift) | ×4 = 40 m/s | **7.0 m/s** | fit human 6–8 | biomechanics §2.1 |
| ground acceleration | instant | **10 m/s²** (brake 14) | elite human sprint peak ~10; games 10–60 | games §5.2.3 |
| backpedal / lateral | none | **×0.75 / ×0.8** | ARMA's shipped values, inside the human 70–80% band | games §5.7(a) |
| gravity | −32 (3.26× Earth) | **−9.81** | Earth | — |
| jump apex | 1.129 m | **0.600 m** (v₀ 3.43 m/s) | human 0.4–0.5 m at ~3 m/s takeoff | biomechanics §6.5 |

Departures from measured reality, named rather than left implicit:

- **Sprint 7.0 m/s** is a fit human, not Bolt's 12.32, and not sustainable — the research's own note
  is that nobody sprints at maximum for more than 5–8 s. There is no stamina pool; the prompt puts
  it explicitly out of scope and it is a design decision the owner has not asked for.
- **Jump apex 0.600 m** is 20–50% above a real standing vertical. A 0.45 m jump does not read on
  screen. The takeoff velocity, 3.43 m/s, is within 15% of the measured human one, and that is the
  half that governs how it *looks*.
- **Acceleration 10 m/s²** is the top of reality and the bottom of the games band. **Decided
  against**: the research's own critically-damped velocity spring. At sprint speed a 0.25–0.5 s time
  constant implies a peak acceleration of v/τ = 14–28 m/s² — above the 10 m/s² the same document
  boxes as the human limit — and it makes "time to sprint" an asymptote rather than a number the
  tuning declares. A constant cap sits exactly on the human figure and gives `t = v/a`: 0.70 s up,
  0.50 s down, which is what the test asserts.

`walk_speed_factor` is **deleted**, not deprecated. The product it named is the thing that was
wrong, so leaving it reachable would leave the bug reachable. `boost_factor` became
`fly_boost_factor` for the same reason: it now applies to exactly one mode and says so.

### What the scale change broke, which is the interesting part

Six things failed, and every one of them was a constant that had silently encoded the old gravity or
the old speed. None was found by reading; all six were found by running.

1. **`test_spectator_camera`'s "huge dt" tunnelling guard stopped testing anything.** It dropped the
   body from a literal `y = 5.0` and asserted it landed within one 0.5 s step. One step falls
   `0.5·|g|·dt²` — 4.00 m under −32 and **1.23 m** under Earth's, so the body simply never reached
   the ground and the case failed on a velocity assertion. The start height is now *derived* from
   gravity (80% of one step's fall above the eye height), which is the only configuration that
   tests tunnelling at all, and it cannot rot again.
2. **The jump-apex band, twice** — once in `test_player_state` (closed form) and once in
   `test_controller` (integrated). Both moved to 0.600 m. The integrated one revealed something
   worth keeping: it now reads **0.6283 against the closed form's 0.6000, a 4.7% overshoot**,
   because the jump impulse lands inside a tick that has *already* paid its gravity decrement. That
   was equally true before; at a 1.13 m apex the same 4.7% hid inside a band 0.25 m wide.
3. **Two `test_controller` cases were sized in ticks against the old speed** — 60 ticks was 10 m at
   10 m/s and is 1.4 m at the new one. They now derive their budget from the tuning
   (`ticks_to_walk`), so a speed change moves them automatically.
4. **The shore-pop case passed for the wrong reason, waiting to happen.** Under Earth gravity the
   swimmer floats higher, high enough that the *mesh path's* 0.55 m step budget climbs a 0.5 m
   shore lip outright — so the assist never fired and the case would have gone green while testing
   nothing. It now sets the SVO path's 4 cm budget, which is what ships.
5. **Water's buoyancy constant was a ratio dressed as an absolute.** `defs/water.hpp` said
   `64.0f` with a comment reading "upthrust is 2× gravity" — true only while gravity was −32. With
   Earth gravity the net upthrust went from +32 to **+54.2 m/s²** and the swimmer was fired out of
   the sea and onto the shore lip within a hundred ticks. It is `19.62` now (= 2 × 9.81), and
   because neither header can see the other, the *relationship* is pinned by a test in
   `world/player` that links both. The real-world figure, recorded for honesty: a human is nearly
   neutrally buoyant (~0.015 g net), so +1 g is game scale — but it is the *same* game scale that
   shipped before. Only its spelling changed.
6. **`--speed-scale` silently stopped scaling the body.** The flag goal 229 added multiplies
   `move_speed`, and goal 232 moved walking off `move_speed`. For one build the instrument that
   exists to stress the sweep at 40× was scaling nothing but the fly camera. It now scales
   `walk_speed`, `sprint_speed` and both acceleration rates together, so *time* to speed is
   invariant and the ramp does not swallow the run at 40×.

That last one is the third instrument in three prompts to quietly measure nothing (after
`walk_violations` and the JSON brace check), and the second in this prompt alone (after the
inside-solid counter measuring its own float round trip). The pattern is now well enough evidenced
to state as a rule: **when the thing being measured changes, the measuring apparatus is part of the
blast radius, and nothing warns you.**

### Scenario coverage had to move with the speed

`hold forward 8` covered 80 m at 10 m/s and covers 11 m at 1.4. Three scenarios were named for
terrain features they would have stopped reaching. Rather than triple their script seconds (and
their CI cost), the *traverse* legs now **sprint** — 7 m/s crosses the same ground in the same time
and exercises goal 234's ramp while doing it. `walk_shoreline`'s entry into the sea deliberately
still walks: entering water at a sprint is a different test from the one that scenario is named for.

### FOV follows speed, not the key (goal 234)

The FOV kick keyed off `PlayerIntent::boost`. With sprint now a ramp that would snap the lens open
most of a second before the body got there — and hold it open while sprinting into a wall at zero
speed. It is `clamp((speed − walk_speed) / (sprint_speed − walk_speed), 0, 1)` now. The head-bob's
full-scale speed came off the same fix: it was the literal `10.0f`, i.e. the old walking speed
hard-coded a third time.

---

## 8. What collision actually costs, and why (goal 230, re-measured)

The first measurement said 0.051 ms/tick and called the 0.20 ms budget met. With realistic speeds
`walk_shoreline` reads **0.196–0.276 ms/tick across five runs** — over budget, consistently. Chasing
that produced the most useful number in this section, and two wrong hypotheses first.

**Wrong hypothesis 1: sub-steps, so cost scales with speed.** Falsified by ramping the same
scenario, which is **not monotone**:

| `--speed-scale` | walking speed | collision ms/tick |
|---|---|---|
| 0.2× | 0.28 m/s | **0.040** |
| 1× | 1.4 m/s | **0.226** |
| 4× | 5.6 m/s | **0.073** |

Cost *peaks in the middle*. What is special about 1× is not the speed: it is that at 1× the body
spends the run in and beside the water, at 0.2× it never gets there, and at 4× it crosses in
seconds.

**Wrong hypothesis 2: the bisection.** A blocked axis ran all 12 halvings unconditionally — 1/4096
of the motion, or **3 micrometres** for a walking tick's 12 mm, against a 7.8 mm voxel. That is real
waste and it is fixed (the search now stops once the interval is under 0.1 mm, a tenth of the
contact skin and 78× finer than a voxel; the count is a ceiling, so a 2.7 m boost-fly frame still
gets all 12). Measured gain: **~8%**. Correct, and nowhere near the explanation.

**What it actually is**, from counters added to `OctreeCollider` for the purpose:

| scenario | queries/tick | nodes/query | nodes/tick | ms/tick |
|---|---|---|---|---|
| `walk_shoreline` | **5.8** | **76.5** | 444 | **0.218** |
| `walk_hillside` | 20.5 | 34.2 | **701** | 0.061 |
| `clip_stress` | 17.7 | 33.5 | 594 | 0.116 |
| `spawn_stand` | 8.0 | 52.8 | 422 | 0.026 |

`walk_hillside` does **more total node visits than `walk_shoreline` (701 vs 444) at less than a
third of the cost.** So neither the number of queries nor the number of nodes is the unit of cost —
which is exactly the kind of result that kills a plausible story. What separates them is *what the
query finds*: `overlaps_solid` early-outs on the first solid voxel, and over water there is no solid
voxel to find, so the traversal runs to exhaustion through dense liquid brick leaves that must be
examined and rejected one occupancy mask at a time. **The expensive query is the one that finds
nothing.**

**Honest position for goal 230**: the 0.20 ms/tick budget is met everywhere the body is on land —
0.026 to 0.116 across the scenario set — and **missed, at 0.20–0.28, whenever the body is over deep
water.** That is a real budget miss, measured, attributed, and not fixed here: the fix is a
"contains solid" summary bit on the node header so a water-only subtree can be rejected at its root,
which is a tree-layout change and belongs with Prompt 004's work on that layout, not bolted on here.
Goal 244 opened.

---

## 9. The walkable slope, and what it revealed about the terrain (goal 235)

**The limit is 40°**, and the band it was chosen from:

- Minetti's gradient measurements span ±45% grade = **±24.2°** — but that is the limit of a treadmill
  protocol, not of human capability. People walk up steeper scree than any treadmill will tilt to.
  Taking 24° as a hard limit would over-read the source.
- Mountain paths statistically optimise at 20–30% grade (**11–17°**). That is the *comfortable*
  gradient, not the possible one.
- Shipped engines default near 45° (Unreal 44.765, Source 45.57) — a number that exists for level
  design with 45° ramps. This terrain has no ramps.

40° sits above every hill the generator makes and below every cliff, which is the behaviour that
reads as "I can walk up that, I cannot walk up **that**".

### The limit and the slide are one number, not two

Above the limit the body slides, and the slide's strength is *derived from the limit* rather than
tuned beside it: the walkable limit **is** the friction angle, so net downslope acceleration is

> `a = g·(sin θ − tan(limit)·cos θ)`

which is exactly zero at the limit and grows above it. A separate "limit" and "slide strength" could
disagree with each other; this cannot. Terminal speed is capped at `max_slide_speed` — a slide is
not faster than a run — and on walkable ground the accumulated slide *bleeds off* at the braking
rate rather than stopping dead, because stepping from a 41° face onto a 39° one should not be a wall.

Three things had to change together, and the third is the one that matters:

1. The uphill component of the wish is refused before the acceleration ramp sees it — you can still
   move *across* and *down* a steep face, just not up it.
2. **The step-up gets no budget on steep ground.** This is the whole reason the limit was toothless
   before it existed: at 7.8 mm voxels every slope is a sub-centimetre staircase, and a 4 cm step
   budget climbs *any* staircase. Leaving the step-up on would have let the body walk straight up a
   face the slope check had just refused.
3. The slope comes from the **analytic** heightfield, by central difference at 1 m — deliberately,
   even though the body collides against the voxelised world. The collision surface at 7.8 mm has a
   local slope of either 0° or 90° and nothing in between, so asking it "how steep is this hill"
   returns noise. The analytic field is the smooth truth those voxels approximate.

### What it measured about the terrain, which was not the point but is the finding

`walk_hillside` is named for a hillside. Along its own line, from `tools/svo_render --xz`:

| x (at z = 16) | 0 | 4 | 8 | 12 | 16 | 20 | 24 |
|---|---|---|---|---|---|---|---|
| surface (m) | 25.88 | 33.61 | 39.91 | 50.38 | 61.87 | 69.22 | 69.63 |
| slope | — | **62.6°** | **57.6°** | **69.1°** | **70.8°** | **61.4°** | 5.9° |

**It is not a hillside. It is a 57–71° cliff**, and the body had been walking up it at 10 m/s purely
because the step-up climbs staircases. The shoreline is the same story: 1.16 m at x = 111 to −0.49 m
at x = 112 is **58.8°**.

That is a finding for Prompt 006, not a reason to pick a limit that lets the body climb cliffs: **the
current generator produces terrain that is mostly unwalkable at any realistic slope limit.** Recorded
here so 006 inherits it as a requirement rather than rediscovering it.

A side effect worth having: with the step-up no longer running on steep ground, `walk_hillside`'s
collision cost fell from **0.061 to 0.040 ms/tick** and its query count from 20.5 to 8.7 per tick.

### The check

`walk_cliff` walks straight into the measured 57–71° face and stays at the bottom of it. Captures:
`research/captures/aj_slope_limit_refused.png` (the shipped 40° limit — the body pressed against a
wall of individual cubes after sprinting at it for six seconds) and
`aj_slope_limit_climbed.png` (the same scenario under `--max-walk-slope 89`, which climbs it). The
flag exists so that comparison is a demonstration rather than a claim.

Unit tests cover the ladder the prompt asked for — 20/30/40/50/60° compared against the **declared**
limit rather than a literal, so retuning the limit retunes the expectation — plus the stability
requirement (2° either side, 60 ticks, **zero** state changes), the friction-cone identity (exactly
zero slide at the limit), the cap, the bleed-off, and that an airborne body is not sliding.

---

## 10. The waterline, where two fixes were needed and the first one was for the wrong bug (goal 236)

The prompt's suspicion was specific: `inWater` is `feetY < surface && ground_height < surface`, and
since goal 197 that surface **moves**, so a body at the waterline could cross the predicate twice per
wave. `waterline_hold` stands at x = 111.5 (ground 0.37 m, sea level 0) for 30 s and counts stance
transitions.

**Measured: 33 transitions**, against a dominant wave period of 2.86 s — about 10.5 crests in the
run. So the flicker was real and roughly three times per crest.

I fixed the predicate first, with hysteresis that has a physical reading — you start swimming once
you are thigh-deep (0.6 m) and stop once only your shins are under (0.2 m), rather than crossing one
threshold in both directions.

**It moved nothing. The count went to 33.** So I did what this pass has now had to do four times:
stopped guessing and made the counter say *which* transition:

```
stance: 33 transitions -- grounded->airborne:16  airborne->grounded:16  airborne->swimming:1
```

**Sixteen and sixteen, and exactly one involving water.** The waterline had nothing to do with it.
The shore at x = 111.5 is a **58.8° slope** (goal 235's own measurement), so the body was *sliding*,
and a sliding body loses contact with the surface for a tick at a time. Every one of those ticks read
Airborne, so the stance oscillated at ~1.9 s — and the landing dip and the coyote timer both react to
that, which makes it a real defect and not just a noisy counter.

The fix is one line and it follows from goal 235's own model: **a body sliding down a face is in
contact with it**, so a `sliding` tick that the sweep did not ground keeps its Grounded stance.
`result.sliding` already requires contact within coyote time, so a body that slides off the bottom of
a cliff into real air still goes Airborne within 0.1 s.

**33 → 4.** And the four are *distinct* one-off transitions — the spawn settling, then entering the
sea — not a repeating pair. Against 10.5 crests, the prompt's criterion ("not more than once per
crest") is met with a factor of 2.6 to spare.

The hysteresis stays. It was a fix for a bug that was not firing yet, but it is correct on its own
terms and it is the reason exactly one of the original 33 involved water instead of several.

### Three sea states

`swim_cycle` walks off the land into the sea, swims out, turns, swims back and climbs out, under
`--ramp wind-speed:0.5,3,8`. Wind and waves share one direction and one strength, so wind speed *is*
sea state: glass, chop, swell.

| wind | stance transitions | inside-solid | frame contrast |
|---|---|---|---|
| 0.5 m/s | 11 | **0** | 54.6% |
| 3 m/s | 11 | **0** | 43.2% |
| 8 m/s | 9 | **0** | 35.3% |

Zero inside-solid events at every sea state, and the transition count does not grow with the waves —
if the surf were driving the predicate it would.

**"Zero stuck frames" was operationalised as** zero inside-solid ticks plus zero walk violations plus
a bounded stance count, because there is no stuck-detector in the harness and inventing one for a
single check would be worse than saying what was actually measured. A body genuinely stuck against a
shore lip shows up as inside-solid events or as the run failing to reach its capture; neither
happened at any of the three.

---

---

## 11. The golden mechanism only works at rest (measured while re-taking them)

Re-taking every golden for AJ-B forced a question Prompt 002 never asked directly: **is a golden of a
MOVING scenario reproducible at all?** Two identical back-to-back runs of each, same build, same
backend, same machine:

| capture | % pixels changed vs its own golden |
|---|---|
| `spawn_stand` first_light / final | **0.062 / 0.107** |
| `valley_far` (static pose) | **0.083** |
| `walk_shoreline` first_light / on_ground | **0.017 / 0.011** |
| `fly_orbit` quarter | **0.0001** |
| `fly_orbit` final | 8.2 |
| `walk_shoreline` final | 15.5 |
| `fly_transect` early / mid / final | 11.8 / 15.6 / 5.3 |
| `walk_hillside` on_ground / final | 12.8 / **35.5** |
| `walk_cliff`, `waterline_hold`, `swim_cycle` | 34.6 / 32.6 / 44.9 |

The gate is 1.5%. **Captures at rest reproduce two to four orders of magnitude inside it; captures
after sustained motion miss it by ten to twenty times.** The `wait 3` settle Prompt 002 added is not
enough — `walk_cliff` waits two seconds and still reads 34.6%.

The cause is the one already on the board: at speed the body outruns the LOD rebuild, so *which tree
is resident when it stops* decides what it sees, and that depends on wall-clock timing. Prompt 002
saw one instance of this (fly_orbit's vk final differing 70.4% from its own golden while d3d12's was
bit-identical) and treated it as a settling problem. It is not; it is the rebuild storm, and no
amount of waiting fixes it while a rebuild can still be in flight.

So the goldens for moving captures are **deleted, not loosened**. Loosening the gate to 40% would
make it detect nothing at all; a golden that cannot fail is the vacuous-check pattern this pass has
now hit four times. Twenty golden files remain, all of captures taken at rest, and eight scenarios
carry a `GOLDEN POLICY` note saying which of their captures are for eyeballing only and why. **Goal
246**: restore them once Prompt 004 removes the storm, which is also the point at which they become
worth having.

---

## 12. The crosshair asks the octree, and stops claiming what the eye cannot check (goals 241, 242)

**241.** The aim query re-derived the world from the height function. That was correct and it was
also a *second opinion*: the crosshair could disagree with the body about what is there, and on the
svo path it does by construction, because the tree carries LOD and edits and the height function
carries neither. `query_aim_octree` asks `world::svo::trace_ray` — the same traversal the body
collides against and the same one the shader mirrors — with the LOD early-out and the smoothing
both **off**, because the readout wants the voxel that is there, not the cube a distant pixel would
be shaded with. The analytic march survives for the mesh path and as the tests' second opinion,
which is exactly what the goal's own check uses it for.

**The check, and the three wrong answers it gave first.** Over 2,000 random rays in a real region at
uniform LOD, comparing the material the readout names against `BrickTree::material_at` at the point
it reports:

| probe method | mismatches / 1272 hits |
|---|---|
| a quarter voxel **along the ray** | 27 (2.1%) |
| a quarter voxel **along the hit face normal** | 2 |
| snapped to that voxel's **centre** | 2 |
| ...and handling `t == 0` | **0** |

Each step named a different thing, and none of them was a defect in the query:

1. The 27 were all `Stone`-vs-`Dirt` or `Dirt`-vs-`Air` at brick level, no LOD cube, no solid leaf.
   Those are **vertically adjacent material bands**: on a shallow ray a quarter voxel of forward
   travel crosses into the next voxel down, so the check was sampling a different voxel from the one
   that was hit. The face normal is the only direction guaranteed to point *into* the hit voxel.
2. Snapping to the voxel centre removed boundary ambiguity but changed nothing, which was itself
   informative — it ruled out rounding.
3. The last two printed **`t = 0.0000`**. The ray *started inside solid*: the tracer reports the
   origin's own voxel immediately and its "face normal" is a default, not a surface it crossed.
   Origins uniform over a region put some underground; a real crosshair's origin is the camera, and
   goal 228's counter asserts every tick that the camera is not inside solid.

Final: **1272 / 1272 material matches, 0 mismatches.** A second case compares the octree query
against the analytic one straight down over 92 land columns: **92 agreed**.

**242.** The readout's range was **300 m**, a round number nobody derived. It is now
`kAimResolvableRange` = **34 m**, from `human-eye-and-vision-research.md` Part 1 §8.1: a **1 cm
detail is resolvable to 34 m at 20/20** (1 arcmin MAR, d = s / tan θ). One centimetre is the scale
of the detail that distinguishes one material from another here — a 7.8 mm voxel — so **beyond 34 m,
naming the material is a claim the eye cannot check.**

Rejected criteria, with the reason:

- **54 m** — the same 1 cm detail at the 0.64′ MAR of the 94-ppd young-observer ceiling. A real
  number describing the best measured eye rather than the nominal one. `--aim-range` exists for it.
- **619 m** — an 18 cm face as a resolvable *blob*. Wrong criterion: detecting that something is
  there is not identifying what it is made of, which is what this readout claims.
- **1719 m** — a 0.5 m tree trunk at 1 arcmin. Same objection, further out.

A test re-derives both distances from the arithmetic rather than trusting the constants, and asserts
the readout goes blank past its range and hits the same column with the old 300 m — so "blank" is
demonstrably a range limit and not a missing surface. Four pre-existing cases now state `300.0f`
explicitly; their subject is classification, not range, and the range is tested on its own.

---

## 13. The view polish, measured against thresholds — and the instrument that was switched off (goal 240)

Every polish constant now sits against a number from
`research/human-movement-and-perception-research.md` Part 2: **vertical translation is detected at
~2.13 cm/s** (median, 2AFC), and **retinal slip costs acuity past ~4 °/s**, degrading rapidly past 6.

| constant | was | is | against threshold | verdict |
|---|---|---|---|---|
| `bob_frequency` | 1.9 cyc/m | **1.36** | 1.9 Hz at walking pace = the measured human step frequency | perceptible, intended |
| `bob_amplitude` | 0.025 m | **0.020** | 24 cm/s peak = **11× detection**; 3.4 °/s gaze = **under the 4 °/s acuity threshold** | perceptible, intended |
| `landing_dip_*` | max 0.06 | **max 0.045** | 3.4 cm dip at a 3.43 m/s landing → 29 cm/s = **13× detection** | perceptible, intended |
| `eye_smooth_tau` | 0.10 s | 0.10 s | hides a ~90 Hz, 7.8 mm staircase whose velocity is orders past detection | perceptible, intended — it buys removal of a larger artefact |
| `eye_smooth_max_lag` | 0.25 m | **0.05 m** | see below | was 32 voxels for a 7.8 mm problem |
| `fov_kick_radians` | 0.075 | 0.075 | 4.3° vertical ≈ 7.5° hFOV, inside the +5–8° recommendation | perceptible, intended |
| `polish_max_offset` | 0.05 m | 0.05 m | a budget, not an effect | keep |

Nothing was below threshold, so nothing was deleted. Three constants moved, and two of the three
moves came from things the capture sequence showed that no still would have.

### The bob was running at 19 Hz until goal 232

`bob_frequency` is cycles per **metre**, so its temporal frequency scales with speed. At the old
10 m/s walk it was **19 Hz** — not a bob, a flicker. Goal 232's 1.4 m/s made it 2.66 Hz by accident;
1.36 cyc/m puts it at 1.9 Hz deliberately. The amplitude then follows from the acuity constraint
rather than taste: A ≤ tan(4°)·4 m / (2π·1.9) = 0.0234 m, and the shipped 0.025 at 2.66 Hz was
producing **5.97 °/s** — past the acuity threshold, into the rapid-degradation band.

### The captured landing, and what it found

Ten frames across a landing (`landing_strip`, `research/captures/aj_landing_strip.png`), with the
render-only eye offset logged at each. Prompt 001 shipped A3/A6 *without* a sequence, on the honest
grounds that a 4 cm effect at a 0.1 s time constant is not something a still shows. It is not — and
the sequence found two things:

**(a) The eye could be 11 cm from the body.** First run, peak total offset **+0.1117 m**. Two
independent clamps were stacking: `polish_max_offset` (0.05) and `eye_smooth_max_lag` (0.25), for a
combined 0.30 m that was nobody's decision. Worse, they were pulling *opposite ways*: the landing
dip pulled the eye down 3.4 cm while the smoothing held it up 11 cm, so a landing read as the view
**floating rather than absorbing** — the exact opposite of the effect's purpose. `eye_smooth_max_lag`
is 0.05 now (6 voxels, ample for the staircase it exists for), and a landing zeroes the smoothing
outright, because the dip is the term that owns a landing. Peak offset **0.1117 → 0.0559 m**.

**(b) The polish was switched off in every scenario, and had been all along.** With the two terms
reported separately the answer was flat: **polish +0.0000 on all ten frames.** The gate was
`view_polish && !autofly && !verify_frame`, and the harness sets `verify_frame` on every scenario —
that is where the contrast metric comes from. So the instrument built to photograph the view polish
photographed it turned off.

The `!verify_frame` term was over-cautious rather than wrong-headed, and the argument for removing
it is checkable: the mechanical counters read the **physical** body (`transform.position`), and the
polish is added to the **camera copy**. It cannot move what they measure. `!autofly` stays, because
that is a streaming smoke test whose documented behaviour is a bare camera.

**That is the fifth instrument in this prompt to quietly measure nothing** — after `walk_violations`
never being written, the JSON brace-balance check, the inside-solid counter measuring its own float
round-trip, and `--speed-scale` scaling only the fly camera. The rate is the finding at this point:
roughly one per group, always discovered by making the instrument disagree with something rather
than by reading it.

### Two workflow defects the same task exposed

- **`--accept-golden` kept resurrecting deleted goldens.** §11's policy (no goldens for moving
  captures) was a comment plus a manual `rm`, and the tool silently undid it twice in one pass. It
  is now a property of the scenario: `capture <when> <name> no-golden` parses, round-trips through
  `emit_scenario`, and beats `--accept-golden`. A policy a tool can undo is not a policy.
- **`ctest -j` flaked on the scenario tests.** `valley_far` failed its golden under `-j 2` and then
  passed three consecutive standalone runs at 0.002%. Two scenarios driving one GPU at once move
  frame timing enough to move a golden. The scenario tests now take `RESOURCE_LOCK gpu`, so they
  serialise against each other and nothing else.

---

## 14. What Group AJ did NOT do

Stated plainly rather than left to inference:

- **AJ-C 237 (crosshair-metered auto-exposure) and 238 (adaptation-scaled bloom): NOT DONE.**
  There is no auto-exposure in this renderer at all -- no metering pass, no adaptation state, no
  exposure input to the tonemap -- so 237 is a feature to build rather than a term to re-point, and
  238 depends on its output. Everything else in AJ-C and all of AJ-D is done (sections 12 and 13,
  plus the decided-against entries in `docs/progress.md` for goals 239 and 243).
- **234's uphill-sprint slope relationship** (Minetti's polynomial at 0 / 15 / 30 degrees) is not
  asserted. It depends on slope-aware *speed*, which is a different thing from the slope *limit*
  goal 235 built, and nothing yet reads the polynomial.
- **The terrain is mostly unwalkable** at the limit goal 235 chose -- 57-71 degrees on the scenario
  named "hillside". That is recorded as a requirement for Prompt 006 rather than worked around here.
- **Collision over deep water misses its budget** (goal 244), for a reason attributed but not fixed.
- **Moving captures have no goldens** (goal 246, section 11), because they cannot reproduce yet.
