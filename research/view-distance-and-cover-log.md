# View distance and living cover — Prompt 007's decision log

Group AN in `docs/goals.md` (goals 326–345), closing Group AF's 190–192, Group AG's 193–195, and
reopening goal 40. Every number here was measured in this pass on this machine, RelWithDebInfo,
1280×720, unless it is cited to a research file.

The arc in one sentence: **the renderer's distances stopped being round numbers somebody chose and
became perceptual quantities with citations**, the region grew 8× to match, and the world it now
shows got trees that move and ground that is covered.

---

## 1. The perceptual header, and three corrections that were already wrong (goals 326, 327)

`render/lod/perceptual.hpp` is the file the rest of the pass cites. Header-only, dependency-free,
and registered **outside** the `VOXEL_BUILD_RENDERER` guard because `world/svo`'s LOD ladder needs it
too and it has to build in the no-GPU CI job.

Its Check said: *"those are the research's own worked numbers — if your code disagrees with them,
your code is wrong."* It reproduces all of them — 1 cm resolvable to 34.0 m at 20/20 and 54.0 m at
the 94-ppd ceiling, an 18 cm face to 619 m, contrast transmission 82/68/46% at 0.5/1/2 km in
V = 10 km, own horizon 5.03 km at 1.7 m eye height, a 1000 m massif visible to 127 km.

Three corrections are pinned as tests so they cannot drift back:

| the error | the correction | what it costs |
|---|---|---|
| 20/20 = Campbell & Green's 120 ppd | 20/20 IS 1′ MAR = 30 c/deg = **60 ppd** | a **2×** budget error |
| pixel angle = deg/px | the **centre pixel** subtends 10.3% more | under-selects LOD exactly where the player looks |
| V = 3.912/σ is "the" visibility law | WMO prints **3/σ** and 3.912 appears nowhere in WMO-No. 8 | 30% in the fog rate |

**The fog** was `density = 0.0030 * (0.80 + 0.20*exp2(-y*0.012))` with a falloff *squared* in
distance: a bare constant, a Gaussian where Koschmieder's law is an exponential, and an unexplained
1/58 m height e-fold that is nothing like an atmosphere. It is now σ derived from an authored
`--visibility` in metres and the **barometric** `exp(-y/H)` with H = 8500 m (which varies σ by 1.3%
over this world's relief — physically right, practically negligible, and correct in advance if the
world ever gets mountains).

The Check demanded pixel-sampled verification rather than formula-reading, and **the first attempt
at it was wrong in an instructive way**: solving a linear interpolation in tonemapped sRGB 8-bit
space read T = 0.603 against a predicted 0.679. Re-measured with `--no-tonemap --no-bloom`,
sRGB-linearised, looking straight down from 1000 m so the ray length is controlled:

| V | T measured | implied ray distance |
|---|---|---|
| 5 km | 0.4102 | 1139.0 m |
| 10 km | 0.6301 | 1180.6 m |
| 20 km | 0.7898 | 1206.2 m |
| 40 km | 0.8884 | 1210.4 m |

The four agree on one distance to within 6%. That is the **distance-independent** form of the check
and the stronger one: a wrong exponent or rate constant would make the implied distance a function
of V. My assumed 990 m was wrong; the shader was not.

---

## 2. The LOD radius was always an angle (goal 328)

`target(d) = max(finest, d * finest / lod_radius)` makes `target(d)/d` constant for every d beyond
the radius. That constant is an angular voxel size, so `lod_radius` was **never a distance** — it was
the denominator of an angle in the least legible possible units.

The shipped 4.0 m at a 7.8 mm finest voxel is **6.71 arcmin**, 6.7× coarser than 20/20 resolves.
Stated, not changed. And the measured answer to "why not just use the eye's limit":

| quality | radius | bricks | MB | build |
|---|---|---|---|---|
| 6.71′ | 4.00 m | 219,703 | 69.0 | 0.98 s |
| 4.50′ | 5.97 m | 449,958 | 143.2 | 2.38 s |
| 3.00′ | 8.95 m | 1,233,952 | 406.8 | 7.83 s |
| 1.00′ | 26.85 m | *did not complete in 900 frames* | | |

A 2.24× radius costs **5.6× the bricks** — an exponent of 2.1, between the area and volume laws,
which is what a surface embedded in a growing volume gives. At 3 arcmin the world needs 406.8 MB
against Prompt 004's 279.5 MB resident result. **The eye's own limit is roughly an order of
magnitude outside the budget.**

