# View distance, LOD and living cover

The operating manual for everything Prompt 007 added: how far this renderer sees, why, and what is
moving out there. The decisions and the measurements are in
`research/view-distance-and-cover-log.md`; this is what a future pass needs to *use* it.

---

## 1. Every distance comes from `render/lod/perceptual.hpp`

There is exactly one place a distance criterion is written down, and it cites its source beside each
constant. It is header-only, depends on nothing, and is registered **outside** the
`VOXEL_BUILD_RENDERER` guard, because `world/svo`'s LOD ladder needs it in the no-GPU CI job.

| question | function | the answer this world uses |
|---|---|---|
| how small before it stops mattering? | `resolvable_distance_m(size, mar)` | 1 arcmin MAR = **60 ppd**, NOT 120 |
| how far can I see through air? | `extinction_from_visibility(V, convention)` | Koschmieder, C_t = 0.02, k = 3.912 |
| how much contrast survives? | `contrast_transmission(d, V)` | `C(d) = C₀ e^(−σd)` |
| where is the horizon? | `visible_distance_km(eye, target)` | `D = c(√h + √H)`, c = 3.856 refracted |
| how big is a pixel? | `centre_pixel_angle_rad(fov, width)` | the CENTRE pixel, 10.3% more than deg/px |

Three errors are pinned as tests because they are easy to make and expensive:

* **20/20 is 60 ppd.** Campbell & Green's 60 c/deg = 120 ppd is a different quantity. Conflating them
  moves a budget by 2×.
* **The centre pixel is not the average pixel.** Using `fov/width` under-selects the octree level
  exactly at screen centre, which is where the player is looking.
* **WMO's visibility law is 3/σ, not 3.912/σ.** Both are here, each named after its authority; the
  ratio is 1.3059 and mixing them is a 30% error in the fog rate.

## 2. The knobs, and what they mean

| flag | default | what it is |
|---|---|---|
| `--visibility M` | 20000 | meteorological visibility in metres. **σ is derived from it**, not authored. |
| `--lod-arcmin A` | 0 (use the radius) | the LOD target as an ANGLE. `--lod-radius 4` IS 6.71 arcmin. |
| `--lod-radius M` | 4 | the faithful alias; a zero `--lod-arcmin` recomputes nothing, so the builder gets the same float it always did. |
| `--region-log2 N` | 12 (4096 m) | the octree root edge. V = N − voxel_size_log2 must stay under 24. |
| `--foveation S` | 0 (off) | fixed crosshair foveation, arcmin per degree of eccentricity. |
| `--skeleton-radius M` | 128 | trees within M metres voxelize from their grown skeleton. |
| `--grass-radius M` | 24 | metres of voxel ground cover. |
| `--grass-tuft-plants N` | 20 | real plants one voxel tuft stands for. Lower is denser. |
| `--grass-overlay` | off | the instanced raster blade overlay. |
| `--grass-overlay-radius M` | 14 | its ring. |
| `--grass-overlay-density F` | 8 | how much denser the raster ring is than the voxel tier. |
| `--grass-bend-radius M` | 0.6 | metres within which the body pushes blades aside. |
| `--sway` / `--sway-radius M` | on / 120 | the hierarchical spring sway and its ring. |
| `--canopy-warp M` | 0 (off) | goal 337's rejected shading-domain warp. |
| `--anim-step S` | 0 (wall clock) | seconds the wind/wave clock advances per rendered frame. |

## 3. The two cost curves that point in opposite directions

This is the single most useful thing to know before spending anything here.

**Growing the REGION is nearly free.** 256× the area costs 2.27× the bricks, because everything a
larger region adds is at coarse LOD. 512 m → 4096 m took the march from 2.69 to 4.09 ms.

**Growing the LOD RADIUS is superlinear.** A 2.24× radius costs 5.6× the bricks — an exponent of 2.1,
between the area and volume laws, which is what a surface embedded in a growing volume gives. The
eye's own 1-arcmin limit would need a 26.85 m full-resolution sphere and is roughly an order of
magnitude outside the budget.

A test asserts that exponent, so a change that appears to make fine LOD cheap is understood to be
structural rather than lucky.

## 4. Ground cover has three tiers, and they share one function

| tier | what it is | ring | moves? |
|---|---|---|---|
| voxel blades | `GrassBlade` capsules in the octree | `--grass-radius`, 24 m | **no** — nothing published animates stored ray-marched voxels |
| raster overlay | one instanced draw, 32 B/blade | `--grass-overlay-radius`, 14 m | yes — wind and a player sphere-mask bend |
| shimmer | a per-pixel brightness term on the ground material | everywhere | yes, but it has **no tufts** |

