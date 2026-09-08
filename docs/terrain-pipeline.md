# The terrain pipeline

What generates the world, what each stage produces, which research section specifies it, where its
parameters came from, and how to run the validation suite.

This exists so a future pass does not have to re-read 3,700 lines of
`research/earth-terrain-geomorphology-research.md`. The decisions and the measurements that produced
them are in `research/earth-terrain-pipeline-log.md`; this is the operating manual.

---

## 1. The shape of it

    height_at(x, z)  =  macro(x, z)          Catmull-Rom from a baked 8 km field
                     +  detail(x, z)         analytic, 5 octaves, always present

**Both terms are always present.** A residual that is absent until something is baked makes the
world's shape depend on where the player has been, which is a determinism hazard — that is why goal
295 rejected the alternative in writing.

The macro field is **500 × 500 cells at 16 m = 8 km, 8.00 MB**, carrying eight planes:

| plane | written by | read by |
|---|---|---|
| `Elevation` | every stage | everything |
| `Precipitation` | `climate`, `climate_final` | biomes, discharge, dunes |
| `Temperature` | `climate` | biomes, the snowline |
| `FlowAccum` | `flow`, `biomes` | rivers, drainage density, wetlands |
| `Lithology` | `continents` | karst's carbonate mask |
| `Biome` | `biomes` | tree density, dunes |
| `IceSnow` | — | reserved |
| `Sediment` | — | reserved (goal 325: not a live system) |

**Why 8 km.** A_c = 0.1–5 km² against a 512 m playable region means the region holds 2.6
channel-head areas at best. The erosion has to run on something much wider and the region is a
window into it. Full arithmetic in the log §1.

**The world origin is moved onto land after baking** (`recentre_on_land`, goal 321). The playable
region is a 512 m window at world (0,0); the continent mask is a 4 km feature; so without this,
whether the player spawns on a hillside or in open ocean is up to the seed. It shifts the field's
origin and touches no elevation. The alternative — stamping a land bump — was tried and measured
worse: sampled land fraction went to 100% and the world lost its coastline.

---

## 2. The stages, in order

Run by `run_pipeline` (`world/generation/field/macro_pipeline.hpp`). `--field-stages N` stops after
N, which is how one stage's contribution is isolated for a capture.

| # | stage | research | produces | cost |
|---|---|---|---|---|
| 1 | `continents` | Part 7 §10.1, §10.2(1) | land/ocean crust, hypsometric curve, a stamped orogenic belt, lithology | 0.021 s |
| 2 | `climate` | §10.2(2) | precipitation, temperature | 0.006 s |
| 3 | `fill_depressions` | §11(1) | a drainable surface, zero internal basins | 0.018 s |
| 4 | `flow` | §11(2) | D8 receivers, contributing area | 0.020 s |
| 5 | `incise` | §11(3) | stream-power channels, modulated by stratigraphy | **0.365 s** |
| 6 | `diffuse` | §11(4) | hillslopes, the slope-area plateau | 0.044 s |
| 7 | `climate_final` | §10.2(2) | precipitation re-run on the eroded terrain | 0.009 s |
| 8 | `biomes` | §6.1, §6.4 | the biome plane | 0.050 s |

**0.545 s total.** The incision is 67% of it and is where any future budget goes.

**Climate runs twice on purpose.** The first pass must precede the incision because the incision
wants a rain field; that leaves the shipped precipitation keyed to pre-erosion topography, while
biomes and vegetation read it against the terrain they can see. Measured: the same background
fraction gives a 32.4:1 land wet/dry ratio pre-erosion and 16.1:1 post-. Part 7 notes one orographic
pass per erosion checkpoint is standard in coupled fastscape work.

### Post-passes (not stages — they consume the field)

- **Rivers** (`field/rivers.hpp`, goal 309): polylines with width, discharge and elevation.
  **Not a heightfield stage**, because the largest river on this world is 2.42 m wide against a 16 m
  cell — a sub-cell feature. `test_rivers.cpp` asserts `maxWidth < 16 m` so that premise fails loudly
  if it stops being true.
- **Stencils** (`field/stencils.hpp`, goals 310–314): glacial, coastal, karst, dunes. Each is a
  stamp validated by a statistic, per §10.1's "convincing costume" line.
- **Caves** (`world/svo/caves.hpp`, goal 313): the only thing here that is 3D. See §4.

---

## 3. Parameters and where they came from

Every constant in this pipeline names its source in a comment beside it. The ones worth knowing:

| parameter | value | source |
|---|---|---|
| stream-power `m`, `n` | 0.5, 1 | Part 2 §11 — θ = 0.5, top of the 0.45–0.5 band |
| erodibility `K` | 4e-5, × per-bed 0.55–1.9 | Part 2; the bed contrast is calibrated against the suite |
| **uplift** | **0** | §10.1: the SPIM's job is to CARVE stamped relief, not grow it. 60 m of uplift on 112 m of relief sent hypsometry back to Gaussian |
| channel threshold `A_c` | 0.01 km² | goal 307 — chosen from the density band, because the research's two bands do not overlap |
| lapse rate | 6.5 °C/km | §10.2(2), standard atmosphere |
| wring-out height | 133 m | water-vapour scale height 2 km ÷ this world's 15× vertical miniaturization |
| drift length | 250 m | Roe & Baker 5–25 km, scaled by drift/barrier-half-width, not by field span |
| background precipitation | 0.20 | swept; §6.2's ~10:1 wet/dry anchor selects it |
| bankfull discharge | `Q = 0.087 A^1.044` | Petit & Pauquet 1997, 4–2,700 km² Ardennes catchments |
| meander λ/W | 10 | set so the MEASURED ratio lands mid-band; smoothing shifts it ~21% |
| detail amplitude | 4 m | swept against the suite — **re-sweep it whenever the macro stages change** |

