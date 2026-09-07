# Frame time — decision log

Prompt 004, Group AK (goals 244–275). **In progress**: AK-A (244–248) is done. Written the way this
repo's logs are: every measurement, every "decided against", and the things that turned out to be
wrong — including the ones I got wrong.

---

## 1. The first measurement changed the question (goal 244)

**Vsync was hardcoded.** `RenderContext::present()` called `swapchain->Present(1)` with the comment
"vsync on -- correctness over speed is M1.4's own done-when". With that in place, `present` time is
the *panel's*, not the renderer's, and every frame-time percentile downstream measures the display:
a 6 ms frame reads as 6 ms and a 7 ms frame reads as 12, which is a step function, not a signal.
It is a setting now (`--vsync` / `--no-vsync`, on by default because that is the shipping
behaviour), and every number below is measured with it **off**.

### And the headline number is not what the brief expected

The brief's baseline table says **76 fps / 13.15 ms**, and its instruction was: *"76 fps at 13.15 ms
with only 3.2–6.3 ms of GPU march+resolve means roughly half the frame is not the marcher. Before
you optimise the marcher, find out what the other 7–10 ms is."*

Measured on `stress_pose` today, vsync off, RelWithDebInfo, vk:

| | value |
|---|---|
| frame ms mean / median | 5.36 / **5.21** |
| frame p95 / p99 / max | 7.09 / 10.23 / 13.68 |
| GPU march (median) | **4.93** |
| GPU whole-frame range | 5.19 |
| phase coverage | 96.6% |

**The median frame is 5.21 ms — 192 fps — and 4.93 ms of it is the marcher.** There is no missing
7–10 ms. The gap the brief asked me to hunt does not exist on this build: the frame is
**GPU-bound on the march**, and the 76 fps baseline predates Prompt 002 (which turned the ImGui
overlay off for scenarios) and Prompt 003. Vsync on vs off at the same pose is 5.46 vs 5.21 ms
median — so vsync was *not* manufacturing the old number either, at this pose.

**Verdict, in one sentence: the frame is GPU-bound on the primary+secondary march, the average is
already comfortably past the 150 fps target, and the owner's complaint is entirely about variance.**
That reframes the whole prompt, and it is why AK-A comes first.

---

## 2. Secondary rays are half the marcher (goal 246)

Six traversals per shaded pixel (1 primary + 1 shadow + 4 AO). `stress_pose`, march GPU ms:

| configuration | vk | d3d12 |
|---|---|---|
| shadows + AO (shipping) | 4.90 | 5.12 |
| `--no-ao` | 3.09 | 3.70 |
| `--no-shadows` | 4.41 | 4.83 |
| both off (primary only) | **2.50** | **3.18** |

- **AO costs 1.81 ms — 37% of the marcher** — for four rays, so ~0.45 ms per AO ray class.
- **Shadows cost 0.49 ms — 10%** — for one ray.
- **Secondary rays together are 49% (vk) / 38% (d3d12) of the march.**
- The primary ray alone is 2.50 ms, i.e. 400 fps. **Even the primary is not the bottleneck at this
  pose**, which is the reframing goal 246 asked for and it should be said plainly: any plan that
  optimises primary traversal is optimising the smaller half.

---

## 3. The rebuild storm is real, and it owns the p99 (goal 247)

`fly_transect`, vsync off, with and without world rebuilds (`--no-rebuild` freezes the tree, which
makes the world go stale — that is the point):

| | rebuild ON | rebuild OFF |
|---|---|---|
| mean | 4.97 | 4.42 |
| median | 4.64 | 4.01 |
| p95 | 7.17 | 5.96 |
| **p99** | **13.16** | **6.89** |
| trees built | 3 | 1 (the initial one) |
| slow frames (>20 ms) | 7 of 2014 | 3 of 2265 |
| ...caused by upload or build | **5** | **0** |