---

## 3. The region grew 256× in area for 2.27× the bricks (goal 329)

V = root_size_log2 − voxel_size_log2, against `kMaxVoxelBits = 24`:

| root_log2 | edge | V | headroom |
|---|---|---|---|
| 9 | 512 m | 16 | 8 ← was |
| 12 | 4096 m | 19 | 5 ← shipped |
| 14 | 16384 m | 21 | 3 |

**V was never the binding constraint.** Cost was:

| region | view | bricks | MB | build | GPU ms | fps |
|---|---|---|---|---|---|---|
| 512 m | 256 m | 219,344 | 68.9 | 0.97 s | 2.69 | 165 |
| 4096 m | 2048 m | 445,038 | 138.0 | 2.12 s | 4.09 | 165 ← shipped |
| 8192 m | 4096 m | 498,334 | 154.6 | 2.79 s | 5.55 | 107 |

**256× the area for 2.27× the bricks**, because everything a larger region adds is at coarse LOD.
That is the exact inverse of goal 328's finding: growing the REGION is nearly free, growing the LOD
RADIUS is superlinear.

**The Check's negative, reported rather than hidden.** It asked for a capture showing the region's
far edge is beyond where fog has taken the image to sky. **It is not.** At clear-air V = 20 km,
transmission at the 2048 m boundary is 0.670 — a visible line. Hiding it needs V = 2670 m, i.e.
thick haze, which throws away goal 327's whole point. Clear air and a 2 km region are incompatible;
the far silhouette tier is what the boundary actually needs.

**Goal 330 then found a real bug goal 329 introduced**: the region reaches 3547 m to a corner against
a 2000 m far plane, and although the marcher has no far clip, the depth it writes comes from the
projection. Raising the far plane to 4096 m **improves** precision at every distance that matters
(one float32 ULP at 1000 m: 65.4 cm → 40.4 cm), which is the opposite of the usual intuition — in a
standard [0,1] projection the NEAR plane dominates the distribution.

---

## 4. Foveation: a real 23.8% saving, shipped off (goal 331)

**A conceptual error the capture caught and no number would have.** The first version wrote
`lodAngle *= 1 + m*e`, treating Guenter's tolerated MAR as a multiplier. But that model is stated
against a 1-arcmin fovea and this renderer's foveal LOD is already 6.71 arcmin — so it compounded a
53× factor at the screen edge onto an already-coarse baseline. GPU fell 5.35 → 0.20 ms, a 27×
"saving", and the image became metre-wide blocks edge to edge. The **ratio** form — coarsen only
where the eye tolerates more than the renderer already delivers — is what foveation means.

At Guenter's 1.32 arcmin/degree with Hsu's 7.5° inner radius: **5.33 → 4.06 ms, 23.8% saved**, which
clears the prompt's 10% bar comfortably. **And the periphery is still visibly degraded** in a still
image without hunting for it. So it ships off. This is not "the saving was too small"; it is "the
saving is real and the cost is visible", and on a desktop the cost is paid wherever the player
happens to be looking.

---

## 5. Sway: the branch is the coordinate, not the segment (goal 335)

The first version gave every SEGMENT its own oscillator, with the finite-difference beam stiffness
`EI/ds` and the inertia above it. **The pole rang at 1.03 Hz against a predicted 0.26.**

The reason is the whole design. A serial chain is `M θ̈ + K θ = Q` with K diagonal but **M dense** —
joints i and j share every mass above both of them. Dropping the off-diagonals leaves each joint
ringing at `√(k_i/J_i)`, and for the root of a 40-segment pole that is 3× the collective mode. The
coupling is not a refinement; it is the mode. And the fix is not a 300×300 solve per tree per frame,
it is **fewer, better coordinates**.

So chains, each following the thickest child, with segment j taking a fixed share `w_j ∝ (L−s_j)·ds_j`
of its chain's rotation — the static tip-load curvature. That makes the generalized quantities come
out right *analytically*: `K/lever² = 3EI/L³` exactly, and `ω² = 12 EI/(ρAL⁴)` against Rayleigh's
12.727 — 2.9% apart, from two independent shape functions.

