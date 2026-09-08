# The look

Every appearance term in the svo path: what it is, what band-limits it, at what frequency, and which
research section specifies it. Written for whoever has to change the look in six months and does not
want to re-derive why each term is shaped the way it is.

Reasoning and every measurement: [`research/fine-grain-look-log.md`](../research/fine-grain-look-log.md).
Backlog: [`docs/goals.md`](goals.md) Group AL. The renderer underneath:
[`docs/gpu-architecture.md`](gpu-architecture.md).

---

## 1. The one thing to understand first

**Two complaints, one cause.** The owner asked for two things that sound opposite — *"it looks like
lumps, lumps"* and *"a much finer grain ... smaller of the pixels of the pixels"*. They are the same
problem: a voxel field whose detail lands at and below one pixel, sampled once per pixel. Ruijters
(arXiv 2109.13704) names the fix and it is the useful sentence in the whole literature here:

> *"When the sample locations of the rays in adjacent pixels is varied ... the stripe pattern is
> broken. **This leads to a more 'dithered' image, as the errors are still present but now randomly
> distributed over the pixels.**"*

**The textbook remedy for the rings is to convert them into a dither, which is a step toward the
look that was asked for.** So the pipeline below is: *band-limit everything that aliases*, then *put
a chosen pattern back at a chosen frequency*. Doing only the first makes the world flat; doing only
the second paints over a crack.

---

## 2. The terms, in the order the shader applies them

| term | anchored | band-limited by | measured contribution to aliasing |
|---|---|---|---|
| **hit material → albedo** | the octree | **the per-node average albedo** (§3) | **59% of the excess** — the dominant one |
| **average-normal blend** | the octree | `faceWeight`, ancestor span ~6 px | already clean (1.28 vs a 1.14 target) |
| **albedo mottle** | world XZ | nothing — and it needs nothing | **0%, measured** |
| **per-cube grain** | world (cube coords) | fades out below 1.5 px | **0%, measured** |
| **stipple** (§4) | world, 4 octaves | fades out below 2 px | *added on purpose* |
| **sun shadow** | ray | the lift, one cube along the smooth normal | 0% (removing it makes things worse) |
| **AO** | screen-space hash | 2 rays, clamped ≤2 m (Prompt 004 goal 268) | 0% on a still |
| **fog** | view direction | converges on the sky gradient | Prompt 007 owns it |
| **TAA** | screen, reprojected | 8-frame exponential history | removes most of what is left |

**The numbers in the last column are measurements, not estimates**, and they are why this document
does not read like the prompt that commissioned it: the prompt named six suspects and **all six
measured at zero**. The one that mattered was not on the list.

---

## 3. The albedo filter — the structural fix

**A node stores the area-weighted average albedo of its own subtree**, packed R4 G6 B4 into the node
header's previously unused bits (`tree_layout.hpp`). Shading blends toward it as the hit cube
approaches pixel size, using the same `faceWeight` the normal has used since Group Z.

Three things about it are load-bearing and each was learned by getting it wrong:

1. **An average, not a representative.** Every node already carried a *representative* material and
   using that was tried first: it moved the metric from 3.127 to 1.795 and made the picture **worse**
   — fine speckle became large blotches, because a majority vote is a coarser quantiser, not a
   filter.
2. **Filtered at the hit node, not at the normal's smoothing ancestor.** Two quantities, two correct
   scales. A normal needs a wide ancestor because a staircase must be averaged over several steps;
   an albedo needs only the pixel's own footprint. Reading it from the 6 px ancestor blurred the
   terrain flat — local contrast **5.7% against a 6% floor**.
3. **Weighted by exposed faces, not by volume.** A volume average includes the stone buried under a
   grass cap, which no viewer sees, and it turned every green hillside **olive**.

**Solid leaves get one too.** They have no attribute word — they have a header, and that is where
this lives, which is the only reason the filtering reaches the 804,157 of them.

**Cost: free.** Zero memory (unused bits), and GPU march median 3.89 ms with it against 3.94 without.

**And the cost that is not free:** local contrast falls 47–56% at distant poses. The filter cannot
tell wanted texture from unwanted. That is what §4 exists to repay.

