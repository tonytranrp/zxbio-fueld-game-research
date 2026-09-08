# The fine-grain look — decision log (Prompt 005, Group AL)

The owner's complaint, in his words: *"I'm still seeing like blocks and rigid cubes and such"*,
*"nó nhìn nó giống như cục, cục à"* ("it looks like lumps, lumps"), and the target —
*"the checker board is like so retro ... but instead of making it similar to that we changed it to a
much finer grain which are still that style but smaller."*

Backlog and Checks: `docs/goals.md` Group AL. What each appearance term is and what filters it:
`docs/the-look.md`.

---

## 1. The moiré metric, built first and falsified twice before it was trusted (goal 276)

Without a number this whole prompt is opinion, and opinion is how the ribbon bug survived two
"verified" runs. But the number has to measure the right thing: **`--verify-frame`'s local-contrast
metric (34.7%) cannot do this job**, because a fine deliberate stipple and a field of ring moiré
both raise it — one is the look being asked for and the other is the artefact being complained
about.

### The first hypothesis was wrong, and printing the spectra is what showed it

**Hypothesis 1: ring moiré is a low-frequency ENVELOPE modulating a high-frequency carrier**, so
measuring the envelope's structure should separate wanted stipple from unwanted aliasing. It scored
the target capture 57.4 and the ringed frame 114.7 — a 2.0× separation — which looked like a result.

It is not. Restricting to textured pixels collapsed the separation to **1.28×**, and printing the two
radial spectra side by side showed why: **the envelopes are nearly identical.** Both put ~24% of
their energy at 64–256 px periods, and the ringed frame has *less* mid-band envelope energy, not
more. An intermediate version's apparent 3.9× separation turned out to be a peak-finder locking onto
the analysis band's own edge — measuring the roll-off, not rings.

### What the same printout showed instead

| radial period | target capture | ringed frame |
|---|---|---|
| **2–4 px** | **54.3%** | **91.5%** |
| 4–8 px | 41.9% | 7.7% |
| 8–16 px | 3.5% | 0.8% |

**The target's high-frequency energy is spread across 2–8 px; the aliased frame's is 91.5% crammed
against the pixel Nyquist limit.** That is what aliasing *is* — signal energy folded up at the
sampling rate — while a deliberate stipple sits at its own frequency with a roll-off either side.
So:

> **moiré ratio = E(period 2–4 px) / E(period 4–8 px), over textured pixels only**

### No FFT

The two bands are separated with a difference-of-box filter bank rather than a transform:
`L − box3(L)` passes roughly 2–4 px, `box3(L) − box7(L)` roughly 4–8 px. Validated against the FFT
form on eleven images — it reproduces the separation (**9.5× vs 9.2×**) without the transform, and a
box blur is a prefix sum, so the whole metric is a handful of linear passes and never shows up in the
frame report.

### Validation — the part that makes the number usable

