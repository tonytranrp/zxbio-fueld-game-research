# Prompt 005 — The look: a fine grain, not a field of cubes

**To:** the main coding session (`[CC]`), branch `C++-voxel`
**From:** the side session, 2026-09-06
**Read this whole file before touching anything.**

**Depends on Prompt 002** (the harness — this is the most capture-heavy prompt in the arc, and
eyeballing it by hand is what made the last two passes slow) and **Prompt 004** (frame-time
headroom: several options here cost GPU time that does not currently exist, and 004's attribute-word
and page-layout decisions constrain what can be stored per node). Do it after 004.

---

## 0. What this pass is and is not

**The gap, in the owner's words**, in two languages and both worth reading literally:

> *"for the current game I'm still seeing like blocks and rigid cubes and such, which is not great.
> I wanted something like this [`research/captures/lin_water_checkerboard_after.png`] where we have
> done it before, but instead of 2D images like this we make it 3D instead."*
>
> *"hãy nhìn vào tấm hình checker và làm y chang ... chứ không phải là bây giờ nó nhìn nó giống như
> cục, cục à. Hình vuông nó nhìn nhìn nó không có, nó không phải là micro voxel game cho lắm."*
> — "look at the checker picture and do exactly like that ... not like now, where it looks like
> lumps, lumps. The squares — it doesn't look like a micro-voxel game."
>
> *"the checker board is like so retro and stuff, but instead of making it similar to that we
> changed it to a much finer grain which are still that style but smaller of the pixels of the
> pixels — smaller checker board pixels kinda thing."*

**Both reference captures were viewed for this prompt, and here is what they actually show.**

`lin_water_checkerboard_after.png` (the target): stone slopes carry a **dense, fine, directional
stipple** — it reads like pencil hatching or a halftone screen — laid over **smooth, soft, large-
scale forms**. The green is broad and faceted-but-calm. Nothing reads as an individual cube. The
grain is *texture*, and it is at a frequency well below the form.

`svo_ground_hilltop.png` (the current svo path, 76 fps): the same stipple is present, but it has
become **moiré** — concentric ring patterns and orange/green speckle sweeping across whole
hillsides, strongest where the surface is most oblique. And `lin_final_vk.png` shows the second
half of the complaint: the terrain forms are isotropic noise cones, so there is no "smooth large
form" for a fine grain to sit on.

So the ask decomposes into three separate, individually verifiable problems:

1. **The grain has turned into aliasing.** The frequency content of the voxel surface lands at and
   below one pixel, one ray per pixel samples it, and eight frames of TAA is not enough
   reconstruction. That is a *filtering* problem with a large published literature.
2. **The grain is not fine enough where it should be, and too coarse where it shouldn't.**
   `lod_radius = 4.0 m` means full 7.8 mm resolution reaches four metres; beyond that the voxel edge
   grows linearly with distance, holding roughly one voxel per pixel — which is *by design* the
   maximum-aliasing operating point.
3. **The forms underneath are wrong** — but that is Prompt 006 (terrain), not this prompt. Say so
   and do not try to fix it here.

**What this pass turns it into.** A surface whose appearance is *derived*: a pre-filtered shading
response per node so a sub-pixel voxel field integrates correctly instead of aliasing; a stipple
that is a deliberate, world-locked, temporally-stable pattern at a chosen angular frequency rather
than an accident of undersampling; and a grain scale set by a stated criterion, with the reference
capture reproduced at a finer grain and shown side by side.

**Named outcome.** From the same pose as `svo_ground_hilltop.png`, a capture with **no ring moiré
anywhere in the frame**, a fine even stipple across the stone at roughly three times the target
capture's spatial frequency, and smooth-reading forms — and a numeric moiré metric that says so, not
just an opinion.

**What this pass is NOT.** Not a terrain pass (006 owns the forms). Not a GPU-architecture pass (004
owns the throughput; this prompt spends from its budget and must say how much). Not a materials/
texture-authoring pass — this world has no textures and adding an asset pipeline is not in scope
(`docs/progress.md` records that decision). Not path tracing or global illumination: this project's
visual arc is explicitly *"cheap, real techniques ... chosen to evoke the feeling of that aesthetic"*
rather than a reproduction of John Lin's actual 5-bounce path tracer, and that decision stands
unless you bring evidence.

---

## 1. Context to read FIRST, in this order

1. **`CLAUDE.md`** — build non-negotiables, and two that make this prompt *fast*:
   **shaders load at runtime**, so a shading experiment is an edit-and-relaunch (the water
   checkerboard was bisected in four runs by swapping one `return` line and sampling a pixel row —
   use that technique here, it is the cheapest tool in the repo); and **`--debug-view NAME` renders
   one shading term per frame** (`lit|ao|normal|facenormal|level|steps|coverage|cubepx|smooth|lodcube|
   material|distance`, and `tools/svo_render --view` takes the same names). *"Reach for it before
   staring at a composite — each view attributed one bug in this pass."* Also: FXC's X3500 (no
   runtime-indexed vector component writes) means test **both backends**.