---

## 4. The stipple — the deliberate half

**Measured from the reference capture, not chosen**: period **10.67 px**, amplitude **5.4% RMS**,
anisotropy **641×** at 30–45°. It is a *hatch*, and it is on **stone only** — the reference's green
is broad and faceted and its sand is smooth.

- **World-locked, four octaves, Bénard/Bousseau/Thollot (I3D 2009).** They prove that constant image
  density, following the surface, and temporal continuity are mutually contradictory, and that four
  octaves on a zoom cycle resolve it. Their weights, exactly: `s/2`, `1/2−s/6`, `1/3−s/6`, `1/6−s/6`.
- **Per material**, as a `Stipple` component. A new material answers or it does not compile. Stone
  0.076, dirt 0.030, everything else 0.
- **Band-limited** below a 2 px period, so it retires toward its mean rather than becoming the
  artefact §3 removed.

**Two honest limits, both measured:**

- **`--stipple-period` is a relative knob, not an absolute one.** The delivered screen period has not
  been verified, because the instrument for it — a fixed camera at four distances from *one* slope —
  does not exist; a landscape pose spans many distances and reads noise (25.6 / 32.0 / 18.3 / 32.0 px
  for monotonically increasing requests). Goal 285a.
- **The default is 7 px, not the owner's requested 3.56.** At 3.56 the delivered amplitude collapses
  to RMS 1.77/255 against ~8.8 — **TAA's 3×3 neighbourhood clamp discarding a sub-pixel feature**,
  the mechanism the TAA survey names. The ask is achievable optically (2.9 c/deg against a 30 c/deg
  limit) and not through this engine's TAA. Goal 285b is applying the grain after the resolve.

---

## 5. The knobs

| flag | what it does |
|---|---|
| `--filter-albedo` / `--no-filter-albedo` | §3, the structural filter |
| `--stipple` / `--no-stipple` | §4, the deliberate hatch |
| `--stipple-amount F` | multiplies every material's own amplitude (default 2.5) |
| `--stipple-period F` | wanted screen period in px (default 7.0; relative, see §4) |
| `--grain F` / `--no-grain` | the older per-cube brightness hash (Group Z) |
| `--smooth-pixels N` | the normal's smoothing ancestor span |
| `--taa` / `--no-taa`, `--taa-blend F` | 0.125 = 8 frames. **Do not lengthen** — see below |
| `--debug-view NAME` | one term per frame; the first tool to reach for |
| `--mottle` / `--no-mottle`, `--flat-albedo` | `svo_render` only — the bisection handles |

**`--look NAME` does not exist.** Goal 289 asked for the knobs to collapse into presets and it is not
done; `SvoRenderer::Settings` is over twenty fields and growing.

**On lengthening the TAA history:** it is *free* (Prompt 004 goal 270 — an exponential history is two
buffers and a blend weight, so 32 frames costs what 8 costs), and it is still the wrong move here.
8 and 32 measure 1.009 vs 1.042 at rest, and 32 is **19% softer one second after motion**. This pass
has already spent half the local contrast on §3; softness is the currency it is short of.

---

## 6. How to judge a change

1. **`voxel_harness --moire FILE.png`** — the aliasing metric. Validated: the owner's target scores
   **1.144**, the frame he complained about **10.877**, a **9.51×** separation. Below a carrier floor
   it reports `n/a` rather than a number, and a caller must check that.
2. **`--verify-frame`'s local contrast** — the *wanted*-texture metric. These two move in opposite
   directions and a change that improves one usually costs the other. Report both.
3. **A viewed capture, on both backends.** Neither number above has ever been sufficient here: the
   ribbon bug survived two "verified" runs, and in this pass the ancestor-material filter improved
   the metric while visibly making the picture worse.
4. **`--debug-view`** before staring at a composite. In this pass `lodcube`, `level`, `smooth` and
   `lit` eliminated four candidates in one look.
5. **Bisect in `tools/svo_render`, not in the app.** It has no TAA, so it shows the raw signal — and
   TAA masks most of what §3 fixes, which is why every attribution number here is a TAA-off number.
