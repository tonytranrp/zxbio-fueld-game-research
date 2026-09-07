# Gameplay, wind and water pass — decision log

Prompt: `Prompts/001-2026-09-07-gameplay-physics-world-life.md`. Groups AD (player physics), AE
(wind), AH (water motion) and C7 (mesh-path wind parity) landed, and AF partially (skeletons, pipe
model, leaf mass — not the sway or the voxelizer); group AG (grass) did not start — §8 and §8b say
exactly where they stand and why, which is the point of this file.

Same style as `research/lin-look-log.md`: what was measured, what was decided against, and the
numbers behind both.

---

## 0. Method

Everything below was verified by running the thing, not by reading it. The tools were the repo's
own, in the order the standing rules put them: unit tests for anything pure, the mechanical
`--autofly --walk` / `--verify-frame` checks for anything that could silently break the world, and
viewed captures for anything visual — plus, new this pass, **numeric frame diffs** for effects too
subtle to judge from a still. That last one earned its place: "does the wind actually reach the
leaves" is a question a screenshot answers badly and a per-pixel diff answers in one line.

---

## 1. Where the player physics lives, and why not where the brief said

The brief suggested keeping the fixed-timestep accumulator in `SpectatorCameraState` or "a new
`app`-level struct". It went into a new module, `world/player/`, instead.

The reason is CI, not taste. `app/` is only built when `VOXEL_BUILD_RENDERER=ON`; the gating
no-GPU `core` job configures with it OFF. A jump/coyote/buffer state machine whose entire value is
that it behaves identically on every machine would have had tests that never ran in the job that
gates merges. Nothing in the controller needs a window, a device or GLFW, so nothing in it belonged
behind the renderer flag.

The knock-on was better than expected: `--noclip` stopped being a second copy of the physics. It is
now `step_player` against an `OpenWorld` query in which nothing is solid — same code path, same
tick, one less thing to keep in sync. `world/collision`'s `SolidQuery` concept made that free.

## 2. The collision bug the step-up test found

Writing A3's "climb a 4 cm lip" test surfaced a real defect in shipped collision code.

**Symptom.** The body climbed the lip correctly on tick 4 (eye 1.70 → 1.74) and then sank off it
over the next three ticks, ending back at 1.70.

**Diagnosis.** Tracing every `overlaps_solid` call for the failing tick showed the whole frame
spending exactly **one** query, which returned "solid". That is `move_and_slide`'s "started inside →
move unblocked" escape hatch: it hands back the entire wanted motion unclipped, so gravity walked
the body straight through the floor it was standing on.

**Root cause.** The caller stores the camera EYE and rebuilds the feet as `eye - eye_height` every
tick. That round trip is worth about 1e-7 — `1.74f - 1.7f` is `0.0399999`, not `0.04` — so a body
the sweep had placed *exactly* on a surface reads back as marginally INSIDE it. A voxel world puts
surfaces at exact coordinates constantly, so this is reachable, not theoretical.

**Fix.** A 1 mm depenetration skin, tried before the escape hatch: lift the body by the skin, and
if that frees it, sweep normally from there and include the lift in the reported motion (so it
converges instead of oscillating). The escape hatch itself is unchanged for a genuinely embedded
body — `--autofly`'s teleport-through-mountains smoke test still means what it meant. Both cases
have their own regression test now.

The lesson is the one worth keeping: **a "never trap the player" escape hatch is also a "silently
disable collision" hatch**, and the difference between the two is a tolerance nobody had written
down.

## 3. Fixed timestep, and the harness bug it exposed

`--autofly --walk`, 900 frames, immediately after the fixed-step switch: **74 ground violations**
(0 required). Before the change it was 0.

Not a physics regression. `--autofly` teleported the camera +X once per FRAME, after the physics.
At ~150 fps against a 60 Hz sim, roughly 60% of frames run **zero** ticks — so the body was shoved
into a new column with nothing to settle it, and the check that runs after correctly said so. The
harness had been silently relying on physics running every frame.

Moving the travel INSIDE the tick, before the physics that has to answer for it, restored **0
violations** and kept the smoke test's meaning. Worth stating plainly: the fixed-step change did not
cause the violations, it *revealed* that autofly was not part of the simulation it was testing.

Slow frames over the same 900-frame run: **1**, on a tree swap — the known goal-175 hitch,
unchanged. No new `camera`-phase cost.

## 4. Jump numbers, and where they came from