**p99 halves, and every upload- and build-caused stall disappears.** The diagnosis in the brief's §0
is confirmed. What is left with rebuilds off is three frames, two of which are the harness's own PNG
capture (below) and one a 35 ms `present`.

### The 180 ms frames were my own instrument, and finding that out was the useful part

Both conditions showed a **max near 180 ms**, unchanged by suppressing rebuilds — which looked like
a second, larger problem hiding behind the first. The slow-frame line printed seven phases summing
to **0.7 ms of 180**, so the frame was 99.6% unaccounted.

It is the **`capture` phase**: a staging copy, a `WaitForIdle` and libpng, which `CLAUDE.md` already
documents at 200+ ms and which Prompt 002 added as an eighth phase. The *exit summary* included it;
the *live slow-frame line* still printed the original seven. A breakdown that does not add up to its
own total is not a breakdown, and this one cost a hypothesis before I noticed the exit summary
disagreed with it. `capture` is in that line now.

**So there is no mystery stall.** Excluding capture frames, the worst real frames are: with rebuilds
on, 39.0 ms (upload), 24.8 (post, while building), 22.7 (upload), 21.1, 20.0 — all rebuild-related;
with rebuilds off, 35.4 / 15.2 / 11.3 ms, all `present`, no upload or build cause at all.

### A vacuous measurement I caught before reporting it

The first attempt at this comparison produced *identical* numbers for both conditions, including
"3 trees built" with rebuilds supposedly off. The cause: my scenario-copy step used
`sed 's|^option --no-taa$|...|'` to inject flags, and **`fly_transect.scn` has no `option` lines at
all**, so the substitution matched nothing and both runs were the default configuration. The
`stress_pose` measurements in §1 and §2 are unaffected (that file does have the line), and the
fly_transect numbers above were re-taken by inserting after `backend`, which every scenario has.

That is the sixth vacuous instrument in this arc, and the first I caught by checking the instrument
rather than by disbelieving the result — the check was "does the flag appear in the file I actually
ran", which took one grep.

---

## 4. The rebuild storm, stopped (goals 249–253) — a stopgap, labelled as one

`> lod_radius * 0.5f` tied how *often* the world is rebuilt to a *detail* parameter, and at the 4 m
default that asked for a full 400 MB rebuild every **2 metres**. The trigger is now a named policy on
`SvoWorldOptions` with four parts: a trigger distance, hysteresis, a minimum interval measured from
the last **adoption** (not the last build start — the upload is 13–21 frames of `UpdateBuffer`
traffic after the build ends), and a speed gate.

### What it bought

`fly_transect`, vsync off, vk:

| | before | after |
|---|---|---|
| median | 4.64 | **4.02** |
| p95 | 7.17 | **5.89** |
| **p99** | **13.16** | **8.52** |
| slow frames (>20 ms) | 7 of 2014 | **4 of 2271** |
| ...caused by upload or build | 5 | **2** |
| trees built | 3 | 2 |

**p99 −35%**, against a rebuilds-*disabled* floor of 6.89 ms — so this closes about two thirds of the
gap between the shipped behaviour and never rebuilding at all.

### The trigger distance turned out not to be the lever, and the ramp is what showed it

I shipped 24 m first, on the reasoning that a 2 s build at 40 m/s is outrun after 80 m anyway. Then
I ramped it, because a number chosen by argument is not a measurement:

| trigger | p99 (ms) |
|---|---|
| 4 m | 9.32 |
| 8 m | 8.19 |
| 16 m | 8.06 |
| 24 m | 8.28 |

**Flat, inside the run-to-run spread.** The reason is the speed gate: during a fast flight *no*
rebuild triggers at any distance, so the trigger distance only decides behaviour while the camera is
moving **slowly** — which is exactly when a rebuild is cheap to absorb. **The AK-B win is goal 250's
speed gate, not goal 249's distance.**

