# Visual Direction & 3D Models — Turning the Plan into Assets

`06-game-design-ideas.md` already worked out the game's *systems* (tech tiers, crop data,
carbon debt, pipeline stages) — genuinely solid work, and most of it is now real code (see
`src/game/gameplay/`). What was still missing was **what any of it should look like.** This
doc closes that gap: concrete 3D model ideas, real-world visual references, and design lessons
from games that make real science feel fun instead of like homework — pulled together from a
research pass across comparable games, real lab-equipment references, and a direct read of the
current codebase (so the model list ties to actual, specific files, not guesses).

Companion doc: `Research/06-climate-and-ecosystem-science.md` (the science this all represents).

---

## 1. Where things actually stand (read this before modeling anything)

A grounded look at the current code, not the plan:

- **The farm/chemistry simulation has zero visual representation today.** `GamePlayScreen` is a
  walkable voxel world; `Block` in `VoxelWorld.hpp` only knows `Air, Grass, Dirt, Stone, Sand,
  Snow, Wood, Leaves`. Farm/chemistry state lives entirely in `src/game/gameplay/` and isn't
  wired to anything visual yet.
- **The model registry is a blank slate.** `ModelAssetId` in `src/engine/models/ModelSystem.hpp`
  is an empty enum. `assets/models/` has no model folders yet, only its own README. You're
  choosing the *entire first wave* of assets, not filling gaps in an existing list.
- **Two of the seven named processing stages are real; five are stubs.** `Distill` and
  `Transesterify` (`src/game/gameplay/stages/`) both compute real fuel output. `WashCrop,
  GrindCrop, Ferment, PressExtract, Pretreat` are literal pass-throughs (`PassThroughStages.hpp`)
  — named, real stops in the pipeline, but currently no-ops. **`Transesterify` is exactly the
  separatory-funnel/NaOH/methanol step from the original capstone sketch, and it's one of the
  two stages that actually works** — the natural place to start.

---

## 2. Real biodiesel lab equipment — model reference

The original capstone sketch named real equipment. Here's what it actually looks like, so a
first-time 3D modeler gets the shape right without ever having seen a chemistry lab.

**Separatory funnel** (the visual centerpiece of the `Transesterify` stage). Pear-shaped clear
glass, ~a foot tall, hanging neck-up from a clamp on a ring stand (it has no flat base — it
never sits on a table). A white plastic stopcock valve at the bottom tapers to a drip tip. After
the reaction, two layers settle inside: **biodiesel on top** (golden, translucent) and
**glycerol on the bottom** (darker amber, denser). The signature move — inverting the funnel to
vent pressure with a hiss, then draining the bottom layer through the stopcock — is an easy,
readable animation beat.

**Magnetic stirrer / hotplate.** A low flat box (roughly the footprint of a paperback book) with
a square metal top plate and two knobs. A flask sits on top; inside it, an invisible spinning
magnet ("flea") creates a visible vortex in the liquid. Green light = stirring, red light =
heating.

**Beaker / Erlenmeyer flask / graduated cylinder.** Beaker = short wide cylinder with a pour
spout. Erlenmeyer = cone-shaped, flat-bottomed, more stable — the standard small-scale reaction
vessel. Graduated cylinder = tall and narrow, for precise measuring.