2. **`docs/progress.md`** — including *"Decisions that survived contact with evidence."* Four
   entries are directly in this prompt's path and must be honoured or explicitly reopened with
   evidence: **ACES was tried and rejected by a viewed capture** (mid-tones washed ~30 %; the
   shipped tonemap is a soft-knee whose only job is rolling off bloom overshoot); **SSAO/G-buffer
   decided against** (goal 41's gate — *"reopen with textures or a real G-buffer need"*);
   **RG16 normals decided against** (no visible banding, viewed); **fog must converge on the sky
   gradient along each view direction, not a flat colour**. Also read the entry about the
   **12-byte vertex's context-dependent 4th byte** and its instruction: *"widen to 16 B rather than
   pack a fourth meaning."*
3. **`docs/goals.md`** — Group Z (shading correctness & the Lin look) **in full**, including goals
   164 (the shadow rings), 165 (`--debug-view`), 167 (the grain), 168 (TAA), and Group AB. Read
   their Checks: they are the standard this prompt's Checks must match or exceed.
4. **`docs/render-pipeline.md`** — the pipeline as documented.
5. **`research/lin-look-log.md`** — **mandatory, and it is the prior art for this exact problem.**
   The previous pass already fought the moiré and won part of it. Read: the per-node normal +
   coverage attribute design and why; the smooth-normal blend; goal 164's shadow-ring analysis
   (*"a slope of tangent s built from steps of any size puts s/tan(sun elevation) of every tread in
   its own riser's shadow (47% at 45 degrees under this sun), scale-free, so it shows at every LOD as
   terraced darkening and, at pixel-sized steps, as moiré"* — that is Gustafsson's "extreme shadow
   acne"); the grain recipe attributed to Binks (*fade the pattern toward its mean as it approaches
   pixel frequency*); and the water-checkerboard bisection, which is the methodology to reuse.
   **Do not re-derive what this log already established. Extend it.**
6. **`research/gpu-voxel-streaming-and-profiling-research.md`** — **§1.5 and §1.7 specifically, and
   read them carefully, because they contain the single most important admission in the
   literature for this prompt.** Crassin's own SIGGRAPH 2009 slides, on the pre-filtered cone
   sampling that GigaVoxels' anti-aliasing rests on: *"But there is a little problem… Approximate
   cone integration ○ Using pre-integrated data — **But the integration function is not the good
   one!** — Emi/Abs model used along rays ○ But pre-integration is a simple sum — Result: Occluding
   objects are merged/blended"*; and on lighting: *"**Lighting problem — How to pre-filter lighting?
   — Pre-filter Normals ○ How to store them? ○ How to interpolate them?**"* Their considered fix
   was anisotropic pre-integration, *rejected on "Storage / Sampling cost!" grounds.*
   **This engine's layout-v2 attribute word (int8×3 average normal + uint8 coverage) is exactly the
   structure Crassin was asking how to store, and the question of how to *interpolate* it is still
   open in the literature.** Also read **§3.5** (NAADF 2026 uses a **32-frame** TAA history rather
   than one, for this content class and these named artefacts) and **§1.3** (why you must not add a
   brick apron: 1.95× memory on 8³ bricks).
7. **`research/voxel-aesthetics-and-view-distance-research.md`** — **mandatory. This is the
   specification for Groups AL-A and AL-B.** Start with **§0**, which names both of this engine's
   artefacts in the literature and, in the process, tells you they are one problem:
   - `[C]` Ruijters, *Common Artifacts in Volume Rendering* (arXiv 2109.13704), on the rings —
     they are called **"onion rings"** and the cause is *correlated* sampling error between
     neighbouring pixels, not too few samples: *"The stripes in the 'onion ring' pattern arise from
     the fact that the rays in neighboring pixels are sampled at similar distances, yielding
     comparable errors (in magnitude and sign). When the sample locations of the rays in adjacent
     pixels is varied (e.g., by using variable sample distances, or a random offset at the beginning
     of each ray), the stripe pattern is broken. **This leads to a more 'dithered' image, as the
     errors are still present but now randomly distributed over the pixels.**"*
     **The textbook remedy for the ring artefact is to convert it into a dither — which is a step
     toward the look the owner is asking for. The two complaints are the same problem.**
   - `[C]` Laine & Karras on the close-range lumps — they are called **"blockiness caused by
     discrete sampling of shading attributes"**, and §1.6 has their published fix.
   - `[C]` A Luanti contributor measuring this exact symptom on a voxel lattice: *"FXA does not help
     with that, and SSAA is way to too expensive"* … *"This really needs a geometry interpolation."*
     **MSAA/FXAA are reported ineffective against voxel-lattice moiré by someone who measured it.**

   Then read, in this order:
   - **§1** — anti-aliasing and pre-filtering for ray-marched voxels: voxel cone tracing's
     pre-filtered representation, and the pre-filtered-normal-distribution line of work (LEAN,
     LEADR, Han et al.'s frequency-domain normal-map filtering, Kaplanyan et al.'s HPG 2016
     filtering of normal distributions for shading antialiasing, and the glinty-surface work). **I
     need the math from this section: what covariance/roughness term is stored, and how it maps into
     the BRDF.** That is the published answer to Crassin's open question.
   - **§2** — stochastic/dithered/blue-noise appearance *as a deliberate style*: ordered dithering,
     blue-noise masks, Bayer matrices, interleaved-gradient noise, and — the question that decides
     this prompt's approach — **what makes a stipple stable and pleasing under camera motion rather
     than crawling: screen-space-locked, world-space-locked, or temporally-animated, and what TAA
     does to each.**
   - **§3** — John Lin's and other micro-voxel developers' own statements about how they filter and
     shade surfaces to avoid a cubic look, with the CONFIRMED/INFERENCE labels honoured. Note that
     the earlier research (`research/micro-voxel-creators-research.md`) already established that
     **Lin's actual technique is a real-time path tracer with 5-bounce GI**, confirmed from his own
     text — so "make it look like Lin" cannot mean "copy Lin's method" at this budget.
   - **§4** — whether one ray per pixel is even the right operation when voxels are sub-pixel, and
     the recommended reconstruction filter and history length.
   - **§6** — fixed foveation and VRS, if Prompt 004 left it open. Short version: **expect
     1.1–1.8× on a desktop, not Guenter's 5–6×** — Tursun measured **1.32× for a ray tracer at
     1440p** — and **no shipped flat-screen game does crosshair-centred fixed foveation**; every
     verified flat-screen VRS implementation is content-driven.
   - **§9, the addendum, in full.** It contains four corrections to §5 and one to §7, and two of
     them change numbers this prompt uses:
     **§9.1** — 20/20 = 1′ MAR = 30 c/deg = **60 ppd**, while Campbell & Green's 60 c/deg = **120
     ppd**. These are different quantities and conflating them moves a resolution budget by 2×.
     Ashraf et al. 2025 (Nature Comms) measured the foveal achromatic limit at **94 ppd**, and —
     usefully for a grain — only **53 ppd** for yellow-violet patterns.
     **§9.3** — Williams & Coletta (JOSA A 1987): observers resolve detail *"as much as about **1.5
     times higher** than the nominal Nyquist frequency of the underlying cone mosaic"*, and the
     Nyquist frequency *"is **not** a theoretical upper bound for psychophysical measures of visual
     resolution."* **This is the physiological reason a band-limit set to the display's Nyquist rate
     can still look wrong, and the reason "convert the aliasing into a well-distributed dither"
     beats "filter until it is provably below Nyquist." Internalise it before writing 279.**
     **§9.4** — the centre pixel subtends **10.3 % more angle** than the frame average. Use the
     centre-pixel spread angle (`α = arctan(2·tan(ψ/2)/H)`, giving 7.8 mm sub-pixel beyond
     **12.97 m** at 1080p/60°), not deg/px (14.30 m) — the frame-average value under-selects the
     octree level exactly at screen centre. And: **at 1080p the DISPLAY is binding by ~1.9× (14.3 m
     vs 26.8 m); the eye only becomes binding past 4K, where the target is LOOSER.** So the band
     limit should target the pixel footprint at 1080p and switch to targeting acuity at 4K+ —
     higher-resolution users should get *more* visible grain from the same code, not less.
8. **`research/human-eye-and-vision-research.md`** — **Part 1 §3.2 and §8.3** are the physical
   justification for the whole approach, and the sentence to internalise is §8.3's:
   *"beyond ~60 cyc/deg no detail survives the eye's optics anyway, so aliasing energy there is
   **purely** artifact — an antialiased, band-limited distant object is not 'missing detail', it is
   **more faithful** than a sharp one."* Also **Part 5 §5.6** on what TAA is and is not
   perceptually (*"Nothing in human vision corresponds to this"* — the retina integrates
   continuously; TAA's ghosting is a temporal alias), and **§5.7(c)** on veiling-luminance-aware
   bloom.
9. **`research/micro-voxel-creators-research.md`** and **`research/baked-ao-design.md`** — the
   earlier passes' own research on the look and on AO.
10. **The C++ skill reference files** (`~/.claude/skills/cpp-heavy-templates/references/`) —
    **mandatory:**
    - `templates-and-metaprogramming.md` §1 (concepts), **§3 policy-based design** — the shading
      model is a set of independent axes (normal reconstruction, grain source, AO source,
      reconstruction filter) and this is the pattern for composing them without a boolean explosion.
      The current `SvoRenderer::Settings` is already 15 booleans and floats; do not make it 25.
    - `memory-and-performance.md` — anything you add to the attribute word costs memory ×
      node count (246,046 internal + 804,157 solid at the default pose). Read before widening it.
    - `release-codegen-and-tradeoffs.md` §1 — classify every option into a bucket and say which.
    - `modular-architecture.md` §1–§2 — the CPU/GPU mirror boundary. `ray_trace.cpp` and
      `svo_march.psh.hlsl` are a hand-maintained mirror; anything that makes that mirror harder to
      keep in lockstep is a cost, and should be counted as one.

---

## 2. The systems you are building on — verified file map

Checked against the files on 2026-09-06.

- **`render/diligent/shaders/svo_march.psh.hlsl`** (698 lines), read in full. The shading model,
  precisely:
  - **The anti-moiré normal blend**: `cubePixels = hit.cubeEdge / max(hit.t * g_ShadeParams.w, 1e-6)`
    (`g_ShadeParams.w` is the raw pixel angle in radians), then
    `faceWeight = saturate((cubePixels - 1.5) / 3.0)`, then
    `normal = normalize(lerp(smoothNormal, faceNormal, faceWeight))`.
    The header comment states the intent exactly: *"the blend between the cube's own face (large on
    screen: the John Lin close-up look, cubes visibly cubes) and the tree's averaged surface normal
    (cubes near pixel size: the staircase must not shade as a staircase, or it moires)."*
    **So the existing mechanism blends the NORMAL only. Albedo, coverage, AO, shadow and grain are
    not filtered at all — which is where the residual moiré lives.**
  - `haveSmooth = !hit.solidLeaf && dot(hit.smoothNormal, hit.smoothNormal) > 0.01` — **a solid leaf
    carries no attribute word at all** (`tree_layout.hpp`: *"a solid cube's normal is whichever face
    is hit"*), so at the default pose's **804,157 solid leaves** the smooth normal is simply
    unavailable and the shading falls back to the face. That is a large fraction of the world with
    no filtering available, and it is a structural fact, not a tuning knob.
  - `ReadAttributes` walks *up* the traversal stack from the deepest attribute-carrying entry while
    `g_TreeOrigin.w * exp2(-float(L)) < h.t * smoothPixelAngle` — i.e. the smoothing ancestor is
    chosen by *angular span*, with `smoothAngle = g_ShadeParams.x * g_ShadeParams.w` and
    `smooth_pixels = 6.0` by default.
  - **The grain**: `cell = floor((p - faceNormal * (0.5 * hit.cubeEdge)) / hit.cubeEdge)`,
    `amplitude = g_ShadeParams.y * saturate((cubePixels - 1.5) / 2.5)`,
    `grain = 1 + amplitude * (Hash3(cell)*2 - 1)`, `grain_amplitude = 0.10`. The comment records
    the rule and the reason: *"(Binks' recipe: fade the pattern toward its mean as it approaches
    pixel frequency)"*, *"Gone by 1.5 px (a per-cube hash at pixel frequency is structured noise
    against the pixel grid -- its own moire), full from 4 px up."*
    **This grain is WORLD-locked (hashed on integer cube coordinates), which is the right choice —
    but it is keyed to the LOD cube size, so the pattern's world frequency changes with distance.**
    That is a candidate cause of the ring moiré and is worth testing directly.
  - **The albedo mottle**: `n1 = ValueNoise(p.xz / 24)`, `n2 = ValueNoise(p.xz / 7 + 17.31)`,
    `mottle = 0.90 + 0.20 * (0.65*n1 + 0.35*n2)` — **two-octave 2D value noise in world XZ, with no
    distance fade at all.** A world-space XZ pattern viewed at a grazing angle is a textbook moiré
    generator, and it is unfiltered. **Suspect this first.**
  - **Shadow and AO origins**: half a finest voxel off the hit face plus a distance-scaled epsilon,
    then lifted along the smooth normal by `liftEdge = hit.solidLeaf ? 0.0 : hit.cubeEdge` (full cube
    for shadows, half for AO). The comment carries goal 164's whole analysis. **Note the
    interaction: the lift is a function of `cubeEdge`, so shadow contact changes with LOD level — a
    per-level discontinuity, which is exactly what a ring artefact looks like.**
  - **AO**: four fixed directions at ~45° off the normal, rotated per pixel by `Hash2(pixel)` — a
    **screen-space** hash, *"so the pattern dithers instead of banding"*. Ray length is a
    screen-space radius (`ao_radius_px = 32`) with a 0.15 m floor. **Screen-space dither + TAA is
    the classic combination; check whether it is crawling under motion.**
  - **Fog**: `heightFactor = exp2(-max(p.y,0)*0.012)`, `density = 0.0030*(0.80+0.20*heightFactor)`,
    `rawFog = 1 - exp2(-(dist*density)^2 * 1.442695)`, `fogAmount = saturate(rawFog*1.12)`,
    converging on `SkyGradient(dir)`. **Prompt 007 owns the fog's physical basis — do not change
    it here**, but note that fog *masks* distance moiré, so a fog change and a moiré measurement
    must not be conflated. Measure with fog held fixed.
  - **12 `--debug-view` terms**, listed in §1.1. `kViewCubePixels` shows `cubePixels / 8` and
    `kViewSmoothNormal` shows the averaged normal alone with **magenta where none was recorded** —
    that magenta map is a direct picture of the 804,157-solid-leaf problem. **Capture it first.**
- **`world/svo/include/world/svo/tree_layout.hpp`** — the attribute word: bits 0–7 / 8–15 / 16–23 =
  normal x/y/z as int8 snorm (decode `/127`), bits 24–31 = coverage as uint8 (decode `/255`).
  `make_node_attributes(normal, coverage)`, `node_attr_normal`, `node_attr_coverage`.
  **Internal nodes and brick leaves carry one attribute word; solid leaves carry none.**
  Layout: `internal: [header][attributes][children...]`, `brick: [header][brick index][attributes]`,
  `solid: [header]`. **Widening this word, or giving solid leaves one, is the central structural
  question of Group AL-A** — and it interacts with Prompt 004's page layout, so read 004's log
  before deciding.
- **`world/svo/detail/tree_builder_impl.hpp`** (403 lines) — where the area-weighted average normal
  and volume coverage are computed during the build. This is where a pre-filtered *distribution*
  (rather than a mean) would be computed.
- **`world/svo/src/ray_trace.cpp`** (369 lines) — the CPU reference, whose `make_hit` attribute rule
  the shader's `ReadAttributes` mirrors statement for statement. **The 7,000-ray oracle gates any
  change to it.** Note: shading changes that do **not** alter traversal do not need the oracle — but
  the attribute *read* rule is shared, so a change there does.
- **`render/diligent/shaders/svo_taa.psh.hlsl`** (97 lines) — distance-reprojected TAA: the march
  jitters rays by a sub-pixel Halton offset and writes hit distance; the resolve reprojects the
  previous frame through camera motion (the world is static), rejects on distance mismatch, clamps
  to the current 3×3 neighbourhood, and blends at `taa_blend = 0.125` — **an eight-frame history.**
  By the TAA survey's own arithmetic α = 0.125 gives ~8 effective samples at steady state, where
  α = 0.1 gives 19. **TAA is off under a debug view**, which is what makes `--debug-view` an honest
  look at the raw signal.
  > **⚠ The neighbourhood clamp is actively destroying the grain this prompt is trying to create.**
  > `[C]` Yang, Liu & Salvi, *A Survey of Temporal Antialiasing Techniques* (CGF 39, 2020) §6.1.2:
  > *"History rectification techniques are based on the assumption that the current frame samples in
  > the neighborhood of each pixel contain the entire gamut of surface colors covered by that pixel.
  > Since the current frame samples are sparse (≤1 sample per pixel)… **Unfortunately, with highly
  > detailed content, this assumption is often violated… causing the underestimated color bounding
  > box to clip or clamp away the line color from history. This happens commonly in highly detailed
  > scenes, where small, sharp features are smoothed out in the output.**"*
  > A per-pixel stipple is *by construction* the "sub-pixel thin feature missing from the input of
  > certain frames" case. Three options, from the same survey: variance-based clamping, a
  > reactive/exclusion mask for grain-bearing pixels, or **apply the grain after the resolve.**
  > This is a real constraint on Group AL-B, not a footnote — resolve it in 285 and say which you
  > chose. Research §3.5 also notes NAADF 2026 uses a **32-frame** history for exactly this content
  > class; Prompt 004 goal 270 may already have measured that — read its log before re-measuring.
- **`tools/svo_render`** (699 lines) — the CPU reference renderer: `--view` takes the same debug
  names, `--lod-center x,y,z` builds the LOD around a point other than the camera (*"the
  deterministic reproduction of 'the camera moved away from the last build center'"*), and it prints
  a per-level sampled/kept brick histogram and the camera column probe. **PNGs are written stored,
  not compressed** (2,765,798 bytes at 1280×720) — re-save through PIL before committing one.
- **The reference captures**, both viewed for this prompt:
  `research/captures/lin_water_checkerboard_after.png` (the target look — 1080×480, the fine
  directional stipple on stone over smooth green forms), `lin_water_checkerboard_before.png` (the
  sun-glint lattice that was fixed), `svo_ground_hilltop.png` (the current svo path with the ring
  moiré and the 76 fps overlay), `lin_final_vk.png` / `lin_final_d3d12.png` (the pass's final
  frames), `lin_view_smooth.png` and `lin_view_facenormal.png` (the two debug views most relevant
  here), `blocky_default.png`, `banding_slope.png`, `sliver_closeup.png`.
- **Current numbers**: GPU march+resolve **3.2–6.3 ms**; **76 fps** ground level; `--verify-frame`
  local contrast **34.7 % / 34.6 %** against a **6 %** threshold; 246,046 internal + 804,157 solid
  nodes + 657,034 bricks at the default pose. **177 tests.**

---

## 3. Standing rules for this pass

Prompt 002 §3 in full. The ones that decide this prompt:

1. **A viewed capture is the evidence, on both backends, always.** This is a look pass; a number
   that says the look improved without an image is worthless. Conversely — and this is the harder
   discipline — **an image that looks better without a metric is also not enough**, because this
   repo has twice shipped a "verified" frame that was wrong (the ribbon bug's silhouette slivers;
   the first Gerstner spectrum). Both, every time.
2. **Use `--debug-view` before staring at a composite.** Each of the 12 terms isolates one cause.
   The pass log's own words: *"each view attributed one bug in this pass."*
3. **Bisect in the shader, not in your head.** Shaders load at runtime. The water checkerboard was
   solved in four runs by swapping one `return` line and sampling one pixel row with a five-line
   Python script. That is the tool for "which term is causing the rings."
4. **CPU reference first for anything touching the attribute read rule**, and the **7,000-ray
   oracle at 0/7,000**. `tools/svo_render --lod-center` is the deterministic reproduction for any
   artefact that appears "after a rebuild".
5. **Performance measured**, against the budget Prompt 004 established, with before/after GPU ms per
   option. State which of `release-codegen-and-tradeoffs.md` §1's four buckets each option is in.
6. **Tests green; new systems get new tests.** 177 is the floor.
7. **Determinism**: a world-locked pattern must be a pure function of world position and level, not
   of frame index or camera. A *temporally* animated pattern (if §2 of the research recommends one)
   is the one exception and must be explicitly flagged as such, because it breaks the "same seed,
   same image" property that `--verify-frame` and the goldens rely on.
8. **Materials are components.** Any per-material appearance parameter (stipple frequency, grain
   amplitude, roughness) goes in `MaterialDef` and is exported as a macro — never a literal in the
   shader.
9. **No new dependencies without a written case.**
10. **Read the goals.md group notes** (Z, AB) before closing or reopening anything there, and honour
    or explicitly reopen the four decided-against entries in §1.2.
11. **Delegate web research to one or two read-only subagents.** Likely candidates if
    `research/voxel-aesthetics-and-view-distance-research.md` leaves a gap: the exact form of the
    LEAN/LEADR/Kaplanyan filtered-NDF term and how it is packed in shipped engines; and
    blue-noise mask generation and its world-space application. Specific question, keep coding,
    persist to `research/` and cite.
12. **Commit and push per group.**
13. **`git add` explicit paths, never `git add -A`.**

---

## 4. Task groups

### Group AL-A — Find the moiré, then filter it properly (goals 276–283)

> ## Before you read the tasks: one finding already verified against this repo
>
> The side session checked `world/svo/detail/tree_builder_impl.hpp` directly on 2026-09-06, and
> **this engine already computes the standard pre-filtering roughness signal and then throws it
> away in one line.** The builder accumulates exactly the right quantity — its own comment:
> *"its area-weighted exposed-face normal sum (world units squared, so coarse and fine children mix
> by real surface area)"* — and then:
>
> ```cpp
> [[nodiscard]] static std::uint32_t pack_summary(const NodeSummary& s) noexcept {
>     const float length = glm::length(s.normal_sum);
>     const glm::vec3 n = length > 1.0e-12f ? s.normal_sum / length : glm::vec3{0.0f};
>     return make_node_attributes(n, s.coverage);
> }
> ```
>
> `length` is computed, used only to normalize, and dropped. Per the research §1.3-A, the length of
> the average normal *is* the variance: `[C]` Crassin et al. 2011, *"the variance is encoded via the
> norm |D| such that **σ² = (1−|D|)/|D|**"*, following Toksvig. **Normalizing before quantizing is
> precisely the operation that destroys the anti-aliasing signal.**
>
> **One subtlety the research does not state and you must handle.** Toksvig's `|N_a|` is the length
> of the average of *unit* normals, in [0,1]. `normal_sum` here is in absolute area units, so its
> raw length is **not** `|N_a|` — you need `|N_a| = |Σ nᵢAᵢ| / Σ Aᵢ`, i.e. the vector sum's length
> divided by the **total exposed face area**. That denominator is **not currently accumulated**:
> `childFaceArea` and `brick.exposed_face_sum()` are available at the seeding sites, so it is
> derivable, but `NodeSummary` needs a third field (`float area_sum`) and one more `+=`.
> **Storage**: the attribute word is full (3×8 snorm normal + 8 coverage). Either add a second word
> (~3.6 MB at the default pose's 246,046 internal + 657,034 brick-leaf nodes — measure it) or split
> the coverage byte 4/4. My read: 4/4 is likely sufficient, since coverage only feeds a threshold
> (`kSecondaryCoverage = 0.35`) and 16 roughness buckets is more than a `σ²` remap needs. **Measure
> both.**
>
> ### The ordered plan, from research §7 as amended by §9.9 — by value ÷ effort
>
> This is the sequence the literature supports. Task 277's attribution may reorder it; if it does,
> say so and why. **But do 1 and 2 first regardless: they are nearly free and they are the two
> things the literature is unanimous about.**
>
> 1. **Stop normalizing the average normal** (above). Free roughness from data already written.
> 2. **Add screen-space NDF filtering** — Tokuyoshi & Kaplanyan's Listing 2, **four lines of HLSL**,
>    `KAPPA = 0.18`, `SIGMA2 = 0.25` (2019 slope-space form) or `0.15915494 = 1/(2π)` (2021
>    projected-space form, which fixes the grazing-halfvector blow-up: RMSE 12631.7 → 0.134).
>    Measured at **0.08–0.11 ms over no anti-aliasing at all, at 8K**. Use the axis-aligned/isotropic
>    variant — the authors state the non-axis-aligned one *"can still flicker in animation"*.
> 3. **Fix the LOD rule to use the cone footprint including the grazing term**:
>    `λ = log2(α·t / |n̂·d̂| / voxel_size)` with the centre-pixel α. **The grazing term is what kills
>    the rings**, because rings live at grazing angles where an isotropic radius under-selects. And
>    GigaVoxels is explicit that you must blend **three** levels: *"For proper blending three levels
>    are a must."* The engine currently blends none — it picks one and shades it.
> 4. **Jitter the ray start** by a random fraction of the step, with **screen-locked** spatiotemporal
>    blue noise (Wolfe et al. 64³ STBN masks, *not* migrated with motion — research §2.2 quotes their
>    reasoning verbatim). This is Ruijters' documented cure and it converts rings into the dither
>    that is wanted.
> 5. **Add Laine & Karras' variable-radius post-process filter** for the near field — the published
>    fix for the exact word *blockiness*: 96 samples in a 24-px disc, radius from the projected voxel
>    size stored as 3.5 fixed-point in the alpha channel, with the `r ← min(r, r′)` clamp that
>    *"prevents visible seams from forming"* at hierarchy level changes. Measured *"one to two
>    magnitudes faster than the ray casting."* **The marcher already writes hit distance to a second
>    target — you have most of the machinery.**
> 6. **Then** the grain as its own system (Group AL-B).
> 7. **Audit TAA for grain destruction** — see the warning in §2's `svo_taa` entry.
> 8. **The int8×3 normals are a documented limiter.** Laine & Karras: 8-bit object-space normals are
>    *"insufficent for smooth highlights and reflections"*; their 14-bit scheme is published. If
>    lumpiness survives 1–7, this is next.
>
> **Two more results worth knowing before you start.** Heitz & Neyret (HPG 2012) wrote the paper for
> exactly this data structure — per-voxel Gaussian descriptors at *"an average 15-20 bytes per
> voxel"*, with the crucial implementation note *"we store and interpolate **variances σ², which is
> the quantity that interpolates linearly**"* and the headline property *"the timings per pixel are
> scale-independent"* (0.1–0.3 µs/pixel while zooming). And Zirr & Kaplanyan's biscale NDF is the
> closest published result to the target aesthetic: **"procedural terrain with grainy snow on an
> overcast day (7.8ms/frame)"**, temporally stable, sub-pixel, with the two scales collapsing
> analytically to **α²g = α²m + α²l** as the grains disappear at distance — *"the resulting density
> Dg defines the global appearance of the material in the limiting case, when individual microdetails
> disappear at distance."* That is exactly "grain up close, defined material at distance."

**276. Build a moiré metric before changing anything.**
"It looks noisy" is not falsifiable. Build a number: for a fixed pose, measure the energy in the
frame at frequencies near the pixel Nyquist that is *not* explained by the underlying form.
Candidate approaches to rank: a high-pass filter's energy in a masked terrain region; the variance
of a local window compared to the same pose rendered at 4× supersampling and downsampled (the 4×
render **is** the ground truth here, and `tools/svo_render` can produce it on the CPU without
touching the GPU path); or a radial spectrum showing the ring frequency explicitly.
The existing local-contrast metric (`--verify-frame`, 34.7 %) **cannot** do this job — it measures
*presence of texture*, which is what you want to keep. You need a metric that separates *wanted
texture* from *unwanted aliasing*, and the supersampled reference is what makes that possible.
**Check**: the metric is implemented, its value is reported for `svo_ground_hilltop`'s pose,
`macro_ground`, and `valley_far`, and — critically — **it is validated against a known-good and a
known-bad input**: `lin_water_checkerboard_after.png`'s pose should score low, the current
`svo_ground_hilltop` pose high. If it does not separate those two, the metric is wrong; fix it
before proceeding. State both numbers and the separation ratio.

**277. Attribute the moiré to a term, by bisection.**
Candidates, each individually testable by returning early from the shader (the water-checkerboard
technique), in the order I would try them:
 (a) **the albedo mottle** — two octaves of world-XZ value noise with **no distance fade**, viewed
     at grazing angles. This is the strongest suspect and the cheapest to test: return
     `mottle.xxx` and look.
 (b) **the per-cube grain** — keyed to `cubeEdge`, so its world frequency changes with LOD level;
     the `saturate((cubePixels-1.5)/2.5)` fade is per-pixel, so at an LOD boundary two adjacent
     pixels can have very different amplitudes.
 (c) **the shadow lift** — `liftEdge = hit.cubeEdge` makes contact shadowing a function of LOD
     level, and goal 164's own analysis says the staircase self-shadowing artefact is *"scale-free,
     so it shows at every LOD as terraced darkening and, at pixel-sized steps, as moire."*
 (d) **the AO's screen-space rotation hash** plus TAA.
 (e) **the LOD early-out itself** — `--debug-view lodcube` and `level` show where the level
     boundaries are; if the rings coincide with them, the cause is the level transition, not any
     shading term.
 (f) **the smooth-normal availability cliff** — `--debug-view smooth` paints magenta where no
     averaged normal was recorded, i.e. the 804,157 solid leaves.
**Check**: one capture per candidate with that term isolated, all viewed, and a written verdict
naming the dominant cause with its moiré-metric contribution. **Capture `--debug-view lodcube`,
`level` and `smooth` first** — if the rings coincide with level boundaries or with the magenta
regions, that answers the question in one look and saves the rest of the bisection.

**278. Pre-filtered shading: store a distribution, not a mean.**
This is the structural fix, and it is the published answer to Crassin's own open question (research
§1.5: *"How to pre-filter lighting? Pre-filter Normals ○ How to store them? ○ How to interpolate
them?"*). The aesthetics research §1 has the mathematics — LEAN/LEADR-style filtered normal
distributions, Kaplanyan et al.'s HPG 2016 filtering of normal distributions for shading
antialiasing, Han et al.'s frequency-domain treatment. The shape of the answer: a node stores not
just the average normal but a measure of how *spread* the normals underneath it are (a covariance /
roughness term), and the shading uses that spread to widen the BRDF lobe instead of shading a mean
normal sharply.
**The design decision is where the extra term lives**, and it interacts with Prompt 004: the
attribute word is one `uint32` per internal node and per brick leaf, and 004 may have repacked it.
Options to rank: a second attribute word (costs 4 bytes × 1.05 M nodes ≈ 4 MB at the default pose —
measure it, do not guess); repurposing bits from the existing word (coverage is 8 bits, the normal
3×8); or deriving the spread from `coverage` alone, which is already stored and is a crude proxy for
it. **Rank with the memory and GPU cost of each, measured.**
**Also decide the solid-leaf question**: 804,157 nodes with no attributes at all is the largest hole
in the current filtering. Giving solid leaves an attribute word costs one word each (~3.2 MB) — but
a solid leaf is genuinely a flat face, so the *normal* is not what is missing; the spread is zero by
definition. Work out whether the hole is real or apparent, and say which.
**Check**: the ranking with measured memory and GPU cost; the chosen term implemented in
`tree_builder_impl.hpp` (build side), `ray_trace.cpp` (CPU read), and `svo_march.psh.hlsl` (GPU
mirror) **in that order**, with the oracle at 0/7,000 between the second and third. The moiré metric
before and after on all three poses. Viewed captures, both backends. **A unit test asserts the
computed distribution against a hand-built node** (e.g. a node containing a known staircase should
report a known spread) — test the instrument before trusting its reading.

**279. Filter the albedo, not just the normal.**
If 277 confirms (a), the mottle needs the same treatment as the normal: fade toward its mean as its
world-space feature size approaches the pixel footprint, exactly as the grain already does. The
machinery exists (`cubePixels`, the `saturate((x-1.5)/2.5)` idiom); the mottle simply does not use
it. **Express the fade in the world-space feature size of the noise octave against the pixel
footprint at that distance**, not in `cubePixels` — the mottle's frequency is 1/24 m and 1/7 m in
world XZ, which has nothing to do with the hit cube's size.
**Check**: the moiré metric before and after; a viewed grazing-angle capture; and the mottle still
visible at close range (assert the metric that measures *wanted* texture — `--verify-frame`'s local
contrast — has not collapsed: report it before and after, it must stay well above 6 %).

**280. Reconsider the reconstruction: is one ray per pixel enough?**
Research §4 asks this directly, and the eye research answers half of it: *"beyond ~60 cyc/deg no
detail survives the eye's optics anyway, so aliasing energy there is purely artifact — an
antialiased, band-limited distant object is not 'missing detail', it is more faithful than a sharp
one."* Options: keep 1 spp and lean on filtering (278/279); 1 spp with a longer TAA history
(research §3.5's 32 frames — Prompt 004 goal 270 may already have measured this, read its log);
2 spp in a stratified pattern at the cost of ~2× march; or a stochastic sample with a proper
reconstruction filter rather than a box.
**Check**: GPU ms and the moiré metric for each option tried, on all three poses, both backends,
plus a **viewed slow-pan sequence** for each — because the failure mode of a longer history is
ghosting, which a still cannot show. State what you kept and why, with both numbers.

**281. The shadow-lift discontinuity.**
If 277 confirms (c): `liftEdge = hit.cubeEdge` makes the shadow contact point jump at every LOD
boundary. Goal 164 fixed the *ring* version of this (secondary rays judging LOD from the eye's
origin); this is the remaining scale-dependence. Consider a lift that is continuous in distance
rather than quantised by level.
**Check**: `--debug-view lit` at a hilltop, before and after, viewed, compared against
`lin_shadow_rings_after_cpu.png`. The moiré metric. **The oracle at 0/7,000** — the lift changes the
secondary ray origin, which is traversal input.

**282. The AO dither, and whether it crawls.**
`Hash2(PSIn.Pos.xy)` is screen-space, so under camera motion the AO pattern is re-randomised every
frame in world terms — TAA then averages it, which is the intent, but it is also exactly the
condition that produces crawling if the history is rejected (and the history *is* rejected on
distance mismatch, i.e. at silhouettes). Research §2 has the answer for what makes a dither pattern
stable under motion; apply it.
**Check**: a viewed slow-pan sequence with `--debug-view ao`, before and after, judged for crawl.
The moiré metric on a still. GPU ms unchanged (this should be free — a different hash, not more
rays).

**283. Report the total, honestly.**
The moiré metric on all three poses, before and after the whole group, with the GPU ms cost and the
memory cost. And a **side-by-side of `svo_ground_hilltop`'s pose, before and after**, committed.
**Check**: the numbers and the image pair, both in the log. If the metric improved but the image
does not look better to you, **say so** — that means the metric is measuring the wrong thing, and
that is a finding worth more than a passing number.

---

### Group AL-B — The grain, as a deliberate style (goals 284–289)

Now that the aliasing is gone, put the *wanted* texture back, on purpose, at a chosen frequency.
This is the half of the ask that is about taste — so it must be driven by the reference capture and
by captures of your own, not by a formula alone.

**284. Characterise the target capture numerically.**
`lin_water_checkerboard_after.png` is the brief. Measure it: the stipple's dominant spatial
frequency in cycles per pixel (a radial FFT of a masked stone region will give it), its contrast
amplitude, and whether it is isotropic or directional (it reads directional to my eye — confirm or
refute). Then convert the frequency into **cycles per degree** using the capture's own FOV and
resolution, so the target is expressed in a resolution-independent unit.
Then apply the owner's actual instruction — *"a much finer grain which are still that style but
smaller"* — as a stated multiplier on that frequency. **Three times finer** is my reading of "much
finer ... smaller of the pixels of the pixels"; state your reading and get it wrong in writing
rather than silently.
**Check**: the measured frequency (cycles/pixel and cycles/degree), amplitude and anisotropy of the
target capture, the chosen multiplier, and the resulting target frequency — all in the log. Then
sanity-check the target against the eye research's ~60 cyc/deg optical limit: **if the target
frequency exceeds it, the grain is physically invisible and the ask is unachievable as stated** —
say so with the arithmetic and propose the nearest achievable frequency.

**285. A world-locked stipple at a chosen angular frequency.**
The existing grain is world-locked (good) but keyed to `cubeEdge`, so its world frequency changes
with LOD. Replace that with a stipple whose **world-space** frequency is fixed per material, faded
toward its mean as its *own* frequency approaches the pixel Nyquist — the same rule as 279, applied
to a pattern chosen for its appearance rather than inherited from the voxel grid.
**Research §2.3 contains a solved recipe for this and you should use it rather than inventing one.**
`[C]` Bénard, Bousseau & Thollot (I3D 2009) state the problem formally and prove the three
constraints are mutually contradictory: the marks must have *"constant size and density in the
image"*, must *"follow the motion of the 3D objects"* (or you get the **shower door effect**), and
must have *"temporal continuity… to avoid popping and flickering."* **That is why a stipple crawls no
matter how you place it — unless you use their fractal-octave construction.** Their answer, verbatim:
*"we define a dynamic solid texture as the weighted sum of n octaves… we introduce the notion of
**zoom cycle** that occurs every time the appearant size of the texture doubles… Empirically we
observed that **n = 4 octaves is enough to deceive human perception**."* With
`(u,v,w)_i = 2^(i−1)(x,y,z) / 2^floor(log2 z_cam)`, `s = log2 z_cam − floor(log2 z_cam)`, and weights
that must sum to 1 across the cycle:
```
a1(s) = s/2 ;  a2(s) = 1/2 − s/6 ;  a3(s) = 1/3 − s/6 ;  a4(s) = 1/6 − s/6
```
Known cost, stated by them: *"the fractalization process introduces new frequencies in the texture,
along with a loss of contrast."* It is procedural in 3D, which is the natural fit for a voxel field —
no texture parameterization needed.

**And the second half: Praun et al.'s nesting property (SIGGRAPH 2001), which is the answer to "why
does my stipple pop when the octree level changes."** `[C]` The failure mode: *"The lack of coherence
between the strokes at the different levels create the impression of **'swimming strokes' when
approaching or receding from the surface**."* The fix: *"we impose a **stroke nesting property**: all
strokes in a texture image (ℓ,t) appear in the same place in all the darker images of the same
resolution and in all the finer images of the same tone."* **If the grain is derived from voxel data
at a level, level N+1's grain must be a superset of level N's, in the same places.** Praun states
outright that *"stippling"* is one of the intended aesthetics of the technique.

Research §2 decides the *pattern*: ordered dither / Bayer / blue-noise mask / interleaved gradient.
**Blue noise is my expectation for the winner** (its energy sits where the eye is least sensitive and
it is the standard choice in shipped engines) but §2.2's finding about *stability under motion* is
what should actually decide it — and note §2.1's table: this is **appearance** grain, so it is
**world-locked**, unlike the sampling jitter in AL-A step 4 which is **screen-locked**. Getting those
two backwards is the classic error. Read it, choose, say why.
Per-material via `MaterialDef` (stone wants a hatch, grass does not). And per research §9.2, the
grain can be **more aggressive in yellow-violet (53 ppd) than in luminance (94 ppd)** — a free win
if you want amplitude without visibility cost.
**Check**: viewed captures at 0.3 m / 2 m / 10 m / 60 m from a stone slope, both backends, showing
the stipple at a *constant apparent frequency* across all four (that is the property the design
claims and it must be verified, not assumed). The measured frequency of your render against 284's
target. A viewed slow-pan for crawl. GPU ms cost.

**286. Directionality, if the target has it.**
If 284 finds the target's stipple directional, decide what drives the direction: the surface normal,
the world axes, or a per-material hatch direction. A pencil-hatch look needs a *coherent* direction
over a region, which world axes give for free on axis-aligned voxel faces.
**Check**: viewed captures with and without directionality on the same pose, and a written judgement
against the target capture. If directionality does not visibly help, drop it and say so.

**287. Close-range: cubes should still be cubes.**
The existing `faceWeight` blend deliberately keeps *"the John Lin close-up look, cubes visibly
cubes"* above 4.5 px, and that is correct — at 30 cm, 7.8 mm voxels are ~2.6 cm on screen at 1080p
and *should* read as cubes. What must be right is the **edge**: a cube edge at 4 px needs proper
antialiasing or it stairsteps. Check whether TAA is carrying that alone.
**Check**: `macro_ground` and `macro_tree` captures at 0.3 m and 2 m, both backends, with and
without TAA, viewed, and a written judgement on edge quality. If the edges are only acceptable with
TAA, say so — that is a dependency worth knowing.

**288. Reproduce the reference, at the finer grain, side by side.**
Set up a pose and a sun angle matching `lin_water_checkerboard_after.png` as closely as the current
terrain allows (the *forms* will differ — that is Prompt 006's job, and this comparison must not be
used to judge them). Render it. Commit the pair.
**Check**: the pair is committed and viewed, and the log states plainly which properties matched
(stipple frequency, amplitude, smoothness of form, absence of visible cubes) and which did not, with
the reason. **This is the capture the owner asked for; it is the deliverable of this prompt.**

**289. One knob, not fifteen.**
`SvoRenderer::Settings` is already 15 fields, and this prompt adds more. Collapse the appearance
axes into a small number of named presets (a "look" policy per
`templates-and-metaprogramming.md` §3) with the individual knobs still reachable for capture
sessions — the same shape `PlayerTuning` already uses ("a plain struct passed by const&, not a
policy template parameter: these are art-direction values a capture session wants to sweep from the
command line").
**Check**: `--look NAME` selects a preset; the individual flags still work and still override;
`--look` values are enumerated in `--help`; a test asserts each preset's field values so a
refactor cannot silently change the shipped look.

---

### Group AL-C — Cost, regression and handoff (goals 290–294)

**290. The cost table.**
Every option in this prompt: bucket (per `release-codegen-and-tradeoffs.md` §1), GPU ms, CPU ms,
memory, and the running total against Prompt 004's budget. Both backends.
**Check**: the table in the log and in the harness report. The shipped default is still above the
fps target 004 established, measured on `stress_pose`, `macro_ground` and `valley_far`. If it is
not, options come off the *default* (not out of the codebase) until it is, and the log says which
and why.

**291. Goldens and the moiré metric as a standing gate.**
Promote the new goldens for every look scenario, and wire the moiré metric as a harness assertion
with a threshold calibrated the way Prompt 002 goal 217 calibrated the image metric — **measure the
noise floor, set the threshold above it, record both numbers.**
**Check**: the gate passes, and fails when the mottle's distance fade is deliberately disabled
(a one-line shader change — verify it, don't assume it).

**292. The three standing defects, re-checked.**
This prompt changes shading and possibly the attribute layout. Re-check the three artefacts that
historically break when either moves: **the shadow rings** (goal 164), **the sliver curtains** at
grazing angles (the open defect in `research/water-foliage-design.md`; Prompt 004 goal 272 may have
a RenderDoc capture of it by now — read 004's log), and **the banding on slopes**
(`banding_slope.png`).
**Check**: one viewed capture per defect compared against its historical capture, with a verdict:
better, same, or worse. **If any got worse, say so and either fix it or open a goal — do not let a
grain win hide a shading regression.** If the sliver curtain is *fixed* by this work, that closes a
defect open for three passes and deserves saying loudly.

**293. Both backends must agree.**
Everything in this prompt is shader work, so FXC's X3500 and any HLSL-version divergence are live
risks. Compare vk and d3d12 for every look scenario using Prompt 002's cross-backend consistency
check, with its documented loose threshold.
**Check**: every look scenario within the cross-backend threshold. Any divergence is investigated
and named, not tolerated — a backend divergence in shading is the exact bug class this repo already
hit once with X3500.

**294. What this pass did not do.**
Record with a reason and a follow-up goal each: textures and an asset pipeline (still decided
against, and now say what would reopen it); SSAO/G-buffer (goal 41's gate — restate whether the
pre-filtered-distribution work changed the answer, because *"reopen with textures or a real
G-buffer need"* is the gate and a filtered NDF is arguably a G-buffer need); path tracing / GI (the
project's stated arc excludes it — restate the reasoning with the Lin research's confirmation that
his own method *is* a path tracer, so the gap is deliberate); NAADF's 32-frame history if you did
not adopt it; anything from the aesthetics research you did not use.
**Check**: the list is in `docs/goals.md` with Checks and in the log with reasons.

---

## 5. Sequencing and risk

- **276 first, absolutely.** Without a moiré metric this whole prompt is opinion, and opinion is how
  the ribbon bug survived two "verified" runs. The metric must be validated against a known-good and
  known-bad input (that is part of 276's Check) or it will lie to you.
- **277 second**, and it may be very cheap: capture `--debug-view lodcube`, `level` and `smooth`
  and the rings may attribute themselves in one look. Do that before writing any filtering code.
- **278 is the structural task** and the one with real risk: it touches the build, the CPU reference
  and the shader, and the oracle gates the middle step. It also interacts with Prompt 004's page
  layout — **read 004's log before choosing where the extra term lives**, and if 004 already
  repacked the attribute word, work within that.
- **279, 281, 282 are independent** of 278 and each other, and each is small. Do them in whatever
  order 277's attribution suggests.
- **AL-B depends on AL-A** — putting a deliberate stipple on top of an aliasing surface is painting
  over a crack.
- **284 before 285**: measure the target before trying to hit it.
- **AL-C last.**
- **Commit and push per group.**
- **Named legitimate negative results**: 278's second attribute word may cost more than it buys (a
  measured negative closes the goal); 280's longer history may ghost unacceptably; 286's
  directionality may not help; and **284 may prove the ask is physically unachievable as literally
  stated** (a stipple finer than ~60 cyc/deg is invisible) — in which case the honest answer with
  the arithmetic and the nearest achievable target is the right deliverable, not a silent
  substitution.
- **The one thing that would make this prompt fail quietly**: shipping a look that scores well and
  looks worse. Guard against it by keeping 283's and 288's viewed side-by-sides as the final
  authority, and by saying so in the log if the metric and your eye disagree.

---

## 6. Explicitly out of scope this pass

- **The terrain forms.** `lin_final_vk.png`'s noise cones are Prompt 006's problem, and half of
  "it doesn't look like a micro-voxel game" is the forms, not the grain. Do not tune the noise here.
- **The GPU architecture, throughput, streaming, brick compression.** Prompt 004. This prompt spends
  from its budget and reports the spend.
- **View distance, region size, fog's physical basis, LOD-radius derivation.** Prompt 007. **Hold the
  fog fixed while measuring moiré** — fog masks distance aliasing and would flatter a change that
  did nothing.
- **Grass and moving trees.** Prompt 007 (goals 190–195).
- **Player physics, collision.** Prompt 003.
- **Textures, asset pipeline, SSAO/G-buffer, path tracing, GI.** All recorded in 294.
- **Editing, multiplayer, audio.** Not this arc.

---

## 7. When you are done

1. Write **`research/fine-grain-look-log.md`**: the moiré metric's design and its validation numbers;
   the 277 attribution with one capture per candidate; the 278 ranking with measured memory and GPU
   cost and the solid-leaf analysis; every before/after moiré-metric and GPU-ms pair; the target
   capture's measured frequency/amplitude/anisotropy and the chosen multiplier; the pattern choice
   with the stability reasoning; the 292 verdicts on all three standing defects; the cost table with
   buckets; and every decided-against and every honest negative — including any place where the
   metric and your eye disagreed.
2. Add **Group AL** to `docs/goals.md`, goals 276–294 numbered exactly as above, each `[x]` with its
   Check recorded as performed with numbers and named captures.
3. Refresh `docs/progress.md`: the current-state paragraph describes the filtered shading and the
   deliberate grain; add this pass's entries to *"Decisions that survived contact with evidence"*,
   including whatever 294 records and whether goal 41's SSAO gate moved.
4. Add **`docs/the-look.md`**: what each appearance term is, what filters it, at what frequency, and
   which research section specifies it — plus the `--look` presets and what each is for. Short, and
   written for the person who has to change the look in six months.
5. Update `CLAUDE.md` with operational deltas only: `--look`, any new `--debug-view` terms, the moiré
   metric's invocation, and the new baseline numbers.
6. Update `Prompts/README.md`'s index row for 005.
7. Full suite green, both backends, the moiré gate passing, and **the 288 side-by-side committed and
   viewed** — that pair is the answer to the question the owner actually asked.

---

*Provenance: written by the side session on 2026-09-06 after viewing
`research/captures/lin_water_checkerboard_after.png`, `lin_final_vk.png` and
`svo_ground_hilltop.png` directly, and after reading `render/diligent/shaders/svo_march.psh.hlsl`
in full (the `faceWeight` blend, `ReadAttributes`, the per-cube grain, the two-octave albedo
mottle, the shadow/AO origins and lift, the AO's screen-space rotation hash, the fog, and all 12
debug views), `render/diligent/shaders/svo_taa.psh.hlsl`,
`render/diligent/include/render/diligent/svo_renderer.hpp`,
`world/svo/include/world/svo/tree_layout.hpp` in full (the attribute word's exact bit layout, and
that solid leaves carry no attributes — 804,157 of them at the default pose), `docs/goals.md`
Group Z, `docs/progress.md`'s decided-against list, and
`research/gpu-voxel-streaming-and-profiling-research.md` §1.3, §1.5, §1.7 and §3.5 — where
Crassin's own admission that pre-integrated cone sampling uses the wrong integration function, and
his open question about how to store and interpolate pre-filtered normals, are quoted verbatim from
his SIGGRAPH 2009 slides. That admission is the reason Group AL-A is structured as it is.*