That mattered, because the distance is *not* free in the other dimension. Rendering the same pose
with the LOD centre displaced (`tools/svo_render --lod-center`, the deterministic reproduction
CLAUDE.md names for exactly this):

| LOD-centre offset | staleness, mean levels | pixels changed |
|---|---|---|
| 4 m | 0.03 | 0.2% |
| **8 m** | **0.27** | **2.9%** |
| 16 m | 5.11 | 36.7% |
| 24 m | 13.70 | **45.3%** |

**Free to 8 m, off a cliff by 16.** Capture: `research/captures/ak_lod_staleness.png` — at 24 m the
near-field terrain is visibly chunky, which is the cost my first draft would have shipped silently.

**So the shipped value is 8 m**: all of the frame-time win, and 94% of the image quality 24 m threw
away. Two measurements, opposite directions, one obvious answer.

### Goal 250's check FAILS, and the reason is structural

The check is "zero rebuilds completing more than one trigger-distance behind the camera". Measured
adopt lag on `fly_transect`: **135.2 m and 33.9 m**, against an 8 m trigger. Both fail.

- The 135 m one is the **initial** build, requested at spawn and adopted after the camera has already
  flown; no trigger policy governs it.
- The 33.9 m one is a build that triggered while the camera was slow and then **accelerated during
  the ~2 s build**.

**Deferring cannot bound the adopt lag, because the camera's speed after the trigger is not knowable
at trigger time.** I chose deferral over velocity prediction (goal 250 permits either) because
prediction needs a reliable build-duration estimate to extrapolate by, and the measured duration
varies **1.89–3.60 s** with terrain complexity — a prediction would be wrong by tens of metres
exactly when it matters. But deferral does not fix it either. **This is a structural limit of
"rebuild the whole world around a point", and it is the argument for AK-C/D rather than a tuning
failure.**

### Goal 251: decided against, with the numbers

`build_job` calls `sampler.set_focus(camera, 4 * lod_radius)`, rebuilding a 1/16 m height field over
a 16 m radius on every build, and 251 asks for cross-build reuse. Measured first:
**`sampler` is 0.15–0.19 s of a 1.89–3.60 s build — 5–9%.**

Reuse would recover at most part of that, and only when consecutive focus regions overlap — which,
with the trigger now at 8 m and the speed gate suppressing rebuilds during flight, is a handful of
builds per minute. Against `fill_brick` at ~60% of build time (goal 162's own note), this is the
wrong 8% to optimise. **Decided against, and the number is why.** It becomes worth doing if AK-C's
per-cell rebuild makes builds frequent and small, which is the opposite regime.

### Goal 161: fixed, and the test that hid it now cannot

`fill_terrain` truncated the surface height toward zero (`static_cast<int32_t>`) instead of flooring,
so a column at −3.4 m became −3: **underwater terrain sat one voxel high, everywhere below sea
level.** Above sea level truncation and flooring agree, which is precisely why the sampler-vs-fill
equivalence test — which **skipped every negative-height column** — never saw it.

Both fixed together: `fill_terrain` floors, and the test's skip is deleted. It now compares
**196,608 voxels with 0 skipped** (was ~2/3 of that with the underwater third excluded). A test that
excludes the region where two implementations disagree is not an equivalence test.

### Goal 253: what this did NOT fix

- The world still goes **stale between rebuilds** — 2.9% of pixels at the shipped 8 m trigger, and
  the capture above shows what the 24 m version would have looked like.
- The **adopt lag is unbounded** (above), so a fast flier is always looking at a tree centred where
  they were, not where they are.
- The **whole-tree cost is untouched**: still 1.89–3.60 s of CPU and 431–688 MB re-uploaded per
  rebuild. AK-B makes it happen less often; it does not make it cheaper.
- The single worst frames are still the **tree swap and its upload** (2 of the 4 remaining slow
  frames), because adopting a tree still means creating and filling GPU buffers for the whole world.
