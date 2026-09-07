# Prompt 007 — As far as the eye can see, and something alive in every metre of it

**To:** the main coding session (`[CC]`), branch `C++-voxel`
**From:** the side session, 2026-09-06
**Read this whole file before touching anything.**

**Depends on Prompt 002** (harness), **Prompt 004** (frame-time headroom and the resident-cache
architecture — view distance is meaningless without it) and **Prompt 006** (the terrain worth
looking at, and the biome field vegetation reads). Do it last in the arc.

---

## 0. What this pass is and is not

**Two gaps, and they are the same gap seen from two ends.**

*How far.* The region is `--region-log2 9` = a **512 m** cube around the camera, and full voxel
resolution reaches only `--lod-radius 4` = **4 metres**. Beyond 4 m the voxel edge grows linearly
with distance, so at 100 m a "voxel" is ~20 cm and at 400 m it is ~78 cm. The owner's ask is
explicit: *"for the chunks of the terrain I wanted a maximum chunks that the human eye could see
all loaded, with optimisations as said before in the documents of how far a human could see."*
That is a question with a real answer, and `research/human-eye-and-vision-research.md` contains it
— Part 1 §8 in particular, which is a worked treatment of exactly "how far can you see, and why do
distant things fade." Right now none of those numbers appear anywhere in the code: the region size,
the LOD radius, the fog density (`0.0030`, hand-picked) and the far plane are all round numbers
somebody chose.

*What is in it.* The world has trees and nothing else. Grass does not exist (Group AG, goals
193–195, **not started**). Trees do not move (goals 190–192, **not started**) even though the wind
field they were designed to consume shipped and is complete (goal 185). At 7.8 mm voxels, an empty
ground plane between trunks is the most obviously wrong thing in every ground-level capture.

**What this pass turns it into.** Every distance constant in the renderer is *derived*, in code,
from a stated perceptual criterion with its research citation next to it — including fog, whose
rate constant Koschmieder's law fixes at 3/V to 3.9/V rather than leaving it to taste. The view
reaches as far as the eye can use, with the LOD ladder sized by the 1-arcmin criterion instead of
by a radius. And the ground is covered: grass near the camera as real voxels, grass at mid range as
an instanced overlay, grass at distance as a wind-modulated shimmer on the Grass material; trees
sway with the field that already exists; and the whole thing is inside a measured frame budget.

**Named outcome.** Standing on a hilltop you can see a valley several kilometres away fading into
haze at the rate real air fades it; the grass at your feet is individual blades, the grass at 20 m
is a moving carpet, and the grass at 300 m is a shimmer that reads as a meadow — and the frame rate
is the one Prompt 004 established.