`world::generation::grass_blade(tuft, index, count, params)` is the ONE place a blade's geometry
exists and both geometric tiers call it. `grass_wind_phase(tuft)` is the same for the phase. That is
what makes the tier boundary a density ramp rather than a ring: crossing it removes the *extra*
raster blades and leaves the shared ones standing as voxels, in the same place.

The third tier genuinely has no per-tuft identity, and the code says so rather than pretending. What
it shares is the wind field.

**Density comes from the biome**, not a constant — `world/generation/grass_cover.hpp`'s table is
`research/earth-terrain-geomorphology-research.md` Part 6 §7's plants/m² with each citation beside
it. **A tuft is not a plant**: `plants_per_tuft` is a stated level-of-detail device, because 60
plants/m² over a 24 m ring is 108,600 of them.

## 5. Trees move as branch chains

A grown skeleton is decomposed into chains — each following the thickest child, so the trunk chain
runs the tree's full height — and each chain is one damped oscillator hanging off the chain that
carries it. Per SEGMENT would be wrong and was: a serial chain's mass matrix is dense, so a diagonal
approximation rings at 4× the collective mode.

Within a chain, segment j takes a fixed share `w_j ∝ (L−s_j)·ds_j` of the rotation — the static
tip-load curvature — which makes `K/lever² = 3EI/L³` come out exactly and `ω²` land 2.9% from
Rayleigh's, from two independent shape functions.

Everything physical is a measurement rather than a taste:

* the fundamental is the cantilever formula, and a 20 m sycamore at 25.2 cm dbh gives **0.260 Hz**
  against `research/tree-motion-growth-and-appearance.md` §3.1's FE-simulated 0.26;
* `leaf_mass_fraction` IS §3.4's measured 18–19% bare-vs-leafy shift, re-expressed;
* damping is §3.4's 8.6% in leaf and 3.9% bare;
* the Vogel exponent is mapped across §4.2's measured −0.2 to −1.2 band by the species' own
  petiole-stiffness knob;
* `drag_pressure` is the ONE authored constant, and the file says so.

**`sway_joint_budget` is a perceptual LOD**, not a distance band: it drops the chains whose residual
contribution to tip motion falls under one minute of arc at that distance.

## 6. Three things worth knowing before touching any of this

**The marcher writes no depth.** Goal 267 removed `SV_Depth` after measuring it at 17% of the march
on vk. Anything that needs to compose against the marched world uses the **hit-distance render
target** instead — that is what the grass overlay does, and it needs its own copy of that target
because a pass cannot read and write one texture.

**The animation clock is the wall clock by default**, and that makes a golden of a scene where
"nothing moves" irreproducible. `--anim-step` ties the clock to the frame index and is necessary but
NOT sufficient, because `capture end` lands on a different frame between runs. **The fix a
golden-comparing capture actually needs is `option --no-wind`**, and every such scenario in
`dev/scenarios` now carries it with that reason written above it: `spawn_stand`'s `final` went from
9.27% of pixels changed to 0.0000%, and `valley_far` from 2.824% to 0.0000%. The hilltop shot's
backend comparison reads 34.0% with wind and **1.375% without**, and only the second number means
anything.

**Pinning the frame does not rescue a capture taken at SPEED.** `fly_orbit`'s `quarter` fires at
`capture frame 120`, a fixed index, mid-orbit — and wind-free it still differs by **53.707% of pixels
between runs on vk**, because at speed the body outruns the LOD rebuild and which trees are resident
decides what it sees. That is the rebuild storm, not the wind, and no flag here fixes it. Such a
capture gets `no-golden` and is written for eyeballing, which is what Prompt 003's rule already
said.

**Every tool bakes the macro field now**, via `world::generation::field::bake_playable_field`. It
used to live in the app alone, and `tools/svo_render` and the harness's `pose_ground` resolver were
therefore working in a world whose surface differs by a **mean of 35.5 m**. Any pose written before
2026-09-08 is suspect.

## 7. What this pass deliberately did not build

* **The far silhouette tier.** The region's 2 km boundary IS visible at clear-air visibility, and
  hiding it with fog would need V = 2.67 km, which throws away the derived-fog work. Stated as an
  open goal rather than papered over.
* **Foveation on by default.** The 23.8% saving is real and so is the visible peripheral
  degradation; on a desktop the player's eyes roam and a crosshair is not a gaze point.
* **Geometric canopy motion.** Attempted, measured, rejected — a silhouette cannot move without
  warping traversal, and this goal excluded that.
* **The raster overlay on by default.** Not the 0.21 ms of GPU; the 10 ms synchronous instance
  rebuild.
* **Moving voxel grass.** There is no published way to animate stored ray-marched voxels and this
  pass did not invent one.
* **Rivers you can see.** Prompt 006's channels are sub-cell at a 16 m field, so there is nothing to
  see at a hilltop's distance. Inherited, not caused.
