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