**Filtration setup** (the sketch's "filter cap" + "filteration flask"). A **Büchner funnel**
(shallow white bowl with a flat perforated bottom, filter paper on top) seated in a rubber
sleeve on top of a **side-arm flask** (like an Erlenmeyer but with a glass tube sticking out the
neck for a vacuum hose).

**NaOH.** A **solid** — white pellets in a labeled plastic bottle with a corrosive-hazard
label. Not a liquid; shown with a scoop, not poured.

**Methanol.** A clear flammable liquid, stored in an **amber glass bottle** or a **red metal
safety can** (red = flammable, by color-coding convention) — never a plain clear bottle.

**"Air compressor" (from the sketch) — likely a mix-up worth a quick check with a chemistry
teacher.** A shop air compressor isn't real biodiesel equipment. What the sketch probably means
is either a small **aquarium air pump** (used to bubble-wash the finished fuel and strip out
leftover methanol/soap) or a **vacuum pump** for the filtration step. Model whichever one
matches the actual gameplay step — a small pump box with a hose, not an industrial compressor.

**Safety gear worth including** (makes it read as a real, careful lab rather than "kids mixing
chemicals"): goggles + gloves on the player's own hands, a fume hood behind the bench, a red
fire extinguisher on the wall, a yellow eyewash station nearby.

**Whole-station layout:** one bench/counter, left to right — oil source → reaction (stirrer +
flask) → separation (funnel on a ring stand) → filtration/wash → finished-fuel bottle. This
left-to-right flow can map directly onto the player's walk down the station.

---

## 3. Clean energy vs. dirty industry — the visual contrast

The original concept (a smoke-belching villain factory, "Tony's Special Gas Industries," vs. a
clean biofuel farm) is a strong, real visual tradition in games — see §4. Reference for each
element:

- **Solar panel:** a flat dark blue-black slab (~5×3 ft) in a thin metal frame, tilted ~30°
  south, in rows on a low steel rack. The tilt and row-repetition are what make it read as
  "array," not "roof."
- **Wind turbine (farm-scale, not utility):** a tall thin tower (much taller than the blades are
  wide — a 3:1 to 5:1 ratio), 3 white/gray blades, and — importantly — a **tail vane** behind the
  rotor. The tail vane is what visually distinguishes a small farm turbine from a giant utility
  one; keep it. A lattice (angle-iron) tower with guy wires reads as "farm," not "wind farm."
- **Water wheel:** the classic image is an **overshot wheel** — water pours in from a raised
  wooden flume at the *top*, and the weight of water in the buckets turns it. Wood construction
  (weathered gray-brown) with iron rim bands fits a cozy-farm aesthetic better than steel. Keep
  the bucket divisions around the rim — a smooth rim reads as a gear, not a water wheel.
- **The rival factory:** the single most recognizable "dirty industry" shape is a **hyperbolic
  cooling tower** (narrow waist, flared top and bottom) — even simplified, that pinched-waist
  silhouette is unmistakable. Pair it with 2–4 thin tall smokestacks (taller than anything else
  on the structure) and a row of cylindrical storage tanks. The game's own green-tinted smoke
  idea is a good stylized "toxic" cue (real smoke is gray-black or white steam, but green reads
  as poison instantly in a stylized game).
- **Making invisible CO2 visible:** games solve this with color, not physics simulation — a
  persistent brownish-green haze around the factory, particle plumes drifting from stacks, the
  sky tinting progressively browner near pollution and clear blue near clean energy. Clean
  energy gets the opposite treatment: bright saturated greens, sparkle particles, a light bloom
  on active generators. A stream turning from murky to clear as the player cleans up the area is
  a cheap, well-attested trick (used in *The Path to Luma*, a real clean-energy game).

---

## 4. Design lessons from games that make real science fun (not homework)

Researched across Kerbal Space Program, the Zachtronics catalog (SpaceChem/Opus Magnum), *Eco*,
*Oxygen Not Included*, *PowerWash Simulator*, *Plague Inc.*, *Cell to Singularity*, and the
"dirty vs. clean" genre (*Terra Nil*, *Catan: New Energies*, *Factorio*). Top five takeaways,
ranked by fit for a first-person 3D game with a hands-on lab-table interaction:

1. **Drastic, instant before/after feedback on every action** (the *PowerWash Simulator*
   technique). Every pour, stir, or heat step needs an immediate, high-contrast visual response
   — color change, bubbles, glow — plus one consistent, satisfying audio "ding" on completion.
   This is the single best defense against the mixing station feeling like a checklist.
2. **Failure that's spectacular, funny, and self-explaining** (the Kerbal Space Program
   technique). A wrong ratio should fizzle, foam, or fume in an amusing, diagnostic way — never
   a red error box. The player laughs, learns the rule, and tries again.
3. **Make the invisible a live, watchable overlay**, not a text readout (Kerbal's orbital map,
   *Oxygen Not Included*'s gas overlays). A live concentration/yield meter on the bench; a
   held "scanner" tool the player can raise to tint the world and reveal pollution or soil
   health.
4. **Let the player watch their own machine run** (the *Opus Magnum* technique). Once a mix is
   right, let it visibly process — flowing tubes, bubbling flasks, a filling gauge — so the
   payoff is watching something *you built* work, not a score screen.
5. **Clean vs. dirty as a felt mechanical consequence, never a stated message** (*Eco*, *Catan:
   New Energies*, *Terra Nil*). The fossil path should be visibly faster/cheaper but cause a
   real, visible cost (a smog haze that dims nearby crops); the biofuel path is slower but
   visibly heals the world. The player concludes "clean energy works" because they watched it
   happen, not because a character told them so.

---

## 5. 3D model priority list (grounded in the actual code)

### Tier 1 — unlocks gameplay that's already coded

| Model | Represents | Difficulty | Placeholder-first |
|---|---|---|---|
| Separatory funnel + stirrer + filtration rig | `stages::Transesterify` — the real, working final biodiesel stage | Beginner–moderate (funnel/flask are classic "spin around a profile curve" exercises) | Cone-on-sphere funnel, plain cylinder flask, flat box plate — upgrade shape later without touching code |
| Distillation still (pot + neck + condenser) | `stages::Distill` — the real shared final stage of the ethanol/cellulosic pipelines | Moderate (the coil is the fiddly part) | Ship v1 with a straight pipe stub instead of a coil |
| Generic stage props (wash basin, hopper, tank, press plates) | The five currently-inert `PassThrough` stages (`WashCrop`, `GrindCrop`, `Ferment`, `PressExtract`, `Pretreat`) | Beginner (boxes/cylinders) | Plain colored boxes are legitimately fine long-term — the code behind them is a no-op too |

### Tier 2 — world-building for the theme

| Model | Represents | Difficulty | Placeholder-first |
|---|---|---|---|
| Five crop models (corn, sugarcane, soybean, switchgrass, algae) | `data::CropId` / `TileType` — each currently has only a flat color, no mesh | Beginner (stem + leaves, or a plane for algae) | Recolor the *same* simple cone per crop using its existing `TileRenderColor` value — the color data already exists |
| Processing shed/barn | Houses the Tier-1 station; would be the first concrete building type (`Tile::buildingId` exists but has no defined types yet) | Beginner (box + roof prism) | A plain gray box is a fine v1 |
| Solar array, wind turbine, water wheel | Theme reinforcement — no code hook yet, pure scenery | Beginner (flat panel / pole+blades / cylinder+paddles) | A flat-colored primitive per element is basically the finished look already |
| Distant rival factory | Landmark silhouette — no code hook, a static prop at a fixed position | Beginner (cylinders + a smoke plane) | Two grey cylinders on the horizon read as "factory" immediately |

### Tier 3 — polish props

Fuel jerrycan (visualizes `ProcessingOutput.fuelGallons`), harvest crate (visualizes
`HarvestOutput` moving field→station — `assets/models/README.md`'s own example folder is
literally named `harvester_popout/`, so this was already anticipated), fence/rail segments, a
signpost, loose lab-glass clutter for the Tier-1 station. All single-or-two-primitive beginner
models with no code dependency.

**Where to start, concretely:** the separatory funnel. It's the single most recognizable object
from the original sketch, maps to the one pipeline stage that's both real *and* matches the
original capstone vision, and is a standard first-project shape in any 3D modeling tutorial
(a lathe/spin operation around a 2D profile curve — Blender, Tinkercad, and most beginner
tutorials cover this exact technique early).

---

## 6. Concrete "cool scientific and fun" gameplay moments

Ideas for making the chemistry-mixing loop and the clean-vs-dirty tension feel like play, not a
lecture — grounded in the actual pipeline code and the real chemistry (methanol + NaOH →
sodium methoxide, an exothermic reaction; mixed into ~55°C oil; too much water/free fatty acid
and it saponifies into soap instead of fuel — a real failure mode that's also a built-in joke):

- **The core loop:** combine methanol + NaOH off to the side first (it visibly fizzes/steams —
  it's genuinely exothermic); pour the result into heated oil and hold a stir input through a
  swirl-shader vortex; then tilt/drain the separatory funnel to catch only the glycerol layer
  without spilling into the fuel — a precision-pour skill check, PowerWash-Simulator style.
  Miscalibrate the ratio and the whole vessel turns into a giant wobbling soap block — not a
  fail screen, a physical, climbable, screenshot-worthy joke.
- **The rival factory as a felt presence, not a cutscene:** put it on the skyline, permanently
  visible from the farm. Tie its smoke plume's density directly to the player's own cumulative
  fuel output — thick and dark early game, visibly thinning as the player's own production
  climbs. Let it re-skin over time (grimy → vines → solar panels bolted on) so it becomes a
  landmark the player can eventually walk to and see reclaimed.
- **Making CO2 visible without a HUD:** a faint stream of dark motes drifting up from
  overworked monocrop tiles, bright green motes from healthy/fallow tiles — walkable, comparable,
  no number required. A literal glass silo that fills with visible "smog" blocks as carbon debt
  rises and drains as it's paid down.
- **A win that's a flex, not a lecture:** frame tech-tier unlocks as visibly upgrading the
  player's own station (bigger columns, glowing pipes) — "I built something cooler than Tony,"
  the same satisfaction as a Factorio base getting more elaborate — rather than a "you saved
  X tons of CO2" scoreboard.
- **Memorable moments to build toward:** the first perfect zero-spill pour; the soap-block
  mishap; standing on your tallest silo at night and seeing your own glowing farm next to Tony's
  now-sputtering smokestack on the same horizon, entirely from world geometry; the first time
  cumulative fuel output visibly powers something in-world (a tractor turns over, lights flick
  on at dusk).

---

## Sources

Full citations for every claim in §2–4 (real lab-equipment specs, real turbine/solar/water-wheel
dimensions, and the game-design research) are preserved in the research session's working notes
and can be pulled forward into this file on request — trimmed here for readability. Key
references used: NOAA/NASA/IEA/EPA (science, cross-referenced with `06-climate-and-ecosystem-
science.md`); Corning/PYREX product specs, university SOPs, and biodiesel-education lab guides
(equipment); MDPI wind-turbine and water-mill reviews, solar installer guides (renewable
visuals); Game Developer, Kotaku, PC Gamer, and Zachtronics/FuturLab/Strange Loop Games
developer interviews (design lessons).

See also: [[06-game-design-ideas]] (the systems this visual plan is dressing), [[06-climate-and-ecosystem-science]] (the science behind it).