**That last row is a standing instruction, not a note.** The detail amplitude has been re-swept four
times because it is not independent of the macro relief underneath it nor of the field's extent. The
tables are in `DetailParams`' header. Goal 320a is to express it as a fraction of local relief.

---

## 4. Caves, and the occupancy rule

The one place the world stopped being 2.5D. The full plan is at the top of `world/svo/caves.hpp`;
the short version:

`TerrainSampler::classify` returns **Solid for a box entirely below every column's surface without
subdividing it**. A cave inside such a box would never be looked for. So every conclusion of "this
whole box is solid" must first prove no cave can intersect it — `caves_possible_in_band` is that
proof, and it is **conservative by construction: false means provably cave-free**.

A band rather than a noise bound, because bounding 3D noise over a box needs either interval
arithmetic FastNoise2 does not offer or a Lipschitz bound so loose that every box in the band would
return Mixed anyway.

**`CaveParams::threshold = 0` restores the exact 2.5D world**, and that is what the
`TerrainSampler`/`fill_terrain` byte-equivalence test runs with — the mesh path is the documented
fallback and has no cave rule. The property that makes disabling sound is asserted separately.

---

## 5. Running the validation suite

The ten acceptance tests from research Part 7 §9 are a library:
`world/generation/validation/acceptance.hpp`. **Every band is a named constant with its citation in
a comment beside it**, and every metric has an instrument test against an analytically known answer.

```bash
ctest --test-dir C:/b/windows-relwithdebinfo -LE scenario -R "generation"
```

To see the numbers rather than a pass/fail:

```bash
C:/b/windows-relwithdebinfo/tools/terrain_dump/terrain_dump.exe --plane biome --out biomes.png
```

`terrain_dump` prints the suite, the per-biome stem densities, the river network, the climate
statistics, and the threshold sweeps. Planes: `elevation`, `flow`, `precip`, `temperature`, `biome`,
`rivers` (with `--zoom X,Z,SPAN`). **Re-save any PNG through PIL before committing it** — the
project's PNG writer stores uncompressed.

### The standing result

Seven of ten pass on the shipped seed. Three do not, each with a cause:

| test | measured | band | |
|---|---|---|---|
| 1 slope skew sign | −0.55 at 8.0° mean, unimodal | sign | PASS |
| 2 spectral β | 2.111 (R² 0.913) | 1.6–2.5 | PASS |
| 3 hypsometry median/max | 0.159 | ≤ 0.40 | PASS |
| 4 drainage density | 5.59 km/km² | 2–12 | PASS |
| 5 valley exponent b | 2.309 | 0.7–1.4 | **FAIL** — diffusion rounds a V into a parabola; at 16 m cells with 112 m relief there is little V left to round |
| 6 constant-drop \|t\| | 13.70 | < 2 | **FAIL** — and pure noise passes it. Not discriminating on an 8 km field |
| 7 slope–area exponent | −1.654 (R² 0.805, plateau present) | −0.6 to −0.35 | **FAIL** — uplift is zero, so this is a decaying landscape, not a graded one; −0.5 is not the prediction |
| 8 variogram Hurst | 0.546 (R² 0.983) | 0.46–0.77 | PASS |
| 9 hydrological coherence | 0 basins, 1132/1132 reaches, 158/158 lakes | structural | PASS |
| 10 stems per hectare | 550 | 400–700 | PASS |

**Do not tune these three away.** Each has an open goal and a stated cause.

---

## 6. What this world is, and what that costs the research's numbers

**8 km across, 112 m of relief, 0.50 °C of temperature range.** That is a gentle coastal plain, and
it is the single most useful thing to know before reading any acceptance result:

- **No snowline.** The freezing isotherm sits at 2,154 m; the highest ground is 44.6 m. The glacial
  stencil selects zero basins — correct behaviour, not a failure.
- **No cliffs.** 1,423 coast cells, zero cliffed. Stratigraphy's benches are ~1.5 m inflections that
  no camera pose can show.
- **Whittaker's temperature axis does not exist.** The biome map is a moisture map with a thin
  alpine belt (0.5% of the field, and absent entirely below a 5.1 km field).
- **Rivers are sub-cell.** 2.42 m wide against a 16 m cell.
- **The research's anchors are quoted for landforms 10–20× larger.** §6.2's 1:10 rain shadow is for
  a 1.5–2 km barrier; drainage statistics want a domain with enough high-order links to run a
  statistic on.

Raising the macro relief is goal 316a and it would move every number in this document.