`apex = v0² / 2|g|`. With the world's `-32 m/s²`, the brief's 1.0–1.25 m band means v0 ∈ [8.0, 8.94].
Chosen **8.5 m/s → 1.129 m**, and a test pins the arithmetic so a gravity change cannot move the
apex silently. The measured in-sim apex (discrete integrator, 60 Hz) lands inside the same band.

Coyote 0.1 s, buffer 0.1 s, both counted down in the same pure function. `PlayerIntent::jump_pressed`
is an **edge** by contract — the app takes it once per frame and gives it to exactly one tick — with
a test asserting a held key produces exactly one jump over 240 ticks.

## 5. The step height is a smoothing budget

0.55 m is a 1 m-block relic. On the svo path every natural slope is a sub-centimetre staircase, so
the failure mode is micro-jitter, not blocked stairs. The svo path now runs **0.04 m** (~5 voxels)
and the mesh path keeps 0.55.

Eye smoothing is written in terms of the OFFSET rather than the position, which collapses the whole
thing to one line: if the body moved `d` this tick and the offset was `o`, the new offset is
`(o - d)·exp(-dt/τ)`. Standing still it decays to zero; a step-up of `d` puts the view `d` below the
body and it catches up. Disabled outright while airborne, so a fall never leaves the view trailing.

**Deliberately not captured as an image sequence.** A 4 cm effect at a 0.1 s time constant is not
something a still frame or a frame-to-frame diff shows honestly. The unit test asserts the strictly
stronger claim — the lag never exceeds the step budget over a 120-tick staircase climb, and never
exceeds the hard clamp under a teleport — and `--autofly --walk` confirms the body itself is
untouched. Recorded as a knowingly weaker verification than the brief asked for.

## 6. Wind: one field, and why it is sines

`world/wind`, not `engine/wind`: `engine/` is infrastructure a different game reuses unchanged;
wind is *this world's weather*, sized for terrain a player walks across, beside `world/generation`.

**Sines, not FastNoise2**, despite the rest of the project using it. The function has to exist twice
— C++ and HLSL — and produce the same numbers. A hash-based noise cannot promise that across two
compilers and two float pipelines; a sum of directional sines can, exactly, because it is only `sin`
and `dot`.

**The constants are not duplicated at all.** `world/wind`'s own header holds the wave shape;
`render/diligent/detail/wind_macros.hpp` compiles those same values into every wind-aware shader as
`WIND_*` macros, the way Group AC's material macros already worked. `shaders/wind.fxh` contains no
numbers of its own. That is the brief's "one source of truth" as a property of the build rather
than a promise in a comment — the parity question the brief worried about is *unrepresentable*
rather than merely tested. Per-run tuning goes through the constant buffer, because `--wind-speed`
has to work without recompiling a shader.

Gusts **travel**: the sampling frame scrolls along the wind, so a gust is a patch of fast air moving
downwind rather than a pulse in place. There is a test for exactly that — what a point sees now is
what the point upwind saw a moment ago, to 1e-4.

`--no-wind` is `still_wind()`, which zeroes the FIELD. Not a flag each consumer checks and one
consumer eventually forgets.

## 7. What the numeric frame diff bought

Three effects this pass are motion, not structure, and a still frame judges them badly. Diffing two
frames and classifying the pixels that changed turned each into a one-line fact:

| effect | pixels changed | of those, on the right thing |
|---|---|---|
| C6.1 foliage shimmer, wind 6 vs `--no-wind` | 18,018 | **98.2% foliage green** (mean 108,158,80) |
| E1 Gerstner water, wind 8 vs `--no-wind` | 52,078 | **98.5% water blue** (mean 100,145,173) |
| C7 mesh canopy sway, wind 8 vs `--no-wind` | 1,473 | 27.9% green — see below |

The mesh number looks wrong until you notice what it proves. The svo path changes *shading*, so
only foliage pixels move. The mesh path moves *geometry*, so it also changes what is visible behind
a canopy edge — terrain revealed and occluded. A 98% green result there would have meant the
displacement was not happening.

This technique is worth reaching for before a capture sequence whenever the question is "does X
affect Y and only Y".

## 8. Trees v2 (AF): the skeleton is done; the sway and the voxelizer are not

Goals 186-188 landed after the main pass: space-colonization skeletons, pipe-model radii, and leaf
mass. Goals 190-192 (the sway oscillator, skeleton voxelization into the svo tree, geometric canopy
motion) did not. Grass (AG) was not started at all.