**What this pass is NOT.** Not eye-tracked foveated rendering (the eye research §5.7(g) defers it
explicitly: desktop eye trackers add the 50–70 ms latency budget with no benefit over fixed
foveation for a crosshair-centred view). Not varifocal anything (§5.5: the hardware does not exist
in consumer form). Not depth of field (§5.5: game DOF reads as *camera*, and without gaze tracking
the focus distance is a guess that fights the player's attention). Not a new terrain pass, not a
new physics pass.

---

## 1. Context to read FIRST, in this order

1. **`CLAUDE.md`** — build non-negotiables; and the shader hot-reload property, which makes the
   fog and shimmer work in this prompt a fast edit-relaunch loop rather than a rebuild loop.
2. **`docs/progress.md`** — including *"Decisions that survived contact with evidence."* Two
   entries are in this prompt's path and both must be handled explicitly rather than contradicted
   quietly: **"grass ground-cover geometry" was decided against** (goal 40 — "needs
   instancing+textures"), and **fog "must converge on the sky gradient along each view direction,
   not a flat colour"** (flat fog made fogged ridges vanish while their darker trees lingered as
   floating dashes). The first is reopened by this prompt with the grass research as the new
   evidence; the second is a constraint you must not break.
3. **`docs/goals.md`** — **Group AF's note and goals 190, 191, 192** and **Group AG's note and
   goals 193, 194, 195**, all in full. These are the specifications for Groups AN-C and AN-D; they
   already carry Checks written by the previous side-session pass, and those Checks stand.
4. **`research/gameplay-pass-log.md` §8 and §8b** — what the unstarted tree and grass work builds
   on, and the one non-obvious design question already answered there: **D3 needs the Grass
   material to be wind-responsive, and Grass is `Shading::Lit`, not `Shading::Foliage`; changing
   its shading model would wrongly give ground grass the mesh path's canopy sway. The
   materials-as-components answer is a new `MaterialDef` member (`wind_responsive`) exported to
   shaders alongside the shading model.** It was deliberately not made last pass because making it
   without a consumer would have been speculative. This pass is the consumer.
5. **`research/human-eye-and-vision-research.md`** — **mandatory, and it is the specification for
   Group AN-A.** Read:
   - **Part 1 §8** in full — §8.1 the angular-size arithmetic with the worked table (1 cm detail
     resolvable to **34 m** at 20/20's 1-arcmin MAR, **~54 m** for the 94-ppd young-observer
     ceiling; an 18 cm face to **619 m** as a blob); §8.2 Koschmieder's law
     $C(d) = C_0 e^{-\sigma d}$ with $V = 3.912/\sigma$ (Koschmieder's $C_t = 0.02$) or
     $V = 3.0/\sigma$ (WMO's $C_t = 0.05$), and the worked transmission table for V = 10/20/50/100
     km; §8.3's three stacking fade mechanisms and — critically — its closing **"LOD/mip
     engineering aside"**, which states the rule this prompt implements: *"(a) fade the contrast of
     distant geometry toward sky/haze colour exponentially in d/V — that's what fog does;
     Koschmieder says the rate constant is 3/V to 3.9/V, not an artist-chosen number; (b) drop
     texture mip/LOD not at fixed distances but where the object's finest texel falls under
     ~1 arcmin (0.64′ for a young-eye ceiling) — for a 1 cm world-space texel that is 34 m;
     (c) beyond ~60 cyc/deg no detail survives the eye's optics anyway, so aliasing energy there is
     purely artifact — an antialiased, band-limited distant object is not "missing detail," it is
     more faithful than a sharp one.*
   - **Part 1 §3** (the receptor sampling limit) and **§1** (the eye as an optical system) for
     where 60 ppd and the 94-ppd ceiling come from.
   - **Part 5 §5.4** in full (peripheral degradation and LOD from vision): Guenter et al. 2012's
     eccentricity slope **m = 1.32–1.65 arcmin/degree** with 5–6× speedup and 10–15× fewer shaded
     pixels; Patney et al. 2016's contrast-preserving result (**2× larger tolerable blur radius**,
     **up to 70% fewer samples**, coarsening **30° closer to the fovea** than Guenter);
     Albert et al. 2017's latency envelope; Hsu et al. 2017's "barely notice at ≥7.5° eccentricity
     with ≥540p periphery"; and the stereoacuity result. **This is the evidence base for fixed
     crosshair-centred foveation** if Prompt 004 did not already do it.
   - **Part 5 §5.7(d)** — the renderer recommendation that follows from §5.4, and **§5.7(f)** on
     why a luminance vignette is the wrong way to express peripheral degradation.
   - **Part 5 §5.6** — the temporal side, which bounds what TAA can honestly do for distant detail.
6. **`research/grass-rendering-research.md`** — **mandatory for Group AN-D.** The previous side
   session's web research: how voxel engines render grass, wind animation, the hybrid
   ray-march/raster depth composition pattern, performance budgets (it carries the Ghost of Tsushima
   figure: **83 K blades, 2.5 ms, 16 floats/blade**), and the ranked options. Group AG's Checks were
   written from it.
7. **`research/tree-motion-growth-and-appearance.md`** — **mandatory for Group AN-C.** Tree
   biomechanics: cantilever fundamental frequency, multiple-resonance damping, the Vogel exponent,
   flutter thresholds, the pipe model (already implemented, goal 187), LAI (goal 188), and §6.2's
   flutter band that the shading-domain shimmer already uses.
8. **`research/voxel-aesthetics-and-view-distance-research.md`** — **mandatory, and it is the
   specification for Group AN-A. Read §5 AND §9 together, because §9 corrects §5 in four places and
   corrects one of §7's recommendations outright.** The findings that decide this prompt:

   - **§9.5 — a feature is visible far past your OWN horizon, and this changes the whole shape of
     the answer.** `D = c(√h + √H)`. At 1.7 m eye height, 1 m detail vanishes at ~**8.9 km** — but a
     **1000 m massif stays geometrically visible to ~127 km** and a 4000 m peak to ~249 km. **A
     4.65 km far plane cuts off geometry the player can physically see.** The research's own
     conclusion, which supersedes its earlier "4–5 km is defensible": **the correct structure is a
     two-tier far range — full prefiltered octree LOD out to ~9 km, plus a very coarse
     silhouette/impostor tier out to 100+ km.**
   - **§9.7 — the two calibration points that matter, and they agree.** `[C]` Jasmin Patry (Sucker
     Punch), SIGGRAPH 2021: Ghost of Tsushima's *"Froxel grid, 128 W x 64 H x 64 D. Covers entire
     depth range (**10 cm to 100 km**)."* A shipped PS4 title, five orders of magnitude of depth, in
     524,288 froxels — **the far tier can be almost free.** And `[C]` ECMWF's operational forecast
     model: *"**Visibility can not be greater than 100km** as this reflects the extinction
     coefficient of clean air used by the calculation."* **Two entirely independent domains
     converging on 100 km is the strongest evidence in the document for the practical atmospheric
     far bound.**
   - **§9.7 — and the most instructive number for a voxel engine specifically.** `[C]` Microsoft's
     own Bedrock docs: render distance *"up to **96 chunks**"* = **1536 m** (Java's max is 512 m).
     **The most successful voxel game ever ships a ~1.5 km ceiling, two orders of magnitude below
     the geometric horizon, and hides the difference with fog** — its own docs say render distance is
     *"also known as 'fog'"*.
   - **§9.6 — the fog model, properly sourced, and three corrections.** The real WMO CIMO Guide
     (2023, Chapter 9) was obtained and read: it prints **`P = (1/σ)·ln(1/0.05) ≈ 3/σ`** and **the
     constant 3.912 appears nowhere in it.** Within WMO's framework 0.05 is *definitional* for MOR,
     not an alternative to 0.02. The operative wavelength is **550 nm**. Use the two-term
     Narasimhan & Nayar model — `e^(−βd)` attenuation **plus** `L_∞(1 − e^(−βd))` airlight, with
     γ ≈ 0 for fog so β is wavelength-independent — and **front-load the step distribution**, because
     WMO §9.1.1 says *"**Contrary to subjective estimates, most of the airlight entering observers'
     eyes originates in portions of their cone of vision lying rather close to them.**"*
     The authoring rule, `[C]` from the US EPA: *"noticeable degradation of scenic appearance
     (including the disappearance of some features) occurs on some objects as near as **within 10
     percent of the visual range**."* **At clear-air V ≈ 39 km that is ~3.9 km — fog should be doing
     perceptible work from the first few kilometres, and can therefore carry the LOD transition
     rather than fighting it.** Also: CIMO's own reporting scale degrades resolution with distance
     on purpose (*"100 to 5 000 m in steps of 100 m, 6 to 30 km in steps of 1 km, and 35 to 70 km in
     steps of 5 km"*) — a real-world precedent for LOD banding.
   - **§9.1 and §9.4 — two arithmetic corrections you must not inherit.** 20/20 = 1′ MAR =
     30 c/deg = **60 ppd**, while Campbell & Green's 60 c/deg = **120 ppd**; conflating them moves a
     budget by 2×. And **use the centre-pixel spread angle, not deg/px** — the centre pixel subtends
     10.3 % more angle, and the frame-average value under-selects the octree level exactly at screen
     centre.
   - **§9.2** — Ashraf et al. 2025 (Nature Comms) measured the foveal achromatic limit at **94 ppd**
     (89 red-green, 53 yellow-violet), against the 60 ppd display criterion.
   - **§6 and §6.5–6.6 — foveation on a desktop is worth far less than the famous numbers suggest.**
     Guenter's 4.8–5.7× was gaze-tracked at 300 Hz with <10 ms latency; **Tursun et al. (SIGGRAPH
     2019) measured 1.1–1.8× on a 1440p desktop and 1.32× for a ray tracer** — and **0.9×, i.e.
     slower than not foveating, on a simple-shader scene.** Shipping flat-screen VRS gains are
     8–20 %. **No shipped flat-screen game does crosshair-centred fixed foveation**; every verified
     one is content-driven. Read §6.3's table before writing goal 331.
   - **§9.7's three negative results**: RDR2, Death Stranding and Assassin's Creed have **no
     attributable published view distance** — every circulating figure is community-derived. Do not
     cite one. Also: the *Nubis* slide altitudes ("8 km", "4 km", "1.5 km") are **Luke Howard's 1802
     cloud classification, not renderer parameters.**
9. **`research/lin-look-log.md`** §on the shadow rings and the LOD-from-their-own-origin rule —
   because this prompt extends the LOD ladder, and goal 164's lesson (secondary rays judge LOD from
   *their own* origin, never the eye's) is exactly the kind of thing a view-distance change breaks.
10. **The C++ skill reference files** (`~/.claude/skills/cpp-heavy-templates/references/`) —
    **mandatory:**
    - `templates-and-metaprogramming.md` §1 (concepts), §3 (**policy-based design** — the LOD
      ladder is a policy, and the three grass tiers are three policies over one interface),
      §6 (variadic + fold expressions, for the tier composition).
    - `modular-architecture.md` §1–§2 — where a `world/vegetation` or `render/lod` module sits and
      what it may depend on.
    - `memory-and-performance.md` — the instanced grass overlay is a per-frame buffer write of tens
      of thousands of instances. Read the SoA section and the `std::pmr` section before allocating.
    - `release-codegen-and-tradeoffs.md` §1's four buckets — classify each optimisation in this
      prompt before applying it, and say which bucket it is in.

---

## 2. The systems you are building on — verified file map

Checked against the files on 2026-09-06.

- **`app/src/svo_world.hpp`** — `SvoWorldOptions { seed, voxel_size_log2 = -7, root_size_log2 = 9,
  lod_radius = 4.0f, trees, worker_threads }`. `geometry_for(camera)` centres the root on the
  camera in XZ (snapped to 8 m so rebuilds keep voxel alignment) and puts Y over
  `[8 - half, 8 + half)`, with the comment: *"this terrain spans [-64, 64] m plus ~15 m of trees,
  so 128 m+ roots keep every hilltop and tree."* **Note what that means: the 512 m root is already
  four times larger than the terrain's own vertical extent needs, and the horizontal extent is a
  free parameter nobody has tied to a perceptual number.**
- **`world/svo/include/world/svo/tree_builder.hpp`** — the LOD rule, verbatim in its header
  comment: *"A box is refined until its brick voxel edge is <= the target voxel edge at the box's
  distance from `lod_center`: `target(d) = max(finest, d * finest / lod_radius)` — i.e. full
  resolution within lod_radius, doubling with every doubling of distance beyond — a roughly
  constant screen-space voxel size, which is also the Laine-Karras traversal criterion the marcher
  applies on top."* **`lod_radius` is therefore the single knob that decides the whole detail
  ladder, and 4.0 m is not derived from anything.** Deriving it is goal 328.
- **`world/svo/include/world/svo/tree_layout.hpp`** — `TreeGeometry { origin, root_size_log2 = 9,
  voxel_size_log2 = -7 }` with `voxel_bits() = root - voxel`, `max_brick_level() = voxel_bits() - 3`,
  and **`kMaxVoxelBits = 24`** with the reason: *"Float has a 24-bit mantissa: integer voxel
  coordinates and root-normalized positions stay exact only while V ≤ 24, and the traversal's fixed
  stack is sized from this too."* At the defaults, V = 9 − (−7) = **16**, so there are 8 bits of
  headroom — but a 4 km root at 7.8 mm voxels is V = 12 + 7 = **19**, and an 8 km root is 20. **The
  headroom exists but it is finite, and it is a hard correctness limit, not a soft one.** Any
  view-distance proposal must state its V and show it under 24. Beyond that the answer is not a
  bigger root; it is a nested/cascaded root, which is a design decision to make explicitly.
- **`render/diligent/shaders/svo_march.psh.hlsl`** — read in full for this prompt. The relevant
  parts:
  - `g_TreeParams.x` = LOD pixel angle (radians/pixel, quality-scaled); `.w` = AO radius in pixels;
    `g_ShadeParams.w` = the raw unscaled pixel angle.
  - `TraceRay(..., lodPixelAngle, tOffset, maxT, smoothPixelAngle, coverageThreshold)`; primary ray
    uses `maxT = 1.0e30`, so **there is no far clip in the marcher** — the region's own bounds are
    the far limit.
  - **The fog, verbatim**: `heightFactor = exp2(-max(p.y,0)*0.012)`,
    `density = 0.0030 * (0.80 + 0.20*heightFactor)`,
    `rawFog = 1 - exp2(-(dist*density)^2 * 1.442695)`, `fogAmount = saturate(rawFog*1.12)`, then
    `color = lerp(color, SkyGradient(dir), fogAmount)`. Note it is **squared** in distance (a
    Gaussian-ish falloff), not exponential in distance the way Koschmieder is, and `0.0030` is a
    bare constant. Converging on `SkyGradient(dir)` rather than a flat colour is the
    decided-against-flat-fog constraint being honoured — **keep that.**
  - `cubePixels = hit.cubeEdge / max(hit.t * g_ShadeParams.w, 1e-6)`;
    `faceWeight = saturate((cubePixels - 1.5)/3.0)` blends face normal → the node's averaged
    normal as cubes shrink; the grain amplitude fades over the same range (full from 4 px, gone by
    1.5 px). These are the existing distance-fade machinery and they are per-*cube*, not per-metre.
  - The foliage wind block: `MaterialShading(hit.material) == MAT_SHADING_FOLIAGE &&
    g_WindDirSpeed.z > 0` → `albedo *= 1 + strength*(0.10*flutter + 0.06*gust)` with
    `strength = saturate(windSpeed/6)`. **This is the pattern goal 195 (distant grass tint) should
    share**, and it is gated on the *shading model*, which is exactly why the `wind_responsive`
    material member from §1.4 is needed rather than reclassifying Grass.
  - `MATERIAL_COUNT`, `MAT_SHADING_*` and `WIND_*` are macros the C++ passes at shader creation
    from the registry (`render/diligent/detail/{material_macros,wind_macros}.hpp`) — **so adding a
    `wind_responsive` flag is a registry edit plus one macro, not a shader constant.**
- **`world/wind/`** — `WindField` (analytic, deterministic, base + scrolling gust + flutter),
  `WindParams`, `still_wind()`; `render/diligent/shaders/wind.fxh` is the CPU-parity mirror with
  the wave shape compiled in from the C++ header. **Complete and tested** (goals 183–185). This is
  what 190/192/194/195 consume.
- **`world/generation/tree_skeleton.{hpp,cpp}`** (untracked as of this writing) + `tools/tree_dump`
  — space-colonization skeletons with pipe-model radii and LAI leaf mass, four species presets,
  byte-deterministic, dumped to `research/captures/gp_af_skeletons.png`. **Goals 186–188 done.**
  190 (sway), 191 (voxelization), 192 (geometric canopy motion) build directly on it.
- **`world/generation/tree_placement`** — the placement grid and the implicit box-trunk +
  octahedron-lobe shapes that `TerrainSampler::voxelize_trees` currently rasterises. 191 replaces
  the *shape* source with the skeleton; the placement stays.
- **`app/src/tree_decoration.{hpp,cpp}`** — the **mesh** path's tree emitter (box trunk +
  octahedron canopy appended into the chunk mesh, Wood/Leaves materials). Group AF's own note says
  the mesh path does **not** get v2 trees; keep that.
- **`render/diligent/src/post_process.cpp`** — RGBA16F → DiligentFX bloom → soft-knee tonemap.
  Two documented, empirically-found constraints: **`PostProcessor` must be constructed BEFORE the
  renderers** (the scene-target format flows into the terrain/sky PSOs), and **DiligentFX bloom
  needs one warm-up `PostFXContext::Execute` with dummy inputs** before it reports ready.
- **`render/diligent/shaders/svo_taa.psh.hlsl`** — distance-reprojected TAA, `taa_blend = 0.125`
  (an eight-frame history). Distant sub-pixel detail relies on it; so will an instanced overlay
  composed against `SV_Depth`, and **goal 194's Check already requires TAA composition to be
  verified.**
- **Current measurements**, for the budget: GPU march+resolve **3.2–6.3 ms**; **76 fps** ground
  level with shadows+AO; 155–159 fps vsync panoramic; 657k bricks / **395 MB** / 14 levels at the
  default pose; tree GPU memory 376.9 MiB against a 7180 MiB VRAM budget.
  **The research warns vegetation is the worst-case SVO content class** (Laine & Karras), which is
  why goal 191's Check requires measuring brick/MB growth *before* committing.

---

## 3. Standing rules for this pass

Prompt 002 §3 in full. The ones that bite here:

1. **A visual change is verified by a viewed capture on both backends.** Every task in this prompt
   is visual. `--verify-frame`'s percentage is a guard against regression, never the evidence.
2. **CPU reference first, GPU mirror second, oracle always.** 191 and 192 touch the tree
   representation and the marcher; the **7,000-ray brute-force oracle must stay at 0 failures**, and
   goal 192's Check already says the warp applies at shading, not traversal, precisely so it does.
3. **Performance measured**, with a stated budget per feature *before* implementing it, and the
   before/after in the log. Prompt 004 established the frame budget; this prompt spends from it and
   must say how much.
4. **Tests green; new systems get new tests.** 177 is the floor.
5. **Determinism**: placement is a pure function of (seed, position); the same tree gets the same
   ID at every LOD band (the cascading-ID property Part 6/§8 of the terrain research and the
   micro-voxel creators research both call for).
6. **Materials are components.** The `wind_responsive` member goes in `MaterialDef` and is exported
   as a macro; `GrassBlade` is a new def file; no ID literals anywhere.
7. **Main-thread-only events**; background work follows the `SvoWorld` pattern.
8. **No new dependencies without a written case.**
9. **Read the goals.md group notes** for AF and AG before closing 190–195, and handle the two
   decided-against entries in §1.2 explicitly.
10. **Use your skills** — §1.10's reference files by name. In particular classify every
    optimisation into `release-codegen-and-tradeoffs.md` §1's four buckets and say which.
11. **Delegate web research to one or two read-only subagents.** Candidates: (a) instanced
    grass/foliage composition against a ray-marched depth buffer in shipped engines, and the
    per-blade data layouts they use; (b) variable-rate shading tiers and measured gains on
    NVIDIA Ampere/Ada under Vulkan and D3D12, if Prompt 004 left foveation open. Specific question,
    keep coding while it runs, persist to `research/` and cite.
12. **Commit and push per group.**
13. **`git add` explicit paths, never `git add -A`.**

---

## 4. Task groups

### Group AN-A — Every distance constant, derived (goals 326–331)

**326. `render/lod/perceptual`: the criteria, in code, with their citations.**
A small header holding the perceptual constants as named, commented, cited values — not scattered
literals:
- angular resolution: 1 arcmin (20/20, 60 ppd) and the 0.64 arcmin / 94 ppd young-observer ceiling
  (eye research Part 1 §8.1, §3);
- the resolvability distance function $d(s) = s / \tan(\text{MAR})$, so "the distance at which a
  feature of size $s$ stops mattering" is one call;
- atmospheric extinction: meteorological visibility $V$ as the *authored* parameter and $\sigma$ as
  the derived one. **Cite WMO and use its own printed form: `≈ 3/σ` (eq. 9.6, $C_t = 0.05$, which is
  *definitional* for MOR).** The 3.912 form is Koschmieder's historical $C_t = 0.02$ and **appears
  nowhere in WMO-No. 8** — if you want it, multiply σ by 3.912/3 = 1.304 and say you switched
  convention. Evaluate at **550 nm** (aesthetics research §9.6). Note the model is *outside its own
  validity domain* past ~200 km: *"inaccurate… for very clean atmospheres where the curvature of the
  earth becomes a factor."*
- the horizon distance for eye height $h$ — and, **more importantly, the target-height form
  `D = c(√h + √H)`**, because that is what actually sets a far plane (aesthetics research §9.5). The
  coefficient spread across authorities is **3.57 (geometric) → 3.856 (7/6 R refraction) → 3.9215
  (Bowditch/navigational)**, about 10 %; √(2R)/1000 = 3.56959 exactly. Pick one, name it after its
  authority, and record that Young's own caveat is *"occasionally, they will be wildly off,
  particularly if superior mirages are visible."*
- the pixel angle already in the shader (`g_ShadeParams.w`), related to the above so the code can
  answer "is this cube below the eye's limit or merely below the screen's."
**Check**: a unit test asserts the worked values from the research reproduce: a 1 cm feature is
resolvable to **34 m** at 1 arcmin and **~54 m** at 0.64 arcmin; an 18 cm feature to **619 m**;
contrast transmission at 1 km in V = 20 km is **82%** and in V = 10 km is **68%**. Those are the
research's own `pwsh`-worked numbers — if your code disagrees with them, your code is wrong.

**327. Fog with Koschmieder's rate constant.**
Replace the bare `0.0030` and the squared-distance falloff with the physical form: contrast decays
as $e^{-\sigma d}$ with $\sigma$ derived from an authored visibility $V$ in metres. **Keep the
convergence on `SkyGradient(dir)`** — that is the decided-against-flat-fog constraint and breaking
it reproduces a known bug (fogged ridges vanishing while their darker trees linger as floating
dashes). Keep the height factor if it survives inspection: real extinction *is* height-dependent
because air density is, so this is defensible — but state the model you are claiming rather than
keeping an unexplained `exp2(-y*0.012)`.
Expose `--visibility <metres>` and default it to a value you justify from the research's V bands
(10 km light haze / 20 km clear / 50 km very clear / 100 km exceptional).
**Check**: the shader's fog at 500 m / 1 km / 2 km matches the research's transmission table for
the chosen V to within a stated tolerance — **verified by sampling actual rendered pixels**, the
way the water bisection was done, not by reading the formula. Captures at V = 10 km, 20 km and
50 km from the same pose, viewed, on both backends. `--verify-frame` contrast reported before and
after (fog reduces local contrast, so this number will move — say by how much and confirm it is
still well above the 6% threshold).

**328. `lod_radius`, derived instead of chosen.**
`target(d) = max(finest, d * finest / lod_radius)` makes the on-screen voxel size roughly
constant, and `lod_radius = 4.0` sets *which* constant. Derive it: the finest voxel is 7.8 mm, and
the eye stops resolving a 7.8 mm feature at $0.0078 / \tan(1') \approx 27$ m (check this
arithmetic yourself — it is exactly 326's function). So a `lod_radius` that keeps full resolution
out to the *eye's* limit for the finest voxel is an order of magnitude larger than today's, and the
cost of that is the whole subject of Prompt 004.
Reframe the knob: replace `lod_radius` with a **perceptual quality parameter** — "voxels are
refined until their angular size falls below X arcmin" — and compute the radius from it, the
viewport and the FOV. Keep `--lod-radius` as a deprecated alias that back-solves X, because it
appears in `CLAUDE.md` and a dozen research logs.
**Check**: at the shipped default, the on-screen angular size of the hit cube (the existing
`--debug-view cubepx` term, converted to arcmin) is within a stated tolerance of the declared X
across a capture's whole frame — that is the property the rule claims and it has never been
verified. Report the measured distribution. `--lod-radius 4` still produces today's tree
byte-for-byte (assert it, so the alias is provably faithful).

**329. Region size, the two-tier far range, and the V ≤ 24 limit.**
**The research's answer here is a structure, not a number** (aesthetics research §9.5/§9.9): a single
far plane is the wrong shape, because 1 m detail dies at ~8.9 km while a 1000 m massif stays visible
to ~127 km. **Build two tiers:**
 - a **near/mid tier** of full prefiltered octree LOD out to roughly the distance where 1 m features
   fall under the pixel footprint (1.83 km at 1080p/60°) or under 1 arcmin (3.44 km) — pick the
   criterion, and extend toward ~9 km as Prompt 004's throughput allows;
 - a **far silhouette tier** out to 100 km, deliberately almost free. The evidence that this is
   affordable is Ghost of Tsushima's shipped **128×64×64 froxel grid covering 10 cm to 100 km**, and
   ECMWF's independent **100 km** visibility cap. At those distances the *only* thing that survives
   is a fogged silhouette — a coarse impostor, a distant-terrain shell, or the coarsest resident
   octree levels. **Do not build a third mechanism if the coarse levels can do it; measure first.**
   Calibration comfort: Minecraft Bedrock's shipped ceiling is **1536 m** and it hides the boundary
   with fog, so this tier is a luxury, not a requirement — **if it does not fit the budget, ship the
   near/mid tier with honest fog and open a goal.**
Then **show the arithmetic for the near/mid tier**: root edge, finest voxel,
V = `root_size_log2 - voxel_size_log2`, and V < 24 with margin. If it needs V ≥ 24, the answer is a
**cascaded root** (nested regions at coarsening finest-voxel sizes, the classic clipmap shape), not
a bigger V — because `tree_layout.hpp` states plainly that float's 24-bit mantissa is what keeps
integer voxel coordinates and root-normalised positions exact, and the traversal's fixed stack is
sized from it. **Note that Prompt 004's Group AK-C may already have moved to a grid of shallow
trees, in which case the cascade is largely built — read its log before designing anything.**
If you go cascaded, that is a real architecture change: write the design in the log first, name
what it does to `TraceRay`'s stack and to the oracle, and get the CPU reference and the oracle
green before the shader.
**Check**: the arithmetic in the log, with V stated and under 24. If single-root: a capture showing
the region's far edge is beyond where fog has already taken the image to sky (i.e. the boundary is
*invisible*, which is the only acceptable way for a bounded region to end) — viewed, both backends.
If cascaded: the 7,000-ray oracle at 0 failures, then the shader mirror, then the capture.

**330. Horizon and the far plane.**
There is no far clip in the marcher (`maxT = 1.0e30`); the depth write comes from `g_ViewProj`, so
the projection's far plane still governs `SV_Depth` precision. With a view distance measured in
kilometres, depth precision becomes a real question. Check what the far plane currently is, state
the depth precision at 1 km and at the new far distance, and decide (reversed-Z is the standard
answer; say whether Diligent and both backends support it here and what it costs to switch).
**Check**: the depth-precision arithmetic in the log; a capture at the new far distance with no
z-fighting on distant terrain, viewed, both backends. If you switch to reversed-Z, the post chain
and the TAA reprojection both read depth — verify both, with captures.

**331. Fixed foveation, if Prompt 004 left it open.**
Eye research §5.4 and §5.7(d) give the numbers: coarsening beyond ~7.5° eccentricity at ≥540p
equivalent is barely noticeable; Patney's contrast-preserving filtering doubles the tolerable rate;
Guenter's m = 1.32–1.65 arcmin/degree is the slope budget; and §5.7(f) warns that the wrong way to
express this is a luminance vignette. For a marcher, the natural lever is the **LOD pixel angle as
a function of screen eccentricity** — a one-line change to `g_TreeParams.x` becoming a per-pixel
value — and/or hardware VRS (see §6 of the aesthetics research for tiers and measured gains).
**Check**: measured GPU ms saved at the shipped default, and a **blind-ish A/B**: two captures of
the same pose, foveated and not, and an honest written judgement of whether the periphery looks
degraded. If it does, dial back to Hsu's ≥7.5° / ≥540p envelope and re-capture. If the saving is
under 10%, say so and turn it off — an optimisation that costs image quality for single digits is
not worth the code.

---

### Group AN-B — What the eye's limit means for shading (goals 332–334)

**332. Contrast-preserving distance filtering, not detail deletion.**
Part 1 §8.3's aside is emphatic: beyond ~60 cyc/deg no detail survives the eye's optics, so
aliasing energy there is *purely* artefact, and *"an antialiased, band-limited distant object is
not 'missing detail', it is more faithful than a sharp one."* Meanwhile §8.3's mechanism 3 says the
contrast threshold *rises* for small targets, so distant things fade faster than transmission alone
predicts. The existing `faceWeight`/grain fades are already doing a version of this per-cube.
Audit them against the criteria from 326: express the fade in **arcmin of angular size**, not in
pixels, so it means the same thing at any resolution, and state what the current pixel-based
thresholds (1.5 px, 4 px) correspond to in arcmin at the shipped FOV and viewport.
**Check**: the conversion table (px ↔ arcmin at the shipped and at 4× resolution) in the log; the
fades re-expressed in arcmin with the pixel behaviour unchanged at the shipped resolution (assert
byte-identical captures at the shipped setting, then capture at 4× and show it is *now* correct
where it previously would have been wrong).

**333. Distance shimmer for the Grass material (goal 195).**
Goal 195's Check, verbatim from `docs/goals.md`: *"viewed capture at 60 m across a valley;
`--no-wind` kills it; debug views unaffected."* Implement it by sharing the foliage shimmer's code
path, gated on the new `wind_responsive` material member rather than on `Shading::Foliage`
(§1.4's answer). This is the cheapest task in the prompt and it is what makes a distant meadow read
as alive at zero geometry cost.
**Check**: goal 195's Check as written, plus: `--no-wind` produces a bit-identical image to the
pre-change build (assert it), and the debug views are unaffected (assert `--debug-view material`
and `normal` are unchanged).

**334. The `wind_responsive` material member.**
`world/materials`: add the member to `MaterialDef`, set it on Leaves and Grass (and GrassBlade
from 335), export it as a macro alongside `MAT_SHADING_*` via
`render/diligent/detail/material_macros.hpp`, and switch the marcher's foliage-wind block to test
it instead of the shading model. **Do not change Grass's shading model** — Group AG's note explains
why (it would wrongly give ground grass the mesh path's canopy sway).
**Check**: the Group AC property holds — adding the member touched the registry and the macro
header and nothing else; grep finds no material-ID literal in any consumer. A test asserts the
macro's value matches the registry for every material. Both backends compile (remember FXC's
X3500: no runtime-indexed vector component writes).

---

### Group AN-C — Trees that move (goals 335–337 = goals 190–192 completed)

These are Group AF's unstarted goals. **Their Checks are already written in `docs/goals.md` and
they stand as written**; the numbers below are the same goals, restated here with their
dependencies made explicit. When you close them, mark **190/191/192** `[x]` in place.

**335. Hierarchical spring sway (closes goal 190).**
Trunk fundamental from the cantilever formula, branches semi-independent so multiple-resonance
damping emerges structurally rather than being scripted. Consumes the existing `WindField`.
`research/tree-motion-growth-and-appearance.md` has the biomechanics; the skeleton from goal 186
has the structure.
**Check** (as filed): step response and resonance near the predicted **f₀ ≈ 0.26 Hz** for
sycamore-scale parameters; determinism; **≤ 0.5 ms/frame for all in-ring trees, measured with the
attributor**. Add: a captured frame sequence across one sway period, viewed, so the motion is seen
and not only measured.

**336. Skeleton-driven voxelization (closes goal 191).**
Capsule trunks and per-segment leaf clouds from the skeleton, replacing the implicit box+octahedron
shapes in `TerrainSampler::voxelize_trees`.
**Check** (as filed): **brick/MB growth measured BEFORE committing** — the research names
vegetation as the worst-case SVO content class; `--verify-frame` stays ≥ 25%; the
terrain-sampler equivalence test still passes byte-for-byte; viewed captures at **2 / 10 / 60 m on
both backends**. Add, because Prompt 004 changed the memory picture: state the growth against the
resident-cache budget 004 established, not against 395 MB.

**337. Geometric canopy motion in the marcher (closes goal 192).**
The bounded experiment: domain-warp the sample position inside canopy bricks. The shading half is
already shipped (goal 185).
**Check** (as filed): captures at **2 m and 10 m**, TAA ghosting evaluated in a slow pan, **oracle
still 0/7,000** (the warp applies at shading, not traversal). **An honest negative result is an
acceptable outcome here** — if it ghosts or costs too much, write that down with the numbers and
the capture, and close the goal as "attempted, rejected, here is why."

---

### Group AN-D — Ground cover (goals 338–340 = goals 193–195; 195 is 333 above)

Group AG, unstarted. This reopens the decided-against goal 40 ("grass ground-cover geometry —
needs instancing+textures"): the new evidence is `research/grass-rendering-research.md`, which
supplies both the instancing pattern and the depth-composition technique. **Say so explicitly in
the log** rather than quietly contradicting the decided-against list.

**338. Voxel blade clusters in the finest ring (closes goal 193).**
A new `world/materials/defs/grass_blade.hpp` with `Phase::Foliage` and `wind_responsive`;
deterministic cluster placement voxelized into the finest LOD ring only.
**Check** (as filed): determinism; **< +15% build time and < +10% tree MB at the default pose, or
halve density and log the tradeoff**; viewed captures at **1 / 4 / 15 m**; walk through it with no
collision and no aim-readout lie. Add: with Prompt 006's biome field in place, grass density comes
from biome and moisture, not from a constant — assert per-biome density against the terrain
research's ground-cover numbers (Part 6 §7).

**339. Instanced raster overlay composed against the march's depth (closes goal 194).**
The hybrid pattern the grass research documents: raster blades within a near radius, composed
against the marcher's `SV_Depth`. Layered wind from the shared field plus a player-position
sphere-mask bend.
**Check** (as filed): correct occlusion both ways on a hillside; wind sweep visible in a sequence;
walking bends it; frame cost measured and **inside the budget**; both backends. Add: state the
per-blade data size and the blade count, and compare to the research's Ghost of Tsushima figure
(83 K blades, 2.5 ms, 16 floats/blade) — if you are far off in either direction, say why. And
verify TAA explicitly: an instanced overlay entering a distance-reprojected TAA history is the
single most likely source of ghosting in this prompt.

**340. The three tiers must agree at their boundaries.**
Voxel blades → instanced overlay → shimmer is three representations of one thing, and the classic
failure is a visible ring where one hands over to the next. This is exactly the cascading-ID
problem the terrain research (Part 7 §8) and the micro-voxel creators research both name: the same
grass tuft must have the same identity, position and phase in all three tiers.
**Check**: a capture sequence walking slowly outward across both boundaries, viewed, with **no
visible pop or ring** — and if there is one, the honest report of it plus the measured distance it
occurs at. A test asserts a tuft's ID and world position are identical in all three tiers for 1,000
random tufts.

---

### Group AN-E — The budget, and the whole view (goals 341–345)

**341. The frame budget, spent and accounted.**
Prompt 004 established a frame budget and a target above 150 fps. This prompt spends from it: fog
(free), derived LOD (potentially expensive), foveation (a saving), sway (≤ 0.5 ms), voxel grass
(build time and MB), the raster overlay (GPU ms), shimmer (free). Produce one table: feature,
bucket (per `release-codegen-and-tradeoffs.md` §1), CPU ms, GPU ms, MB, and the running total
against the budget.
**Check**: the table in the log and in the harness report. The shipped default is **still above the
fps target 004 set**, measured on `stress_pose` and `walk_hillside` and `valley_far`, both
backends. If it is not, features come off the default (not out of the codebase) until it is, and
the log says which and why.

**342. The hilltop shot.**
One capture, at the shipped default, from a hilltop, looking down a valley, at the new view
distance, with grass, moving trees, derived fog, and a river from Prompt 006. This is the frame the
whole arc exists to produce.
**Check**: the capture exists, is viewed, is committed, and is named in the log and in
`docs/progress.md`. Both backends, and the two must agree to within the noise floor measured in
Prompt 002 goal 217.

**343. The scenario library grows.**
Add harness scenarios for the new surfaces: `valley_far_v` (fog sweep across V values),
`grass_walk` (the three-tier boundary walk), `tree_sway` (one sway period, sequence capture),
`horizon` (the far-region-edge invisibility check). Register the cheap ones in `ctest -L scenario`.
**Check**: all four run on both backends, baselines committed, and the goldens promoted.

**344. Cross-check the old distance machinery for regressions.**
This prompt changes the LOD ladder, the fog, and possibly the far plane. Three things historically
break when those move: **the shadow rings** (goal 164 — secondary rays must judge LOD from their
own origin, never the eye's), **the sliver curtains** at grazing angles (the open defect in
`research/water-foliage-design.md`), and the **moiré** the smooth-normal blend exists to suppress.
Re-check all three deliberately.
**Check**: `--debug-view lit` and `ao` at a hilltop at the new view distance show no ringing
(viewed); a grazing-angle capture is compared to `sliver_closeup.png` and the state of that defect
is reported (better, same, or worse — with the capture); `--debug-view smooth` and the moiré
assessment at 100 m / 500 m / the new far distance, viewed. **If any of the three got worse, say
so and either fix it or open a goal — do not let a view-distance win hide a shading regression.**

**345. What this pass deliberately did not do.**
Record with a reason and a follow-up goal each: eye-tracked foveated rendering (§5.7(g)); varifocal
(§5.5, hardware does not exist); depth of field (§5.5, reads as camera and fights attention without
gaze tracking); motion blur (already decided against in Prompt 003 — cross-reference it);
per-object cards/billboards for very distant vegetation if you did not need them; seasons and
vegetation succession (Prompt 006's list); flowing water (goal 199).
**Check**: the list is in `docs/goals.md` with Checks, and in the log with reasons.

---

## 5. Sequencing and risk

- **AN-A first**, and 326 before everything in it — the constants module is what the rest cite.
  327 (fog) is the cheapest visible win in the prompt and a good first commit: it is a shader edit,
  so it needs no rebuild.
- **328/329 are the expensive pair.** They are where Prompt 004's headroom gets spent, and 329 may
  turn into a cascaded-root architecture change. **If it does, stop and write the design in the log
  before writing code**, and take the CPU reference and the oracle first. A bigger V that quietly
  exceeds 24 is a correctness bug that will present as random geometry corruption at distance, and
  it will be very hard to attribute later.
- **AN-B is small and independent.** 334 unblocks 333 and 338.
- **AN-C and AN-D are independent of each other** and both depend on AN-B's material member.
  Within AN-C: 335 → 336 → 337. Within AN-D: 334 → 338 → 339 → 340.
- **AN-E is last** by construction.
- **Commit and push per group.** Six or seven commits.
- **Legitimate negative results, named in advance**: 337 (geometric canopy motion) has no public
  prior art and goal 192's Check already allows an honest negative; 331 (foveation) should be
  turned off if the saving is under 10%; 339's blade budget may not reach the research's figure on
  this hardware. In all three, the honest measured negative with a capture is a completed goal.
- If a task is blocked on Prompt 004 or 006, finish everything that is not, and state exactly what
  is blocked and on what.

---

## 6. Explicitly out of scope this pass

- **The GPU architecture, the resident cache, brick compression, the marcher's traversal
  performance.** Prompt 004.
- **The stipple/grain aesthetic itself.** Prompt 005 owns what the surface looks like; this prompt
  owns how far you can see it and what grows on it.
- **Terrain generation.** Prompt 006.
- **Player physics and collision.** Prompt 003. Grass gets no collision (goal 193's Check says so).
- **Eye-tracked foveation, varifocal, DOF, motion blur, lens ghosts, vignette.** All deferred or
  decided against, with citations, in 345 and Prompt 003's 239.
- **Flowing water (goal 199), editing (160), multiplayer, save/load, audio.** Not this arc.
  (Audio: note that `research/` holds four large audio/hearing documents — 333 KB of
  `sound-physics-and-audio-research.md` alone. That is a whole arc of its own and nobody has
  started it. Worth a line in the pass log as the obvious next arc after this one.)

---

## 7. When you are done

1. Write **`research/view-distance-and-cover-log.md`**: 326's constants with the research values
   they reproduce; the fog model and the pixel-sampled verification; the `lod_radius` → angular-size
   reframing with the measured distribution; the V arithmetic and the region decision (and the
   cascaded-root design if you went there); the depth-precision decision; the foveation A/B with
   its honest judgement; the sway/voxelization/canopy results including any negative; the three
   grass tiers with the boundary assessment; the full budget table; and the 344 regression
   cross-check with all three defect states.
2. Add **Group AN** to `docs/goals.md`, goals 326–345 numbered exactly as above, each `[x]` with
   its Check recorded as performed. **Mark 190, 191, 192, 193, 194, 195 `[x]` in place**, each
   pointing at the goal here that closed it, with the measured numbers their filed Checks demand.
   Note goal 40's reopening.
3. Refresh `docs/progress.md`: the current-state paragraph gains the view distance, the derived
   constants, the grass tiers and the moving trees; add this pass's entries to *"Decisions that
   survived contact with evidence"* — including the reopening of goal 40 and the deferrals in 345.
4. Add **`docs/view-distance.md`**: every distance constant, what perceptual criterion it comes
   from, its research citation, and how to change it safely (especially the V ≤ 24 limit). This is
   the document that stops the next pass from putting a round number back.
5. Update `CLAUDE.md` with operational deltas only: `--visibility`, the reframed LOD flag and its
   alias, the new region default and its V, the new scenarios.
6. Update `Prompts/README.md`'s index row for 007.
7. Full suite green, both backends, the hilltop shot from 342 viewed, committed and named — and
   the three regression checks from 344 reported honestly.

---

*Provenance: written by the side session on 2026-09-06 after reading
`render/diligent/shaders/svo_march.psh.hlsl` in full (fog, `cubePixels`/`faceWeight`, the grain
fade, the foliage-wind block, `TraceRay`'s parameters and the absence of a far clip),
`render/diligent/include/render/diligent/svo_renderer.hpp`,
`world/svo/include/world/svo/{tree_layout,tree_builder,brick_tree,terrain_sampler}.hpp`
(including the `kMaxVoxelBits = 24` constraint and the `target(d)` LOD rule verbatim),
`app/src/svo_world.{hpp,cpp}` (the region geometry and the 8 m snap), `docs/goals.md` Groups
AF/AG/AH in full with goals 190–199, `docs/progress.md`'s decided-against list (goal 40 and the
flat-fog entry), and `research/human-eye-and-vision-research.md` Part 1 §8.1–§8.3 (including its
worked distance and transmission tables) and Part 5 §5.4–§5.7. The measured current numbers
(3.2–6.3 ms GPU, 76 fps ground level, 657k bricks / 395 MB / 14 levels, 376.9 MiB tree GPU memory)
were read off `docs/progress.md` and the viewed capture `svo_ground_hilltop.png`.*