| input | ratio | why this input |
|---|---|---|
| **`lin_water_checkerboard_after.png`** (the owner's target) | **1.144** | known-good |
| `lin_water_checkerboard_before.png` | 1.154 | same pose, the bug was on water only — it *should* match |
| `blocky_default.png` | 1.561 | close-up, cubes are meant to be cubes |
| `lin_final_vk.png` / `lin_final_d3d12.png` | 4.699 / 4.642 | the two backends agree to **1.2%** |
| **`svo_ground_hilltop.png`** (the frame complained about) | **10.877** | known-bad |

**Separation: 9.51×.**

And against synthetics whose answer is known by construction rather than by my opinion of a
screenshot — isotropic band-limited noise centred on a given radial period:

| synthetic | ratio |
|---|---|
| band noise, 6 px period (a wanted stipple) | **1.19** |
| band noise, 4 px | 5.84 |
| band noise, 2.3 px (aliasing) | **34.88** |
| white noise | 9.71 |

**The target capture's 1.144 lands on the 6 px synthetic's 1.19**, which independently says the
target's stipple has a ~6 px radial period — a number goal 284 needs and did not have to be measured
separately.

**It responds to supersampling**: the same CPU render at 1 spp scores 4.72 and at 9 spp 3.52. Only
**25% for 9× the samples** — which is itself a confirmed finding rather than a disappointment, and it
matches the Luanti report quoted in the research (*"FXAA does not help with that, and SSAA is way to
too expensive"*). **This needs pre-filtering, not more rays.**

### Two things the synthetic tests falsified, both worth keeping

- **A pure sinusoidal "stipple" at a 5 px period scores 12,957** — apparently catastrophic. It is
  not a bug: a 2D product of sines at period 5 has a *radial* period of 5/√2 = 3.54 px, which really
  is near Nyquist. The lesson is about the synthetic, not the metric — and it is why the validation
  above uses band-limited noise, which is what a real stipple is.
- **A textureless image scores 4.57 on pure numerical noise**, which would read as a failing frame.
  `MoireResult::valid` is false below a carrier floor and a caller must check it. This is pinned by a
  unit test so it cannot regress into a silent lie.

### Where it lives

`dev/harness/src/moire_metric.{hpp,cpp}`, four unit tests in `dev/harness/tests/`, a
`moire_ratio` scenario assertion, `moire_ratio` in every harness report, and
**`voxel_harness --moire FILE.png [--moire-crop-top N]`** — which is how the validation table above
was produced and how anyone can re-produce it.

### The baseline, on the three poses goal 276 names

| pose | moiré ratio |
|---|---|
| `stress_pose` (the hilltop) | **3.816** |
| `macro_ground` | 2.368 |
| `valley_far` | 4.333 |

**And a finding that belongs to Prompt 004, not this one:** the committed `svo_ground_hilltop.png`
measures 10.877, and the same pose today measures 3.816. That capture predates Prompt 004's goal 268,
which halved the AO ray count and clamped the ray length. **Two thirds of the moiré the owner
complained about was already removed by a change made for frame-time reasons**, and saying so now
prevents this pass from claiming it later.

---

## 2. The attribution: not one of the six named suspects (goal 277)

The prompt lists six candidates in the order it would try them: (a) the albedo mottle — *"the
strongest suspect"*, (b) the per-cube grain, (c) the shadow lift, (d) the AO's screen-space hash,
(e) the LOD early-out, (f) the smooth-normal availability cliff.

**Every one of them measures at zero.** The bisection, on the CPU reference at `stress_pose`
(640×360, no TAA, so this is the raw signal):

| configuration | moiré ratio | verdict |
|---|---|---|
| base | **3.127** | — |
| `--no-grain` | 3.127 | **identical — (b) contributes nothing** |
| `--no-mottle` | 3.134 | **no change — (a), the "strongest suspect", is innocent** |
| `--no-ao` | 3.140 | worse — (d) is not it |
| `--no-shadows` | 3.179 | worse — (c) is not it |
| `--no-ao --no-shadows` | 3.209 | worse |
| `--no-lod-march` | 3.127 | **identical — (e) is not it** |
| `--smooth-pixels 24` | 3.967 | *worse*: widening the smoothing ancestor hurts |
| everything above, off together | 3.218 | **worse than the full shading** |

The debug views ruled out three more in one look
(`research/captures/al_debug_views_attribution.png`): **`lodcube` is uniform** across the frame, so
the rings are not LOD-cube boundaries; **`level` shows a handful of large smooth bands**, nothing at
the moiré's scale; **`smooth` is populated almost everywhere** with only scattered magenta, so the
804,157 solid leaves are *not* leaving holes — **(f) is refuted**, and that is worth stating because
the prompt treats it as *"the largest hole in the current filtering"*; and **`lit` is near-uniform**,
so the lighting term carries almost no structure at this pose.

### What it is

`--view material` was the one debug view that carried the speckle. So the CPU reference gained
`--flat-albedo` — shade every hit with one colour regardless of its material — because **no existing
flag could test this**: every other toggle removes a term applied *after* the material is chosen.

| configuration | moiré ratio | carrier RMS |
|---|---|---|
| base | **3.127** | 0.02244 |
| **`--flat-albedo`** | **1.962** | 0.01382 |
| `--flat-albedo` + every other term off | 1.567 | 0.01008 |
| *the target capture, for scale* | *1.144* | *0.01951* |

**Discrete sampling of the per-hit MATERIAL is the dominant cause, at 59% of the excess over the
target** ((3.127−1.144) − (1.962−1.144)) / (3.127−1.144), and it takes the carrier RMS down 38% on
its own. `research/captures/al_moire_material_attribution.png` is the viewed pair: the left half's
green hillside is covered in red per-pixel flicker and its mountains are a tan/green salt-and-pepper;
the right half has none of it.

**This is Laine & Karras' artefact under its published name** — *"blockiness caused by discrete
sampling of shading attributes"* — and a material ID *is* a shading attribute. The research names it
in §0(b) and the prompt quotes it, but both then organise the work around filtering **normals**,
because that is what Crassin's open question is about. At this pose, on this build, the normals are
already fine: `--view smooth` measures **1.351** and the blended `--view normal` **1.279**, both
close to the target's 1.144. **The thing that is not filtered at all is the material.**

### One masking effect worth recording

With the material sampled per hit, turning off every other term made the ratio *worse* (3.127 →
3.218). With a flat albedo, turning the same terms off makes it *better* (1.962 → 1.567). The
material aliasing is large enough to swamp the others, so measuring them individually against the
shipping configuration reads zero for all of them — which is exactly what happened, and is why the
attribution needed `--flat-albedo` to exist before any of it could be believed.

---

## 3. Pre-filtered shading: an AVERAGE, not a representative (goals 278, 279)

Goal 278 says *"store a distribution, not a mean"* and organises itself around normals, because
that is what Crassin's open question is about. **§2 measured that the normals are already fine at
this pose** (`smooth` 1.351, blended `normal` 1.279, against a target of 1.144) and that the
material is what is unfiltered. So this section filters the material — and the design went through
two rejected versions first, both rejected by a viewed capture rather than by argument.

### Attempt 1 — the smoothing ancestor's REPRESENTATIVE material. Rejected.

Zero storage: every node header already carries a representative material, and the attribute walk
already has the header in hand. Blend the albedo toward the ancestor's as the cube approaches pixel
size, using the same `faceWeight` the normal has used since Group Z.

**Metric: 3.127 → 1.795. Image: worse.** The fine red speckle became *large salmon blotches*. A
representative is a **majority vote**, so it is a coarser quantiser, not a filter — and no ancestor
span fixes that: 2.407 / 1.948 / 1.795 / 2.035 at `--smooth-pixels` 2 / 4 / 6 / 12, non-monotonic,
with the coarser settings trading small blotches for big ones with hard edges.

**This is the case the prompt warns about — the metric improved and the picture got worse — and it
is recorded rather than shipped.** It is also the argument for what follows: only a genuine average
is a filter.

### Attempt 2 — a real average, read at the normal's smoothing ancestor. Rejected.

`Brick::mean_albedo()` plus an accumulator up the tree, packed into the header's free bits, read at
the ~6 px ancestor the normal uses.

**It failed a test rather than an eye**: `--verify-frame`'s local contrast fell to **5.7% against a
6% floor** and `svo_render_smoke` went red. The 6 px ancestor is the right scale for a *normal* —
a staircase has to be averaged over several steps before it stops shading as a staircase — and the
wrong scale for an *albedo*, which needs only the pixel's own footprint. **Two quantities, two
scales; conflating them blurred the terrain flat.**

### Attempt 3 — and the bug in it that a capture caught

Read the average at the **hit node**, not at the smoothing ancestor. Tests passed, moiré fell
3.127 → 1.952 — and every green hillside turned **olive**.

`Brick::mean_albedo()` averaged every *occupied* voxel, which includes the dirt and stone **buried
under a grass cap**, and no viewer ever sees those. The correct weight is **exposed face count** —
the same quantity `exposed_face_sum()` already accumulates as a vector, counted here as a scalar.

### What shipped

- **`Brick::exposed_albedo_sum()`** — Σ(albedo × exposed faces) and its denominator. Faces on the
  brick's outer boundary are excluded, for the same reason `exposed_face_sum` excludes them.
- **`NodeSummary` gains `albedo_sum` and `albedo_weight`**, in world units² so coarse and fine
  children mix by real surface area, exactly as `normal_sum` already does.
- **Solid leaves get one too**, weighted by one face at their own level — which is what lets this
  reach the **804,157 solid leaves** the prompt calls the largest hole in the filtering. They have
  no attribute word; they do have a header.
- **Storage: the header's previously unused bits, R4 G6 B4 into bits 24–27 / 10–15 / 28–31.**
  Green gets the extra two bits because luminance is mostly green. **Zero bytes.**

**Why the header and not a second attribute word.** A second word costs 4 B on every internal node
and brick leaf — about **5.0 MB** on the shipping tree. These 14 bits cost nothing, they sit beside
the representative material they band-limit, and they are the only storage a **solid leaf** has.
The prompt's own ranking asked for the second word to be measured rather than assumed; it was, and
it lost to a free option.

**Why a flag bit and not a cbuffer field.** Prompt 004 lost hours to a cbuffer field-*order*
mismatch that a size `static_assert` cannot catch. A spare bit in a word that already exists cannot
reorder. `kFlagFilterAlbedo = 64u`.

### Measured

| pose | moiré OFF → ON | local contrast OFF → ON |
|---|---|---|
| `stress_pose` | **3.814 → 1.800** (−53%) | 17.98 → 9.44 |
| `macro_ground` | **2.367 → 1.665** (−30%) | 28.47 → 23.57 |
| `valley_far` | **4.333 → 1.756** (−59%) | 22.25 → 9.89 |

**The two backends agree to 0.03%** (vk 1.79963, d3d12 1.80010) — which for a shader change that
unpacks bit fields is the check that matters, since FXC and Vulkan's compiler have disagreed here
before.

**Cost: free, in both senses.** GPU march median, `stress_pose`: vk **3.89 on / 3.94 off**, d3d12
**5.32 on / 5.34 off** — the "on" case is nominally faster on both, i.e. the difference is noise.
Memory: **zero**. In `release-codegen-and-tradeoffs.md` §1's classification this is the **free**
bucket: no compile cost, no runtime cost, no memory cost.

Captures, both viewed: `research/captures/al_filtered_albedo_closeup.png` (the close-up pair) and
`research/captures/al_filtered_albedo_pair.png` (`stress_pose` and `valley_far`, before and after).

### And the cost that is not free, stated plainly

**Local contrast drops 47–56% at the two distant poses.** The filter removes texture — that is what
a filter does — and it cannot tell the wanted half from the unwanted half. `valley_far`'s "before"
is the clearest evidence: its mountains carry a dense tan hatching that genuinely *resembles the
target capture*, and the filter removes it along with the speckle.

**That is the AL-A / AL-B trade, and it is why the prompt sequences them this way**: *"putting a
deliberate stipple on top of an aliasing surface is painting over a crack."* AL-A removes both
halves by design; AL-B has to earn the wanted half back deliberately, at a chosen frequency. If it
does not, this change is a net loss on look and should be reconsidered — and the honest place to
judge that is goal 288's side-by-side, not this section.

### Goal 279, answered by 277's negative

Goal 279 says *"if 277 confirms (a) [the albedo mottle], the mottle needs the same treatment as the
normal."* **277 did not confirm (a): `--no-mottle` measured 3.134 against a base of 3.127.** The
mottle is world-locked 2D value noise at 1/24 m and 1/7 m, and at this pose those features are far
larger than a pixel, so it is not near Nyquist and does not alias. It needs no distance fade, and
adding one would be a fix for a problem that was measured not to exist. **Recorded as a completed
negative rather than an unimplemented task.**

---

## 4. Reconstruction, and the measurement that qualifies §3 (goals 280, 281, 282, 283)

### The number that changes how §3 should be read

Every number in §3 is measured with **TAA off**, because that is what the golden-comparing scenarios
use. TAA off is not what ships. With TAA **on** — the shipping configuration:

| pose | TAA on, filter OFF | TAA on, filter ON |
|---|---|---|
| `stress_pose` | 1.090 | **1.024** |
| `taa_pan` @ rest | 1.667 | 1.697 |
| `taa_pan` @ panning | 1.114 | 1.265 |
| `taa_pan` @ just_stopped | 0.968 | 1.087 |
| `taa_pan` @ settled | 1.022 | 1.014 |

**§3's 53% becomes 6% at `stress_pose`, and on the pan captures the filtered version measures
slightly WORSE.** TAA was already averaging away most of the pixel-scale material flicker, so the
two are solving overlapping halves of the same problem. Reporting the 53% without this would have
been true and misleading.

**What survives the qualification**, and it is worth keeping for three reasons rather than one:

1. **The residual speckle is visible and it goes away.**
   `research/captures/al_taa_masks_the_filter.png` is the shipping-configuration pair: the red
   dotted texture on the green hillside is present with the filter off and largely gone with it on.
   Smaller than the TAA-off difference — and real.
2. **It is free.** Zero memory, zero measurable GPU time (§3).
3. **TAA's help is conditional and the filter's is not.** A temporal average only helps once the
   history has converged; it is rejected at silhouettes and under fast motion, which is exactly when
   this world moves. The pre-filter works on the first frame.

**And the cost stands**: local contrast 17.99 → 8.83 at `stress_pose`. For a pass whose complaint is
*"not enough fine grain"*, spending half the texture to remove a faint speckle is only a good trade
if AL-B puts texture back at a **chosen** frequency. That is the whole bet of this prompt's
structure, and goal 288 is where it is settled, not here.

### 280 — the history length, decided

Prompt 004 goal 270 settled the cost half: an exponential history is two buffers and a blend weight,
so **32 frames costs exactly what 8 costs (0.37 ms resolve, identical memory)**. It deferred the
quality decision here. Measured now, `stress_pose` at rest with the filter on:

| | moiré ratio |
|---|---|
| `--taa-blend 0.125` (8 frames) | **1.009** |
| `--taa-blend 0.03125` (32 frames) | 1.042 |

**Indistinguishable at rest (3%), and goal 270 measured 32 frames as 19% softer one second after
motion.** **Decision: keep the 8-frame history**, and the reason is now a budget rather than a
preference — AL-A has already spent 47–56% of the local contrast, and a longer history spends more
of the same currency. NAADF's 32 frames are recorded as available for nothing if a later pass wants
them; this pass cannot afford the softness.

### 281 — the shadow lift, closed by 277's negative

Goal 281 is conditional: *"if 277 confirms (c)"*. It did not. `--no-shadows` measured **3.179**
against a base of **3.127** — removing shadows entirely makes the metric *worse*, so the
LOD-quantised lift cannot be a significant contributor at these poses. **No change made, and the
conditional is recorded as unmet rather than silently skipped.** Goal 164's ring fix, which this
would have extended, is holding.

### 282 — the AO dither, closed on stills and open on motion

`--no-ao` measured **3.140** against **3.127**: on a still frame the screen-space AO hash contributes
nothing to the metric. The *crawl* question a still cannot answer is left open — and note that
Prompt 004 goal 268 already halved the AO rays and clamped their length, so the pattern under test is
not the one the prompt describes. **Recorded as partially answered**: no measurable contribution on
a still, crawl not judged.

### 283 — the total, so far

| pose | at the start of this prompt (TAA off) | now (TAA off) | now (TAA on, shipping) |
|---|---|---|---|
| `stress_pose` | 3.816 | **1.800** | **1.024** |
| `macro_ground` | 2.368 | **1.665** | 0.530* |
| `valley_far` | 4.333 | **1.756** | 1.061* |

\* the TAA-on figures for these two were taken with the filter OFF; the filter-on pair was measured
only at `stress_pose`, and the difference there was 6%.

**For scale: the owner's target capture measures 1.144.** `stress_pose` now measures **1.024** with
TAA on — *below* the target, which is the arithmetic form of the cost above: the frame is now
**smoother than the reference**, not just less aliased than before. The aliasing half of the
complaint is answered; the grain half is not, and AL-B owes it.

---

## 5. The grain as a deliberate style (goals 284–289)

### 284 — the target, measured

A radial FFT of a 128×128 window inside the largest solid-stone region of
`lin_water_checkerboard_after.png` (the window is 100% stone by the green-excess mask):

| quantity | measured |
|---|---|
| **dominant radial period** | **10.67 px** (0.0938 cycles/px) |
| **amplitude** | **RMS 6.37/255** = 0.0250, against a tile mean of 0.463 → **5.4% modulation** |
| **anisotropy** | **641× max/min** across 15° orientation bins, energy concentrated at **30–45°** |

**The prompt asks me to confirm or refute that the stipple is directional. CONFIRMED, and not
marginally: 641×.** It is a hatch, not a dither. And the stipple is on **stone only** — the target's
green is broad and faceted with no stipple at all, and its sand is smooth, which is why the
amplitude became a per-material component rather than a global constant.

**The multiplier, stated in writing as the prompt demands.** The owner asked for *"a much finer
grain which are still that style but smaller of the pixels of the pixels"*. The prompt reads that as
**3×**. I adopt that reading: 10.67 / 3 = **3.56 px**.

**Sanity-check against the eye, with the arithmetic.** Assuming the engine's 70° vertical FOV over
720 rows, one pixel subtends 0.0972°, so the target's 10.67 px period is **0.96 cycles/degree** and
3× finer is **2.9 c/deg**. The eye's limit is 30 c/deg at 20/20 and ~60 c/deg optically
(`research/human-eye-and-vision-research.md`), so **the ask is comfortably achievable optically —
by a factor of ten.** It is not the eye that binds here. See 285.

### 285 — what shipped, and what binds

- **World-locked, per Bénard, Bousseau & Thollot (I3D 2009).** Their result is that the three
  properties a stipple must have — constant density *in the image*, following the 3D surface, and
  temporal continuity — are mutually contradictory, and their resolution is a weighted sum of
  **four** octaves tied to a *zoom cycle*, with weights summing to 1 so no octave pops in.
  Implemented with their weights exactly: `a1 = s/2`, `a2 = 1/2 − s/6`, `a3 = 1/3 − s/6`,
  `a4 = 1/6 − s/6`, with `s` the fractional part of `log2(distance)`.
- **Directional**, along a fixed world-space vector, because 284 measured 641× anisotropy and
  because a world direction is what makes a hatch coherent across a whole slope.
- **Per material**, as a `Stipple` component on `MaterialDef` — every def answers or it does not
  compile. Stone **0.076** (the measured 5.4% RMS is 7.6% peak for a sinusoid), dirt 0.030, and
  **zero for grass, sand, water, wood and leaves**, read off the target capture.
- **Band-limited** by the same fade the grain uses, so the pattern retires toward its mean before it
  reaches pixel frequency and cannot become the artefact §3 just removed.
- **Packed into spare slots**: the amplitude rides in the fractional part of the material record's
  `w` (its integer part is the shading model, and `MaterialShading` now reads it with `floor`, not
  `uint(w + 0.5)`, which would have rounded a large amplitude into the next model); the two knobs
  live in `g_WaveParams.zw`, which were unused. **No cbuffer layout change** — Prompt 004's
  field-order trap cannot recur through a field that does not move.

**The bug worth recording**: the first version passed a *distance-dependent* world scale into a
construction that *already* compensates for distance, so it compensated twice and the pattern
landed an octave and a half too coarse to see — moiré 1.028 against 1.024 without it, i.e. no
measurable effect. Bénard's construction **is** the distance compensation; the scale it takes must
be a constant.

### The Check I could not perform, and why the instrument was the problem

Goal 285's Check is *"viewed captures at 0.3 / 2 / 10 / 60 m from a stone slope showing the stipple
at a constant apparent frequency across all four — that is the property the design claims and it
must be verified, not assumed."*

**It is not verified.** I measured the delivered period by differencing two renders (with and
without the stipple, so the terrain's own facets cancel — an FFT of the composite is dominated by
them and reads nothing useful). Against monotonically increasing requests the delivered period read
**25.6 / 32.0 / 18.3 / 32.0 px** — not monotonic, i.e. noise. The reason is the instrument, not the
constant: **a landscape pose spans many distances at once, so "the delivered screen period" is not a
single quantity there**, and a highest-variance tile picker lands on different terrain at different
depths between runs. A calibration fitted to one of those points made the spread worse and was
reverted.

**The right instrument is the one the Check names — a fixed camera at four distances from one
slope — and it does not exist.** Opened as goal 285a. Until then `--stipple-period` is an honest
*relative* knob and a dishonest absolute one, and the shader says so where the constant is defined.

### What is verified

- **It reaches the frame and it reads as a hatch.** `research/captures/al_stipple_mechanism.png`
  (exaggerated, to show the mechanism) and `research/captures/al_stipple_pair.png` (the shipping
  default): diagonal stripes across the stone faces, **grass untouched**, which is the per-material
  component doing its job and is the target's own arrangement.
- **Both backends**: vk 1.040, d3d12 1.019 on the moiré metric; no FXC errors — and this change
  introduced a `floor` where a `round` was, plus new bit-packing, both of which are exactly where
  the two compilers have disagreed before.
- **335/335 tests.**

### The default, and the honest reason it is not 3.56

At `--stipple-period 3.56` — the owner's 3× — the **delivered amplitude collapses to RMS 1.77/255**,
against ~8.8 at coarser settings. That is not the eye (284 shows 2.9 c/deg is ten times inside the
limit); it is **TAA's 3×3 neighbourhood clamp discarding a sub-pixel feature**, which is precisely
the mechanism the TAA survey names and which §2 of the prompt flags as *"actively destroying the
grain this prompt is trying to create."*

**So: the owner's 3× is achievable optically and not achievable through this engine's TAA.**
`stipple_period_px` ships at **7.0**, the nearest setting that survives the clamp, with
`--stipple-period 3.56` available and its consequence recorded. The published fixes — variance-based
clamping, a reactive mask, or applying the grain *after* the resolve — are the way to get 3× back,
and applying it post-resolve is the cheapest of the three. Opened as goal 285b.

### 286 — directionality, kept

284 measured it rather than leaving it to taste (641×), so this is not the open question the prompt
allowed it to be. The direction is a fixed world-space vector; per-material hatch directions are not
implemented and are not needed by anything measured here.

### 289 — not done

`SvoRenderer::Settings` gained three fields this pass and now has more than twenty. The `--look`
preset collapse is **not implemented**; the individual knobs all work and are in `--help`. Recorded
as owed, not as done.

---

## 6. Cost, regression and what this pass did not do (goals 290–294)

### 290 — the cost table

| option | bucket (`release-codegen-and-tradeoffs.md` §1) | GPU | memory |
|---|---|---|---|
| **per-node average albedo** (278) | **free** | vk 3.89 vs 3.94 ms, d3d12 5.32 vs 5.34 — noise | **zero** (unused header bits) |
| **directional stipple** (285) | **free** | four `sin` per shaded pixel, behind a flag | **zero** (spare cbuffer slots + a `MaterialDef` field) |
| a second attribute word (278, **rejected**) | pay-to-save | — | ~5.0 MB — measured, then not taken |

**The shipped default stays inside Prompt 004's budget**: vk march median **3.89 ms** at
`stress_pose` against that pass's gate of 4.8 ms.

### 291 — goldens, and the half that is owed

Goldens re-accepted on both backends for every scenario, because the look changed on purpose.
`moire_ratio` is available as a scenario assertion and appears in every harness report.

**The standing gate is NOT wired**, and it needs a calibration this pass has not done: a threshold
set above the measured noise floor, the way Prompt 002 goal 217 calibrated the image metric, plus
the falsification the goal asks for (it must fail when the filter is deliberately disabled). Owed.

### 293 — the backends agree

| change | vk | d3d12 | difference |
|---|---|---|---|
| goal 278's filtered albedo | 1.79963 | 1.80010 | **0.03%** |
| goal 285's stipple | 1.040 | 1.019 | 2.0% |

**No FXC errors through either change**, which is the check that matters rather than a formality:
goal 285 moved a `round` to a `floor` and added two new bit-field unpackings, and bit manipulation
plus rounding is precisely where these two compilers have diverged in this repo before (X3500).

### 294 — what this pass did not do

**Named, with a Check each, in `docs/goals.md` Group AL:** goal 287 (close-range edge quality with
and without TAA), **goal 288 (the reference reproduced side by side — which the prompt calls "the
deliverable of this prompt")**, goal 289 (the `--look` presets), goal 292 (the three standing
defects re-checked), 291's gate, and the two the stipple opened — 285a (the fixed-slope instrument
that would make `--stipple-period` absolute) and 285b (applying the grain post-resolve so the
owner's 3× survives TAA).

**And the four standing decisions the prompt requires be honoured or reopened with evidence:**

- **Textures / an asset pipeline: still decided against, and this pass added no pressure to reopen.**
  The stipple is procedural and the albedo filter is a per-node average computed at build time;
  neither wants an authored image.
- **SSAO / a G-buffer: goal 41's gate is *"reopen with textures or a real G-buffer need"*, and the
  answer is still no.** This is worth stating rather than skipping, because a filtered-NDF approach
  *would* arguably have been a G-buffer need — and that is not what shipped. The filtering here is
  per-node and lives in the octree; nothing was added that wants a screen-space buffer.
- **Path tracing / GI: still excluded, and the reasoning is now better evidenced than when it was
  made.** `research/micro-voxel-creators-research.md` confirms from Lin's own text that his method
  *is* a real-time path tracer with 5-bounce GI, so "make it look like Lin" cannot mean "copy Lin's
  method" at this budget. The gap is deliberate.
- **ACES, RG16 normals, and fog converging on the sky gradient**: untouched, and nothing here bears
  on them.

**NAADF's 32-frame history**: measured free and deliberately not adopted — see §4's goal 280.

---

## 7. The honest state of this prompt

**Answered.** The aliasing half. The metric exists, is falsifiable and is validated at 9.51×
separation; the cause is attributed to a term nobody had named, with every named candidate measured
at zero; the fix is free in both memory and GPU time; both backends agree to 0.03%; 335/335 tests.

**Half-answered.** The grain. A deliberate, world-locked, per-material, directional hatch exists and
reads as one — but its central claim, *constant apparent frequency with distance*, is **not
verified**, because the instrument that would verify it does not exist. Saying "the design claims it"
is exactly what goal 285's Check forbids.

**Not answered.** Goal 288 — the side-by-side against the reference at the finer grain. That is the
image the owner asked for and it is the one thing this pass most owed him. The measurements to aim
it exist now (10.67 px, 5.4% RMS, 641× anisotropy at 30–45°, stone only); the pose match and the
capture do not.

**And the number that should temper all of it**: with TAA on, at `stress_pose`, the frame now
measures **1.024 against the reference capture's 1.144** — it is *smoother* than the target, not
merely less aliased than before. The pass removed more texture than it put back. Goal 288 is where
that becomes visible, and it is the first thing the next pass should do.

---

## 8. Goal 288 — the reference, side by side

`research/captures/al_reference_side_by_side.png`, committed and viewed. This is the capture the
prompt calls *"the deliverable of this prompt"*, and §7 above was written before it existed — it is
left standing as written, because "not answered" was the honest state at that moment and revising it
away would hide how the pass actually went.

**The pose was chosen twice.** First for matching *composition* — water, shoreline, green slopes and
exposed stone in one frame — which produced a wide aerial view whose terrain was about **four times
smaller on screen** than the reference's. **No grain comparison is meaningful across a 4× scale
difference**, so it was re-shot closer, at a matching apparent scale.

**What matched:**

- **The stone carries a fine, dense, directional diagonal hatch** that reads as the same *kind of
  mark* as the reference's. This is the thing the owner asked for and it is there.
- **The grass is broad and calm, with no stipple at all**, and the stipple is on **stone only** —
  which is the arrangement 284 measured in the reference rather than a choice.
- No individual voxel reads as a cube at this distance in either image.

**What did not match, separated by whose problem it is:**

- **The forms.** Ours is a smooth cone; the reference has rounded organic shapes with real
  large-scale structure. **This is Prompt 006's subject and the prompt is explicit that this
  comparison must not be used to judge it.** Recorded, not fixed.
- **Tonal variation within the stone.** The reference's rock has light and dark patches at a scale
  well above the hatch; ours is more uniform. Not investigated.
- **The reference's green carries a red speckle** — that is its own *aliasing*, the artefact this
  pass removed, and ours deliberately lacks it. Worth stating plainly: **on this axis ours is
  cleaner than the target**, and "match the reference exactly" would mean putting a defect back.

**Verdict.** The grain question this pass exists to answer is answered: the mark is reproduced, on
the right material, at a visible amplitude, and without the aliasing that used to accompany it.
What separates the two images now is **form**, not grain — which is the correct hand-off to
Prompt 006 and is the most useful thing this comparison establishes.

---

## 9. Goal 292 — the three standing defects, re-checked (and two of them are not this renderer's)

`research/captures/al_standing_defects.png`, viewed. The three artefacts the prompt names as
historically breaking when shading or the attribute layout moves, checked against their committed
captures.

**The finding that reframes two of them: `banding_slope.png` and `sliver_closeup.png` are MESH-PATH
captures.** Both carry the mesh renderer's own overlay — *"chunks ready: 294"*, *"visible after
culling: 32 / 95"*, *"chunk GPU memory"* — none of which the svo path has or prints. The sliver
curtains (goals 73/105) and the slope banding were diagnosed on `--renderer mesh`, which this pass
does not touch at all. **They cannot regress from a change to `svo_march.psh.hlsl`**, and comparing
an svo frame against them would be comparing two different renderers and calling the difference a
verdict.

| defect | verdict | basis |
|---|---|---|
| **shadow rings** (goal 164) | **same — no regression** | `--debug-view lit` at the hilltop is **uniformly white over every terrain pixel**: no discs, no terraced darkening, no rings. This is the direct check, on the right renderer, and goal 164's fix is holding through both of this pass's changes. |
| **slope banding** | **not applicable to this renderer**, and better anyway | The historical capture is the mesh path's heavy terracing. Today's svo grazing view of the same kind of slope shows smooth shading with no terracing — but that is a property of the svo path, not evidence about the mesh path. |
| **sliver curtains** (goals 73/105) | **not applicable to this renderer** | Same reasoning. They remain open on the mesh path, and Prompt 004 goal 272a already records what they are now blocked on: installing RenderDoc, nothing else. |

**So the honest answer to *"did any of them get worse"* is: the one that is on this renderer did not,
verified on the right instrument; the other two are not reachable from anything this pass changed,
and saying "unchanged" about them would have been an unearned claim rather than a measurement.**

---

## 10. Goal 287 — close-range edges, and they depend on TAA

`research/captures/al_close_range_taa.png`, viewed: `macro_ground` at **0.3 m**, `--no-taa` against
`--taa`, same pose, same everything else.

**Without TAA the cube edges stairstep visibly.** The slope carries a fine crenellated texture along
every cube boundary, plus a few horizontal streak artefacts. **With TAA it is smooth** — the
stepping is softened to the point where the surface reads as a surface rather than a staircase.

**So the answer to the prompt's question is yes, and it is worth knowing: the close-range edge
quality is carried by TAA alone.** Nothing else in the pipeline antialiases a cube silhouette — the
albedo filter of §3 deliberately does *not* apply here (`faceWeight` is ~1 when cubes are several
pixels across, which is the John Lin close-up the look wants), and the stipple is a shading term
that does not touch edges. **That is a real dependency**: `--no-taa` is not a neutral A/B at close
range, it is a visibly worse image, and any future work that weakens TAA — a reactive mask, a
shorter history, applying the grain post-resolve (goal 285b) — has to keep the edges in mind.

Also visible and by design: a few red dots survive at close range. `faceWeight` is high there, so
the albedo filter correctly stands aside and lets each cube show its own material. The filter is for
sub-pixel cubes; at 0.3 m these are not sub-pixel.

---

## 11. Goal 289 — one knob, and the tooling bug it surfaced

`--look shipping|raw|flat|hatched` (`render/diligent/look_preset.hpp`).

**Only the seven APPEARANCE fields are in a preset**, not the nineteen. `SvoRenderer::Settings` also
holds shadows, AO, TAA, the LOD multipliers and the beam tile, and **a "look" that silently turned
shadows off would be a performance setting wearing a costume** — it would show up as a frame-time
win nobody asked for. A test asserts that property directly: every preset applied to a Settings with
shadows/AO/TAA off and non-default LOD values leaves all of them untouched.

**A plain function, not a policy template parameter.** `templates-and-metaprogramming.md` §3's
policy-based design is the pattern when axes are *types with different behaviour*; here every axis is
a bool or a float with identical behaviour and only its value differs, so a policy parameter buys
nothing and costs a recompile per look. Same reasoning `PlayerTuning` already records for itself:
these are art-direction values a capture session sweeps from the command line, so they are runtime
data.

**Ordering, stated as a contract rather than left emergent**: `--look` applies **at the position it
appears**. Anything after overrides it; `--look` after a flag overwrites that flag. The alternative —
tracking which options were explicitly set so a preset never clobbers them — needs per-option
provenance in the parser, and this repo has exactly one argv-indexing site precisely because that is
where CLI bugs live. Two tests pin both directions.

The four presets: **shipping** (the defaults, and a test asserts a fresh `AppOptions` already *is*
this look, or the name is a lie); **raw** (everything this prompt added, off — the A/B for the whole
pass, and the configuration §2's attribution numbers were measured in); **flat** (filtering on, every
deliberate pattern off — the control that shows how much of the frame's texture is deliberate);
**hatched** (the stipple at the reference capture's own measured 10.67 px period, for look sessions).

### And a tooling bug worth recording

The three new tests **passed when the binary was run by hand and failed under `ctest`**. The names
began with `--`, and `catch_discover_tests` registers a test by passing its name to the binary as an
argument — so **Catch2's own parser read `--look selects an appearance preset` as an unknown
OPTION**, not a test name. Renaming them fixed it. A test name is an argument; do not start one with
a dash. Noted at the test site so the next person does not spend the same twenty minutes.

---

## 12. Goal 291's other half — the aliasing gate, and a metric with a real noise floor

`assert moire_ratio < N` now stands on `stress_pose`, `valley_far` and `macro_ground`, which are the
three poses goal 276 baselined and all three are in `ctest -L scenario`.

### It is calibrated, and the calibration is the interesting part

| pose | shipping (3 runs) | spread | with `--no-filter-albedo` | gate |
|---|---|---|---|---|
| `stress_pose` | 1.8426 / 1.8435 / 1.8432 | **0.05%** | 3.814 | **2.2** |
| `valley_far` | 1.7712 / 1.7710 / 1.7713 | **0.015%** | 4.333 | **2.1** |
| `macro_ground` | 1.6700 / 1.6702 / 1.6700 | **0.01%** | 2.367 | **2.0** |

**Compare that with Prompt 004's frame-time gate, whose noise floor is ~18% and belongs to the
laptop's GPU boost state.** This metric has a 0.01–0.05% floor because it is a **deterministic image
measurement, not a clock reading** — no boost state, no thermal drift, no contention. It is the gate
Prompt 004's goal 275e wishes it had, arrived at from the other direction.

So the thresholds sit at ~1.2× the measured value, which is 24 000× the noise floor rather than the
uncomfortable 1.1× that a millisecond gate is forced into.

### Falsified, by running it

Goal 291's Check says *"fails when the [filter] is deliberately disabled — verify it, don't assume
it."* Verified, on a scenario copy with `--no-filter-albedo` injected:

```
stress_pose    ASSERTION FAILED: moire_ratio < 2.2  (measured 3.792)
valley_far     ASSERTION FAILED: moire_ratio < 2.1  (measured 4.245)
macro_ground   ASSERTION FAILED: moire_ratio < 2.0  (measured 2.361)
```

**All three fail.** And note the specific falsification changed from the one the prompt anticipated:
it names *"the mottle's distance fade"*, and §2 measured the mottle at zero contribution, so
disabling its fade would have proved nothing. The gate is falsified against **the change that
actually moves the metric**, which is the change it exists to guard.

### And the pairing rule, stated in the scenario files themselves

`contrast_percent` and `moire_ratio` ask opposite questions and a change can trade one for the
other — §3's albedo filter did exactly that, halving the contrast while halving the aliasing.
**Both assertions now stand in the same file, so that trade cannot pass unnoticed in either
direction.**