**Two calibration errors, both caught by tests rather than by reading the code**, and both worth
recording because the *shape* of the mistake repeats:

1. `area_per_leaf_unit` was set to 6e-5 m² of sapwood per m² of leaf, an order of magnitude under a
   real tree's Huber value (~5.7e-4, from an oak at 0.6 m DBH carrying ~500 m² of leaf). Every
   radius in the tree then fell under the 1 cm twig floor, so the floor *became* the model — and the
   pipe-model test failed by reporting a trunk exactly as thick as a twig. A constant that produces
   a plausible-looking picture can still be an order of magnitude wrong; only a test that asserts a
   RATIO catches it.
2. Leaf area was first hung on each tip as a fixed amount. The single-seed test passed. Strengthening
   it to twelve seeds killed it immediately: tip count varies about **sevenfold** between seeds (34
   at one, 245 at another), so the same species came out at LAI 0.3 or 8.7 depending on the seed.
   The fix is conceptual, not numeric — leaf area is a property of the **crown** (LAI × footprint,
   shared over whatever tips grew), and tip count is only how finely the crown is subdivided. LAI is
   now exact by construction.

The second one is the more useful lesson: **a single-seed test of a stochastic generator tells you
almost nothing**, and the version that passes is the one that hides the variance.

`tools/tree_dump` writes a skeleton as .obj line elements so it can be looked at before anything
voxelizes it — a skeleton bug is obvious in a picture and nearly invisible in a voxel field.
`research/captures/gp_af_skeletons.png` shows all four species: a dome on a bare stem, a cone, a
trunkless spreading shrub, and a tall narrow aspen, with a metre scale bar. The round broadleaf ends
up 8.4 m tall on a 16 cm trunk, which is the right order for a real tree of that size.

## 8b. Grass (AG): not done, and what stands

Neither group was started. This is a scope outcome, not a discovery — the pass ran out of budget
after A, B, E and C7, and the honest thing is to say where the line is rather than land half a tree
system.

What exists that they build on: the wind field (AE) is complete and is what both groups were going
to consume; C6.1's shading-domain foliage response is in and working, which is the half of C6 the
brief said to ship even if the geometric half stalls; C7 retired the last per-feature wind. What
does not exist: skeletons, pipe-model radii, leaf mass, the sway oscillator, skeleton voxelization,
and every part of grass.

One design question they will hit, recorded now because the answer is not obvious: **D3's distant
grass tint needs the Grass material to be wind-responsive, and Grass is `Shading::Lit`, not
`Shading::Foliage`.** Changing its shading model would also give ground grass the mesh path's
canopy sway, which is wrong. The materials-as-components answer is a new `MaterialDef` member
(`wind_responsive`) exported to shaders alongside the shading model — one file in `defs/` per
material plus one field, no `== MaterialID::X` at any consumer. That is the shape of the fix; it was
not made, because making it without a consumer would be speculative.

## 9. Water: scope held, spectrum corrected by a capture

Gerstner, four components, deep-water dispersion `ω = √(gk)`. Wind and waves share one field, so
`--no-wind` is glass by construction.

**The steepness budget is a correctness constraint.** `Σ Q·k·A > 1` makes a Gerstner surface
self-intersect into visible loops. The field targets 0.6 and a test pins it at every wind speed from
0 to 25 m/s — not just the shipped default, which is where that class of bug hides.

**Normals from the same sum, analytically.** Gerstner displaces points horizontally as well as
vertically, so a finite difference of the height disagrees with the crests at exactly the
steepnesses that make crests worth having. A test compares the analytic normal against the real
geometric normal of the displaced surface (`dot > 0.999` over a grid).

**The spectrum was wrong the first time, and a capture said so.** Components scaled
1.00/0.61/0.37/0.19 make `--wind-speed 8` a field of 51 m down to 9.7 m — all swell, no chop — and
the water read *flatter* than the fixed ripple lattice it replaced, because from a shoreline you see
barely two wavelengths of it. A real sea carries long swell and short chop simultaneously; that is
what a spectrum is. The range now spans a factor of ~14 (51 / 23 / 9.2 / 3.6 m at 8 m/s) with a
milder amplitude falloff, because short chop is steep in reality and the first falloff made the
short components invisible (0.6 cm) while still spending their share of the budget.

Worth noting **what made that fix cheap**: the field is derived on the CPU and only summed on the
GPU, so correcting the spectrum was a C++-only change with no shader edit. That was the payoff of
deciding the derivation lives in one place.