| quantity | measured | the research's |
|---|---|---|
| closed form, 20 m sycamore at 25.2 cm dbh | **0.260064 Hz** | 0.26 Hz (§3.1's FE sycamore) |
| leaf-off frequency shift | **18.4964%** | 18–19% (§3.4) |
| log-decrement recovers ζ = 0.039 | 0.039009 | — (an instrument check) |
| branch reaction on vs off | ζ_eff **0.0861 → 0.0901**, f₀ 0.914 → 0.831 Hz | §3.3's damping by branching |
| frame cost, 129 trees / 3,650 chains, 120 m ring | **0.049 ms mean, 0.246 worst** | budget 0.5 ms |
| the same at a 400 m ring, 1,552 trees | **0.585 ms mean, 2.933 worst** | over budget |

Cost is linear in stepped chains (12× chains, 12× cost), so the ring could reach ~370 m before the
budget binds. **120 m is not a taste, it is that number.**

**Two things only a picture found:**

1. **A horizontal branch could not move sideways.** Rotation vectors were projected onto the
   horizontal plane, reasoning that a trunk is vertical so its torsion axis is Y — true of a trunk,
   false of everything else. A horizontal branch broadside to the wind has a purely *vertical*
   bending torque and the projection deleted all of it. The correct constraint projects out the
   component along the chain's OWN axis.
2. **The wind had a hole exactly where trees resonate.** A ~0.05 Hz gust term, a 4 Hz flutter term,
   and nothing between — and tree fundamentals are 0.26–1.0 Hz. The tree took its static lean and
   drifted. Fixed in `world/wind` (one field is the rule) as `wind_buffet`: three incommensurate
   waves at 0.17/0.59/1.87 Hz with amplitudes falling with rate, scaled by a stated 0.20 turbulence
   intensity. Mean crossings in 60 s went **6 → 28**.

---

## 6. Skeleton voxelization, and a world the CPU reference had never seen (goal 336)

| | implicit | skeleton (128 m) | delta |
|---|---|---|---|
| bricks | 1,061,035 | 1,063,288 | +0.21% |
| tree memory | 350.3 MB | 350.9 MB | **+0.6 MB (+0.17%)** |
| build wall | 10.09 / 9.85 s | 10.54 / 10.88 s | **+8%** |
| skeletons | — | 183 trees, 29,465 primitives, 1.84 MB, 38 ms | |

Against Prompt 004's resident-cache budget (279.46 MB resident, 333.14 MB peak GPU), **0.6 MB is
0.21% of it**. The research names vegetation as the worst-case SVO content class; on this world it is
not, because a crown of overlapping leaf balls has less surface than a smooth octahedron of the same
volume and the branches it adds are thin.

**The build cost was +37% and is now +8%**, from two fixes found by measuring. `material_at` is the
hottest function in the voxelizer — once per candidate voxel, at 7.8 mm, over a canopy — and it was
calling a gather that ran `std::sort` to deduplicate; a POINT lands in exactly one cell and has
nothing to deduplicate. Then the grid's cells were sized to the LARGEST primitive, which in a crown
is a metre-wide leaf cloud, so every cell was two metres across and held a dozen clouds that every
voxel distance-tested. Mean-sized cells fixed that.

**The canopy took three tries and a picture each time**: one cloud per tip at a fixed leaf-area
density of 2.0 m²/m³ (a dozen separate spheres on a stick); two clouds per segment at half the volume
with a jitter (better, still spheres); and finally the density **derived** as
`total leaf area / crown volume`. 45 m² of leaf over a 3 m crown is really 0.4 m²/m³, so the fixed
2.0 had been packing the whole crown's leaf into a fifth of its volume. That was the bug.

### And the thing this goal actually found

**`tools/svo_render` and the harness's `pose_ground` resolver were working in the pre-Prompt-006
NOISE world.** The macro-field bake lived in an anonymous namespace inside `app/src/svo_world.cpp`,
so only the app had it; every other caller constructed a bare `HeightmapGenerator(seed)` and got a
different planet. Measured by `test_playable_field.cpp`: over a 320 m square the two surfaces differ
by a **mean of 35.5 m and a worst of 107.1 m**.

That means the CPU reference renderer — the thing this project's own rules say to reproduce an
artefact on *before* touching a shader — had been a reference for nothing since Prompt 006, and every
`pose_ground` in the scenario library had been resolving the camera against ground that is not there.
The collision counter had been reporting it all along as "ticks ended INSIDE solid"; nobody connected
the two. It was found by putting an `svo_render` frame and a harness capture of the same coordinates
side by side and seeing two unrelated landscapes.

Two consequences, both pre-existing and both now visible: `dev/scenarios/macro_tree.scn` —
"camera two metres from a trunk" — has **no tree within 38 m of its pose**; and `valley_far`'s gates
move because its pose now resolves correctly (moire_ratio 2.304 → **1.736, which passes** the gate it
had been failing; gpu_ms_median 3.88 → 5.30 against a 3.6 limit, which fails harder).

---

## 7. The canopy warp: attempted, measured, rejected (goal 337)

A crown reads as moving because its **edge** moves. The edge is where traversal stopped, and this
goal excludes warping traversal. So the warp can only move a texture inside a bit-identical
silhouette — 17.9% of pixels differ at 2 m and 28.2% at 10 m, and none of it says the tree is in
motion. **TAA ghosting: none**, because the warp changes shading without changing motion vectors, so
TAA absorbs it — the good half of the same fact that makes it invisible. Ships present and off.

**Two real finds on the way.** The first version gated on `wind_responsive`, and ground grass IS
wind-responsive (goal 334 gave it the shimmer deliberately) — a 6 cm domain warp on a lawn reads as
the ground crawling, and it was the *dominant* change in the frame-to-frame difference image. And
**the animation clock was the wall clock**, so no wind-driven A/B was reproducible: three runs of one
scenario capturing at the same scripted second measured canopy motion at 0.122 / 0.255 / 0.192% — a
spread three times the effect. `fixed_anim_step` fixes it for scripted runs, with the honest limit
that a capture at a scripted TIME still lands on a different frame index, so a reproducible
wind-driven capture needs `capture frame N` as well.

---

## 8. Ground cover, and the ninth material (goals 338–340)

**This reopens goal 40** — "grass ground-cover geometry, needs instancing+textures" — and the new
evidence is `research/grass-rendering-research.md`, which ranks "grass AS voxels in the SVO, finest
levels only" as this engine's #2 option needing no new pipeline at all. Its two stated costs are
accepted rather than argued away: vegetation is the worst-case SVO content class, and there is no
published way to animate stored ray-marched voxels, so the voxel blades **do not move**.

### The palette, which was the real work

A blade must be `Phase::Foliage` where ground grass is `Phase::Solid`, so it needed a NINTH material
— and the brick palette held exactly eight, with a static_assert naming two ways out:

* **a wider index** — 512 four-bit indices is 64 words against 52 and the palette doubles: 84 words,
  336 B against 280, i.e. **+20% on every brick in the world**, most of goal 275's measured
  −264.2 MB handed back for one material;
* **a second size class** — `BrickPool` is a fixed-size slot allocator and the whole resident cache
  is built on it.

The third way, which is what shipped: keep 3 bits and 280 B and make the invariant **per brick**,
backed by the measurement already in the file — 98.6% of bricks hold three or fewer distinct
materials and nothing exceeds five, against eight slots. Overflow has a defined answer (the
material's `palette_fallback`, then entry 1 — mis-shaded, never a hole) and a counter that a test
asserts is zero on real content and non-zero on a constructed nine-material brick.

Two existing tests had encoded the old guarantee and both were **wrong** rather than stale:
`tree_replaces` was asserted against a hand-written restatement that named the yielding materials by
identity, so a ninth that also yields made it disagree with the registry.

### A tuft is not a plant

Part 6 §7 measures ground cover in plants/m²: 10–100 for natural dry grassland, 10–18 for semi-arid
rangeland, ~2 for Namib tussock, and 940–2,836 **tillers** (not plants) for a managed sward. At 60
plants/m² a 24 m ring holds 108,600 of them. So a TUFT stands for `plants_per_tuft` real plants, and
that is named as the LOD device it is.

| plants/tuft | tufts/m² | bricks | MB | build |
|---|---|---|---|---|
| (off) | — | 779,688 | 279.0 | 6.46 s |
| **20** | 3.0 | 791,695 (+1.54%) | 282.6 (+1.29%) | 6.76 s (+4.6%) ← ships |
| 6 | 10.0 | 819,307 (+5.08%) | 290.9 (+4.27%) | 7.42 s (+14.9%) |
| 2 | 30.0 | 889,358 (+14.06%) | 311.7 (+11.72%) | 7.67 s (+18.7%) |

Against the Check's < +15% build and < +10% MB: 20 ships comfortably inside, 6 is inside at the very
edge of the build limit, 2 exceeds the memory limit and is the documented ceiling.

**The honest look note**: at an affordable density this reads as scattered dark tussocks, not a
sward. That is what 10 plants/m² *is* — the research is explicit that natural rangeland is two to
three orders of magnitude sparser than a lawn — and the lawn look is what the overlay exists for.

**Walked through it**: 420 ticks sprinting, 0 frames below the ground surface, collision
**0.0359 ms/tick** against a 0.20 ms budget.

### The overlay, and a marcher that writes no depth

The research's recommended technique assumes a depth buffer. **Goal 267 removed the march's SV_Depth
export** after measuring it at 17% of the march on vk and 10% on d3d12. The engine already exports
what is needed under another name: the march's second render target is the hit distance, written for
the temporal resolve. A blade compares its own view distance against that texture and discards where
the world is nearer — depth composition with no depth buffer. Grass-against-grass still needs real
depth and gets it from the otherwise-untouched DSV.

| | march | grass | whole frame |
|---|---|---|---|
| vk off / on | 6.80 | **0.21** | 6.92 → 7.09 |
| d3d12 off / on | 9.99 | **0.05** | 10.10 → 10.22 |

Against Ghost of Tsushima's ~83,000 blades in **2.5 ms** at **16 floats (64 B)** per blade: this is
**32 B/blade** and **0.0092 µs/blade on vk** against GoT's 0.030 — 3.3× cheaper per blade on vk and
14× on d3d12. Not a cleverness claim: a desktop GPU a decade newer, a 6-triangle blade against GoT's
15-vertex one, and no compute culling because the ring is small enough not to need it.

Wind on vs off moves **49.7% of pixels**; the player bend moves **1.79%** looking down (0.30% at a
level pose, because a 1.5 m radius from a 1.7 m eye is a small patch at the bottom edge); **TAA does
not ghost**, because the overlay writes its own distance so the resolve reprojects a blade against
the blade's distance.

**It ships off, and not for the GPU cost.** A full instance rebuild is **10 ms worst, 0.148 ms/frame
mean**, once per quarter-radius of camera movement — a dropped frame every few seconds at walking
pace. The rebuild is single-threaded, synchronous and inside the frame, and the fix is a goal.

**And a bug worth recording**: the first version drew nothing. `mul(v, M)` compiles fine and silently
transposes (this project uses `mul(M, v)`), and then, while bisecting *that* with a debug constant
colour, the pixel cbuffer went unreferenced, the compiler stripped it, and `GetStaticVariableByName`
threw at PSO setup. Two independent invisibilities stacked.

### The tiers agree because they call the same function

`grass_blade(tuft, index, count, params)` is the ONE place a blade's geometry exists, and both tiers
call it. The first version of the overlay re-derived the fan from the tuft's id with its own copy of
the turn angle — which is exactly how a ring appears six months later.

**The third tier does not have tufts**, and saying so is more useful than pretending it does. The
distance shimmer is a per-pixel brightness term with no per-tuft identity; what it shares is the wind
field. So the test asserts identity across the two tiers that have identities (1,000 tufts, 5,000
blades, endpoints IDENTICAL, 48,004 assertions) and phase across all three.

**No ring, measured rather than eyeballed.** The overlay ends at 14 m and the voxel tier at 24 m; at
the capture's pose those fall at image rows 335 and 301. The per-row fraction of blade-dark pixels
ramps 0.247 → 0.338 with a largest smoothed step of 0.0103 **at row 634** — a hill crest, 4.1× the
mean step and three hundred rows from either boundary.

---

## 9. Captures

| file | what it shows |
|---|---|
| `an_sway_period.png` | four frames across one sway period at 14 m/s, rest pose in grey behind |
| `an_sway_breeze_4ms.png` | the default breeze: a 1.39%-of-height lean, which is what calibrated `drag_pressure` |
| `an_skeleton_voxels_vk.png` | 2 / 10 / 60 m, implicit left, grown skeleton right |
| `an_skeleton_voxels_d3d12.png` | the same on d3d12 — indistinguishable |
| `an_canopy_warp.png` | goal 337's rejected experiment: identical silhouettes |
| `an_voxel_grass_vk.png` | 1 / 4 / 15 m, off left, voxel blades right |
| `an_grass_overlay_vk.png` | the overlay's density, the player bend, and a TAA pan |
| `an_grass_tier_boundary.png` | both tier rings in one frame, no ring |

---

## 10. The budget, spent and accounted (goal 341)

Measured at `stress_pose` — the pose CLAUDE.md records as the one a GPU budget is set from — by
turning this prompt's default-on features off one at a time. Buckets are
`release-codegen-and-tradeoffs.md` §1's.

| variant | bricks | vk march | d3d12 march |
|---|---|---|---|
| shipped default | 845,260 | 5.83 | 8.77 |
| − voxel grass | 841,994 | 5.80 | 8.55 |
| − skeleton voxelization too | 844,486 | 5.69 | 8.37 |
| − sway, and the region back to 512 m | 772,906 | **4.69** | **6.92** |

| feature | goal | bucket | CPU ms/frame | GPU ms vk | GPU ms d3d12 | MB | default |
|---|---|---|---|---|---|---|---|
| perceptual header | 326 | free | 0 | 0 | 0 | 0 | — |
| derived fog | 327 | free | 0 | ~0 | ~0 | 0 | on |
| LOD stated in arcmin | 328 | free | 0 | 0 | 0 | 0 | on |
| **4 km region** | 329 | pay-to-win | 0 | **+1.00** | **+1.45** | **+69** | on |
| far plane 4096 m | 330 | free | 0 | 0 | 0 | 0 | on |
| foveation | 331 | *saving* | 0 | **−1.27 (23.8%)** | — | 0 | **off** — visible |
| fades in arcmin | 332 | free | 0 | 0 | 0 | 0 | on |
| grass shimmer | 333/334 | free | 0 | ~0 | ~0 | 0 | on |
| tree sway | 335 | pay-to-win | **0.049** | 0 | 0 | ~0 | on |
| skeleton voxelization | 336 | pay-to-win | 0 (build +8%) | +0.11 | +0.18 | +0.6 | on |
| canopy warp | 337 | — | 0 | ~0 | ~0 | 0 | **off** — rejected |
| voxel ground cover | 338 | pay-to-win | 0 (build +4.6%) | +0.03 | +0.22 | +3.6 | on |
| raster overlay | 339 | pay-to-win | 0.148 (**10 worst**) | +0.21 | +0.05 | +0.73 | **off** — rebuild hitch |
| tier sharing | 340 | free | 0 | 0 | 0 | 0 | on |

**The region is the spend.** Everything else this prompt turned on is at or below the 18% run-to-run
noise floor stress_pose's own comments record for this machine. That was not the expectation going
in — trees that move and ground that is covered *sound* expensive, and the 4 km region sounded free
because goal 329 measured it at 165 fps.

### Against the target

| | whole GPU frame | fps equivalent | 150 fps target |
|---|---|---|---|
| vk, shipped default | **6.32 ms** | 158 | **inside** |
| d3d12, shipped default | 9.19 ms | 109 | outside |
| d3d12, every one of this prompt's features off AND the old 512 m region | 7.35 ms | 136 | **still outside** |

So: **vk meets the target and d3d12 does not — and d3d12 did not before this prompt either.** Goal
244's own measurement, which set the target, reads "`stress_pose`, vk". No feature comes off the
default on account of d3d12, because taking every one of them off does not bring d3d12 to target;
the gap is the backend's, and it is a goal rather than a knob.

A note on reading the frame numbers rather than the GPU ones: **vsync is on by default and this panel
is 165 Hz**, so a frame whose GPU work exceeds 6.06 ms lands on the next interval at 12.1 ms. Every
frame median at these poses is 12.02–12.10 ms for exactly that reason, and `--no-vsync` is not the
answer either — with it, the run hits the harness's frame cap before the script finishes. The GPU
pass timers are the honest signal, which is also why Prompt 004's own gates are on `gpu_ms_median`.

---

## 11. The hilltop shot, and what is not in it (goal 342)

`research/captures/an_hilltop_vk.png` and `an_hilltop_d3d12.png`: the shipped default, from high
ground at (48, 0), 12 m up, looking down the valley. Prompt 006's terrain under it, goal 338's ground
cover on it, goal 335's trees on the ridge, goal 327's fog taking the far end.

**Backend agreement, and a measurement that nearly reported the opposite.** vk against d3d12 at this
pose reads **34.0% of pixels differing** — three times Prompt 002 goal 217's 12.8% floor. With
`--no-wind` the same pair reads **1.375%**. The whole difference was the wind's phase: the two
backends reach a scripted capture at different frame counts, and `fixed_anim_step` ties the clock to
frame count, so it fixes reproducibility *within* a backend and not *across* two. The honest number is
1.375%, and the way to get it is stated rather than assumed.

**What is not in the frame, reported rather than quietly dropped: no river.** The Check asks for one.
Prompt 006's own `docs/terrain-pipeline.md` §6 already records sub-cell rivers as a limitation of this
world at a 16 m field resolution — the channels are carved but they are narrower than a cell, so at a
hilltop's viewing distance there is nothing to see. That is a property of the terrain this prompt
inherited, not of anything this prompt did, and the fix belongs to whichever pass widens the field or
adds a river-water surface.

---

## 12. The goldens, and what a world with wind in it costs a capture (goal 344)

**Every static golden was stale** — the region went 512 m to 4 km, the terrain's pose resolver was
fixed, and trees and grass changed. Re-taken on both backends: spawn_stand 62.4%, macro_ground
89.6%, stress_pose 58.2%, macro_tree 43.2% of pixels changed against their old references.

**And then two of them still failed, for a reason this prompt created.** Goals 333, 335 and 338 put
moving content in every frame, driven by a clock that is the wall clock by default. A golden of a
scene where "nothing moves" measured **9.27% of pixels changed against a 1.5% gate**.

`--anim-step` is necessary and NOT sufficient: it makes the clock deterministic per FRAME, and
`capture end` lands on a different frame index between runs. So the capture scenarios take the fix
the project already uses for every wind A/B — `--no-wind`, which goal 332's own 0.0012% number was
measured with. The golden then measures everything else: terrain, tree shape, grass geometry,
shading, LOD. **spawn_stand `final` and stress_pose now read 0.003/255 and 0.000/255, 0.0000% of
pixels.**

One capture lost its golden rather than being loosened: `spawn_stand`'s `first_light` fires on the
world-ready EVENT, whose frame index moves because the build is threaded. Re-taken and wind-free it
still read **1.652% against a 1.5% gate at a mean distance of 0.502/255** — everywhere-slightly
rather than somewhere-badly. Prompt 003's rule applies: a capture that is not reproducible does not
get a golden, it gets a note.

**And the fix had to reach four more scenarios, which is how the second irreproducible capture was
found.** `valley_far`, `macro_ground`, `macro_tree` and `fly_orbit` also promote goldens, and they
were re-taken BEFORE `--no-wind` was added to them. `valley_far` then failed standalone at **2.824%
of pixels and a mean of 2.253/255, against gates of 1.500% and 1.200** -- the largest residual in the
library. With `--no-wind` and a re-take it reads **0.0000% at a mean of 0.004/255 on vk and 0.024 on
d3d12**.

The final state, ten golden comparisons over six scenarios on both backends:

| scenario | vk mean /255 | d3d12 mean /255 | pixels changed |
|---|---|---|---|
| spawn_stand `final` | 0.000 | 0.039 | 0.0000% |
| stress_pose | 0.000 | 0.013 | 0.0000% |
| valley_far | 0.004 | 0.024 | 0.0000% |
| macro_ground | 0.000 | 0.000 | 0.0000% |
| macro_tree | 0.000 | 0.007 | 0.0000% |

**Every one of the sixteen scenario runs PASSES on both backends.** Not one golden is loosened; the
gate stayed at 1.5% throughout and the measurements moved to meet it.

**`fly_orbit`'s `quarter` was the second capture to lose its golden, and it lost it to a cause
`--no-wind` cannot fix.** Re-taken wind-free it still promoted at **53.707% of pixels changed on vk**
against 2.719% on d3d12 -- worse than the 5.3-35.5% band Prompt 003 measured for post-motion
captures. It fires at frame 120, a third of the way through an orbit, with the body at speed, and
the scenario's OWN `GOLDEN POLICY` block already said such a capture gets no golden; `capture end
final` honoured that and `capture frame 120 quarter` had been missed. The dominant term there is the
rebuild storm, not the wind. So it follows `final`: `no-golden`, and its two reference PNGs are
deleted rather than replaced. **A capture taken at rest needed `--no-wind`; a capture taken at speed
cannot be made reproducible at all today, and the honest response to that is still to delete the
reference rather than widen the gate.**

## 12b. Voxel grass costs 4.9x the collision time AT ONE KIND OF POSE, and a log line found it

Not on the task list, and not something any gate would have caught: `macro_tree` logs

```
[ERROR] collision: 0.3577 ms mean per tick over 121 ticks, worst 1.2480 ms (budget 0.20 ms)
        -- 8.1 queries/tick, 151.9 nodes/query
```

against a documented pre-Prompt-007 land range of 0.026-0.116 ms/tick. **No scenario asserts on
collision cost** -- `app_run.cpp` logs it at Error level and nothing gates it -- so this would have
shipped as a number nobody read.

`nodes/query` reads **151.9 on both backends, identically**, which places the cause in the octree's
contents rather than in timing noise. Two candidates, both measured with the harness's own `--ramp`
so the arms share a build and a pose:

| arm | collision ms/tick | nodes/query |
|---|---|---|
| shipped default | 0.3465 - 0.4299 | 151.9 |
| `--skeleton-radius 0` (goal 336 off) | 0.3485 - 0.3546 | **151.9, unchanged** |
| `--grass-radius 0` (goal 338 off) | **0.0688 - 0.0791** | **85.9** |

**Goal 338 is the whole of it, and the first guess -- skeleton voxelization -- was wrong.** The
mechanism is the one Prompt 003 already wrote down for deep water: `overlaps_solid` early-outs on the
first SOLID voxel, and `GrassBlade` is `Phase::Foliage` on purpose, so a 24 m ring of
occupied-but-not-solid voxels around the body turns every query into a descent that finds nothing and
runs to exhaustion. Turning grass off puts the cost back inside the 0.20 ms budget.

**And then the obvious generalisation turned out to be false.** Three walking and flying scenarios
measure collision INSIDE the budget on the shipped default -- `clip_stress` 0.0562, `walk_cliff`
0.0392, `fly_transect` 0.0233 ms/tick, at 26.7 / 34.3 / 36.2 nodes/query -- all of them below even
`macro_tree`'s grass-OFF figure of 85.9. So the same A/B was run at `clip_stress`:

| `clip_stress`, walking | collision ms/tick | nodes/query |
|---|---|---|
| shipped default | 0.0367, 0.0378 | 26.7 |
| `--grass-radius 0` | 0.0317, 0.0334 | 25.7 |

**Grass costs 3.7% of the node visits there, against 77% at `macro_tree`.** The regression is real and
it is LOCAL: it needs a body standing still in dense ground cover, which is what `macro_tree`'s pose
is and what a walking transect mostly is not. `macro_tree`'s grass-free baseline is itself 3.3x
`clip_stress`'s, so that pose is intrinsically the deeper one before grass is added at all. Reporting
this as "voxel grass costs 4.9x collision" would have been wrong in the direction that matters --
it would have justified a fix nobody needed on the poses people actually walk.

**One number in the macro_tree table does not follow from the other, and it is worth keeping.** Node
visits rise 1.77x while time rises 4.9x. Whatever those extra visits are, they are individually more
expensive than the ones already being made -- deeper descents, or worse locality from bricks that a
solid-only query never used to touch. That is a measurement, not an explanation, and the fix belongs
to whoever writes the goal: a query that can ask "is anything in this subtree SOLID" of a node
summary would skip a foliage-only brick whole, which is the same shape of answer
`caves_possible_in_band` already is for caves.

Filed as an open goal rather than fixed here: it is a collision-system change, this prompt's subject
is the view, and the honest report of a 4.9x regression is worth more than a rushed fix to it.

## 13. What this pass deliberately did not do (goal 345)

| not done | why |
|---|---|
| the far silhouette tier | the region's 2 km boundary IS visible at clear air; hiding it needs V = 2.67 km, which throws away goal 327 |
| foveation on by default | 23.8% is real and so is the visible peripheral degradation; on a desktop the eyes roam |
| geometric canopy motion | a silhouette cannot move without warping traversal, which the goal excluded |
| the raster overlay on by default | the 10 ms synchronous instance rebuild, not the 0.21 ms of GPU |
| a threaded grass-instance rebuild | the fix for the above, and it is a goal rather than a tweak |
| moving voxel grass | no published way to animate stored ray-marched voxels, and this pass did not invent one |
| rivers you can see | Prompt 006's channels are sub-cell at a 16 m field — inherited, not caused |
| d3d12 at the fps target | it was outside before this prompt, and removing every feature does not bring it in |
| reversed-Z | goal 330 assessed it: nothing z-fights, so the benefit is unclaimable until something distant needs composing |
