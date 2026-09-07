# Player embodiment — decision log

Prompt 003, Group AJ (goals 225–243). **Partial**: AJ-A is complete (225–230, including 229a)
and AJ-B's first goal (231) is done; 232–243 are not. What is here is written the same way
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