Captures: `gp_e_calm` (0.5 m/s, glass), `gp_e_chop` (3), `gp_e_swell` (8, long crest bands sweeping
the bay), plus `gp_e_swell_d3d12` — both backends agree.

## 10. E3 (shore fade): half done, deliberately

`shore_fade()` implements the research's §2.3 criterion — a wave cannot orbit in water thinner than
about half its wavelength — and the CPU side uses it. **The shader passes 1.0.**

The reason is specific: a probe ray straight down from the water surface hits the water column's own
voxels immediately, so getting a depth in the marcher needs a water-skipping traversal variant, and
that is a change to the marcher the 7,000-ray oracle guards. It was not worth spending that
budget here, for a reason that also matters: **this implementation displaces normals, not geometry**,
so the artefact E3 exists to prevent — crests clipping through the sand — cannot occur. What is
missing is only that shallow water should look calmer than open sea.

Named follow-up rather than a guess.

## 11. Two tooling defects found in passing

- **`--dump-every` wrote nothing and said nothing.** `dump_frame`'s result was `(void)`-discarded
  and the path was relative to whatever the working directory happened to be. A whole capture
  session produced no files and no error. It now honours `VOXEL_DUMP_FRAME` as the stem (absolute
  paths, numbered) and logs `written`/`FAILED` either way. This is why the first several capture
  attempts in this pass looked like the app was broken.
- **The mesh path's overlay showed `wind: 0.0 m/s` regardless of `--wind-speed`**, because only the
  svo path filled those stats. Caught by reading a capture, not by a test.

Also added: `--crosshair` / `--no-crosshair`. The crosshair is off under `--verify-frame` by default
so a HUD cross cannot inflate the local-contrast metric; the override exists so a capture can show
it anyway.

## 12. What was decided against

- **A second fullscreen pass for the crosshair.** The brief offered it as the fallback if an ImGui
  crosshair came out anti-aliased into mush by TAA. It does not: the overlay pass already runs
  *after* the TAA resolve, so ImGui's foreground draw list gives a crisp pixel cross for free. No
  new PSO.
- **Velocity-based horizontal movement** (acceleration/friction). The brief did not ask for it, it
  would have changed the feel of every existing check, and direct wish-velocity is what the walk
  invariant and `--autofly` were tuned against. Vertical stays velocity-integrated as before.
- **Interpolating the render pose between simulated poses.** `FixedStepper::alpha()` exists and is
  tested, but the camera renders the current pose. At 60 Hz sim against 150 fps render the visible
  artefact is well under the eye-smoothing time constant already applied, and interpolating would
  have put the rendered eye somewhere the collision never validated. Available if a lower sim rate
  is ever wanted.
- **Changing `Grass` to `Shading::Foliage`** to get D3's shimmer cheaply — see §8. It would give
  ground grass the mesh path's canopy sway.
- **FFT/Tessendorf water, currents, foam, breaking.** Out of scope by the brief and by judgement;
  §10 names the follow-up.

## 13. Numbers, before and after

| measure | before | after |
|---|---|---|
| tests | 119 | **176** |
| `--autofly --walk` 900 frames, ground violations | 0 | **0** (74 mid-pass, §3) |
| slow frames (>20 ms) in that run | 1 (tree swap) | **1 (tree swap)** |
| `--verify-frame`, Vulkan | 34.0% | **34.7%** |
| `--verify-frame`, D3D12 | 34.0% | **34.6%** |
| gpu march+resolve | 3.2–6.3 ms | 4.2–7.0 ms observed, same band |

## 14. Sources

`research/grass-rendering-research.md` (§3 hybrid depth composition, §5 the ranked options — the
overlay plan D2 was to follow), `research/micro-voxel-creators-research.md` (§4 character
controllers at micro-voxel scale: the smoothing-budget conclusion behind §5, rlVoxel's liquid
pop-up behind A5's shore assist), `research/tree-motion-growth-and-appearance.md` (§6.2's 3–5 Hz
sunfleck band behind C6.1's flutter; §3/§5/§8/§9 are what AF would have used),
`research/water-physics-and-wave-simulation.md` (§8.2 Gerstner and the steepness warning, §2.2
dispersion, §2.3 the depth-vs-wavelength criterion behind `shore_fade`), and Ghost of Tsushima's
constant-direction/Perlin-magnitude wind model as reported in the grass research, which is the shape
of `world/wind`. Everything numeric in this file is this session's own measurement.
