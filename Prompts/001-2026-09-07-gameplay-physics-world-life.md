# Prompt 001 — Gameplay physics, crosshair/aim UX, wind, trees v2, grass, water motion

**To:** the main coding session ([CC]), branch `C++-voxel`
**From:** the side session, 2026-09-07
**Read this whole file before touching anything.** It is the complete brief: context to read,
standing rules, five task groups (A–E) with Check criteria, and explicit out-of-scope items.

---

## 0. What this pass is and is not

The engine renders a beautiful sub-centimeter ray-marched world, but it is still a *spectator
with a body*: you fly by default, walking exists but is minimal (no jump, micro-step jitter
from 7.8 mm voxel stairs, no coyote/buffer timing, no head-bob/ground feedback), there is no
visible crosshair (only an overlay *text* readout of what you're aiming at), there is no wind
system, trees are three hand-composed primitives (box trunk + octahedron canopy) with no
biomechanical plausibility, there is no grass at all, and water is visually static (a fixed
ripple lattice in the shader). This pass turns each of those into a real system, in the order
below. It is a gameplay/vegetation pass, NOT an editing pass (goal 160), NOT an incremental-
rebuild pass (goal 158), NOT a path-tracing pass.

**Named outcome:** you stand on a hill, the crosshair names the thing you look at, wind moves
grass and tree canopies in a shared gust field, trees look and move like trees, and the water
surface visibly travels and breaks. All of it composes with the existing svo renderer's
shadows/AO/TAA, on both Vulkan and D3D12, at interactive frame rates.

## 1. Context to read FIRST (in this order — do not skip, do not summarize from memory)

1. `CLAUDE.md` (repo root) — machine-specific build notes. Non-negotiables you will hit
   immediately: build with `cmake --preset windows-relwithdebinfo` (or `windows-release`) to
   `C:\b\...`, MSVC only, source `vcvars64.bat` from the SAME edition the build dir was
   configured with, never mix the two VS installs, shaders in
   `render/diligent/shaders/*.hlsl` load at runtime (edit + relaunch, no rebuild).
2. `docs/progress.md` — current state + the architecture map + "decisions that survived
   contact with evidence" (do not re-litigate decided-against items without new evidence).
3. `docs/goals.md` — the backlog. This pass adds new groups; it does not silently close open
   goals (158 incremental rebuild, 160 editing, 173 octree collision, 175 growth-swap hitch
   all stay open and untouched).
4. `research/lin-look-log.md` and `research/micro-voxel-pivot-log.md` — how the svo renderer
   and its shading/TAA actually work, and why (secondary rays judge LOD from their own origin;
   tree layout v2; the slow-frame attributor).
5. The research files this prompt was built on (read the sections the tasks cite, not
   necessarily front-to-back):
   - `research/grass-rendering-research.md` — how voxel engines do grass + wind, hybrid
     ray-march/raster depth composition (§3), performance budgets (§4), ranked options (§5).
   - `research/micro-voxel-creators-research.md` — John Lin / MishMash / Teardown / Octo
     techniques with CONFIRMED/INFERENCE labels (§1–3), character controllers at micro-voxel
     scale (§4), ranked adoptables (§5).
   - `research/tree-motion-growth-and-appearance.md` — tree biomechanics: cantilever sway
     frequencies (§3.1), multiple-resonance damping (§3.3), leaf-on/leaf-off mass effect
     (§3.4), Vogel exponent (§4.2), flutter threshold (§4.3), petiole stiffness as the species
     knob (§4.4), Pipe Model leaf-count derivation (§5.3), the recommended pipeline (§8), the
     parameter table (§9).
   - `research/water-physics-and-wave-simulation.md` — Gerstner waves (§8.2), spectra
     (§8.3), the shoaling/refraction/breaking pipeline (§9), parameter table (§10).

## 2. The systems you are building on (file-level map, verified 2026-09-07)

- **Per-frame loop:** `app/src/main.cpp` — `Session`, `run_svo` (default) / `run_mesh`;
  `update_camera_phase` is where input → camera → collision happens today. Frame phases are
  timed; slow frames (>20 ms) are attributed and counted by cause.
- **Input:** `engine/input/input_state.hpp` (semantic snapshot, accumulate-then-take edges)
  and `engine/input/src/glfw_input.cpp` (W/A/S/D, Space=up, LCtrl=down, Shift=boost, G=fly/walk
  toggle, F2=screenshot, Esc=quit; RMB-held = mouse-look). Free keys: E, F, Q, Z, X, C, V, B,
  H, J, K, L, M, N, R, T, U, Y, 1–9, Tab, LAlt.
- **Camera/movement:** `app/src/spectator_camera.{hpp,cpp}` — `SpectatorCameraState`
  (yaw/pitch/speed/mode/vertical_velocity), `compute_spectator_step` (wanted motion before the
  world has a say), walk-mode gravity + buoyancy + drag (constants from the Water material
  def), `clamp_to_ground` backstop. Body constants: `kEyeHeight=1.7`, `kBodyHalfWidth=0.3`,
  `kBodyHeight=1.75`, `kStepHeight=0.55`, `kGravityAcceleration=-32`.
- **Collision:** `world/collision/` — `SolidQuery` concept, `move_and_slide` (y/x/z bisected,
  0.25 m sub-steps, step-up in walk mode), `TerrainCollider` (analytic height function over a
  cached 16 m / 3.1 cm grid + trunk boxes, background refresh; `params.voxel_edge` set from
  the tree's finest voxel on the svo path, 1.0 m on the mesh path).
- **ECS/events:** `engine/ecs` (entt alias; `Transform`, `CameraLens`), `engine/events`
  (entt::dispatcher alias; MAIN-THREAD-ONLY rule and the event-vs-polling convention are
  documented in `dispatcher.hpp` — read them).
- **World/materials:** `world/materials/` — `MaterialDef` schema (`material_def.hpp`), the
  composition `materials.hpp` (Air, Stone, Dirt, Water, Wood, Leaves, Sand, Grass; ids derived
  from pack order). Adding a material = one file in `defs/` + one entry + one enumerator.
  `tree_replaces` governs voxelization priority.
- **Trees today:** `world/generation/tree_placement.{hpp,cpp}` (jittered-grid placement, three
  `TreeShape` silhouettes: Round/Conifer/Shrub; implicit box-trunk + octahedron lobes;
  deterministic per (seed, column)). Consumers: the mesh emitter (`app/src/tree_decoration`)
  and the svo voxelizer (`TerrainSampler::voxelize_trees` in `world/svo/terrain_sampler.cpp`).
  The mesh path already has canopy sway (`terrain.vsh.hlsl`, goal 39 — sine wobble on foliage-
  shaded material only); the svo path shades Leaves as `Shading::Lit` and does NOT sway.
- **SVO renderer:** `render/diligent/shaders/svo_march.psh.hlsl` (statement-for-statement
  mirror of `world/svo/src/ray_trace.cpp` — THE RULE: a traversal change lands CPU-side
  first, passes the 7,000-ray oracle, then is mirrored in HLSL; both backends must pass) +
  `svo_taa.psh.hlsl`. `SvoRenderer` (`render/diligent/src/svo_renderer.cpp`) with staged
  uploads, spare buffer reuse, DYNAMIC SRB variables (MUTABLE binds once — read
  `research/lin-look-log.md` before touching SRB variables).
- **Trees-in-the-octree:** `world/svo/brick_tree.hpp`, `tree_layout.hpp` (layout v2: header
  word, attribute word (int8×3 normal + uint8 coverage), child pointers; `kMaxCanopyLobes=3`),
  `tree_builder.hpp` (distance-LOD build, `parallel_split_level=5`), `TerrainSampler` (the
  SAME world as `fill_terrain`, proven byte-identical at 1 m — do not break that test).
- **Aim query:** `app/src/aim_query.{hpp,cpp}` — analytic ray march for the overlay's
  "Material @ x,y,z" text line. It does NOT see trees (analytic height only).
- **Debug tools you will use constantly:** `--debug-view` (12 single-term views),
  `tools/svo_render` (CPU reference frames, `--lod-center`, `--view`), `--verify-frame`
  (local-contrast metric; current threshold 6%, terrain reads ~34% with TAA),
  `VOXEL_DUMP_FRAME`, `--dump-every`, F2, `--pos/--yaw/--pitch`, `--autofly --walk` (0 ground
  violations required), slow-frame attributor exit summary.

## 3. Standing rules (all of these are the repo's own rules, restated)

1. **A visual change is verified by a viewed capture, not by a passing number.**
   `VOXEL_DUMP_FRAME=<path>` + actually opening the PNG, on BOTH Vulkan and D3D12 where
   shader-affected. `--verify-frame` alone is never "done".
2. **CPU reference first, GPU mirror second, oracle always.** Any `ray_trace.cpp`-adjacent
   change (grass-in-march, water displacement of the march, wind-warped sampling) lands in
   `ray_trace.cpp` + `tools/svo_render` + the oracle tests first, then in `svo_march.psh.hlsl`.
3. **Performance claims are measured, not assumed.** The slow-frame attributor and the 2 s
   stats line (including `gpu march+resolve`) are the tools; before/after numbers go in the
   research log. Any per-frame CPU work must be justified against the existing phase budget
   (upload/camera/render/post/overlay/present).
4. **Tests stay green.** 119/119 today. New systems get new tests (unit tests for pure
   functions; the walk invariant counter is extended, not bypassed).
5. **Determinism:** placement of trees/grass must remain a pure function of (seed, position)
   — same seed → same world, both renderers, `tools/svo_render` included. The equivalence
   test (`test_terrain_sampler.cpp` vs `fill_terrain`) must not break.
6. **Materials are components.** A new material (GrassBlade if you add one) is one file in
   `world/materials/defs/` + one entry + one enumerator, with its `phase` (likely
   `Phase::Foliage`), shading, tree flags, and `fills()` (likely `return false;`). The
   shaders get its record automatically via the palette macros.
7. **Main-thread-only events; workers hand results through completion queues** (the
   `dispatcher.hpp` convention). New tree/grass build jobs follow `SvoWorld`'s existing
   thread pattern (worker + pool, at most one in flight, results adopted on the main thread).
8. **No new dependencies without a written case.** Everything in this pass is implementable
   with the existing stack (Diligent, EnTT, GLM, FastNoise2). If you believe a dependency is
   genuinely needed (e.g. a mesh library for tree assets), STOP and present the case in the
   research log before adding it — the repo has a history of rejecting unpinned/unfit deps.
9. **Read `docs/goals.md` group notes before closing any goal** — several things LOOK done
   but have specific checks (e.g. goal 84's aim query tests).
10. **Use your skills.** The C++ work here (concepts-constrained samplers, policy-shaped
    systems, new modules) is exactly what `cpp-heavy-templates` governs — load it and follow
    it. For anything that turns out to need live verification beyond the repo's own tools
    (a technique nobody has documented), switch to `research-and-think`'s empirical mode
    rather than guessing.

## 4. Task groups

Work them in order A → E; within a group, tasks are roughly sequential but you may interleave
verification. Each task has a **Check** — that is the acceptance criterion, per the repo's
standard. As you complete groups, append a decision log to `research/gameplay-pass-log.md`
(new file; same style as `research/lin-look-log.md`): what was measured, what was decided
against and why.

---

### Group A — Player physics: from spectator-with-gravity to a real controller

The reference research here is `research/micro-voxel-creators-research.md` §4 (character
controllers at micro-voxel scale; the 2–5 cm smoothing-budget conclusion; fenomas sweep;
VoxelBoxMover semantics) plus the existing `world/collision` tests as the baseline. Key
insight from the research, which the current code ALREADY half-embodies: at 7.8 mm voxels,
step-up is not stair logic, it is a **smoothing budget** — the current 0.55 m step height is a
mesh-world relic; on the svo path every natural slope is sub-cm stairs and the failure mode is
micro-jitter, not blocked stairs.

**A1. Fixed-timestep simulation with interpolation.** Today `update_camera_phase` runs physics
at render-dt (variable). Integrate a fixed timestep (target 60 Hz; `engine::core::Clock` gains
an accumulator; camera render pose = interpolate(prevPose, currPose, alpha)). Store the
accumulator in `SpectatorCameraState` or a new `app`-level struct — NOT in the render loop's
locals. Keep `--autofly`'s constant-velocity motion working (it asserts against ground
fall-through every frame; extend the invariant counter to the fixed-step world).
**Check:** a unit test drives N fixed steps at a forced irregular render cadence and asserts
identical end state to a regular cadence (determinism of the sim); `--autofly --walk` 900
frames still reports 0 ground violations; slow-frame attributor shows no new `camera`-phase
cost spikes at 30 fps vs 144 fps (measure at both).

**A2. Jump with coyote time and jump buffering; ground state machine.** Space in walk mode =
jump (it is currently inert vertical-strafe in walk mode — change that), with ~0.1 s coyote
time after leaving a ledge and ~0.1 s input buffer before landing. Requires a real grounded/
airborne/swimming state machine on the player (currently `grounded` is a per-frame sweep
result). Input: reuse `move_up` (Space) — its fly-mode meaning is unchanged.
**Check:** new unit tests on the state machine (pure functions of state + input edges:
coyote expiry, buffer consumption, jump-strength vs gravity numbers — jump apex ≈ 1.0–1.25 m
with `kGravityAcceleration=-32`, i.e. initial vy ≈ 8–9 m/s); interactive check: you can walk
off a 0.5 m ledge, jump 0.12 s late, and still clear a gap you'd otherwise miss.

**A3. Micro-step smoothing on the svo path.** Reduce the svo-path step height to a smoothing
budget (start 0.04 m = ~5 voxels; expose `--step-height M` to tune with captures) and add
ground-height smoothing (critically-damped spring or exponential on the *camera eye*, not the
feet — the feet stay exact; the eye target follows with a ~0.1 s time constant, disabled while
airborne). This is what makes walking on 7.8 mm voxel stairs feel like a surface instead of a
staircase. The mesh path keeps its 0.55 m step (1 m blocks there).
**Check:** walk up a steep grassy hill at fixed camera height-delta captures (`--dump-every`
sequence) — eye height traces a smooth curve, not voxel steps; `--autofly --walk` still 0
violations; a new test asserts the smoothed eye never diverges from the exact feet by more
than one step-height budget.

**A4. Crosshair + richer aim readout.** A visible crosshair (render it in the DebugOverlay as
   a screen-center ImGui element first — a few pixels, no new PSO; if it needs to be visible
   with the overlay hidden, move it to a tiny second fullscreen shader pass AFTER TAA so it is
   not anti-aliased into mush — document which you chose and why). Extend `aim_query` to
   report TREES (sample `tree_material_at` along the ray the way the voxelizer does -- the
   query currently sees only the analytic height field, so trunks and canopies are invisible
   to it; water it already handles) and show the hit DISTANCE in the overlay line.
**Check:** aiming at a trunk reads "Wood @ … (3 m)"; aiming at water reads "Water @ … (12 m)";
new aim tests for the trunk case; capture with the crosshair visible, viewed.

**A5. Swim-through-and-climb-ashore.** The buoyancy model floats you at the surface; you
   cannot dive (Space/Ctrl inert in water) or climb onto a shore ledge from the water.
   Implement: Space = swim up / Ctrl = swim down while submerged (direct velocity input,
   damped), and the "liquid pop-up" trick from the research (rlVoxel): when horizontally
   blocked while swimming and there is air within ~0.6 m above the contact, apply an upward
   impulse. Keep the equilibrium-depth bobbing.
**Check:** from floating, you can dive to the seabed and surface again; you can swim to a
   0.5 m shore lip and walk out without flying; unit tests on the pop-up condition and dive
   input gating; `--autofly --walk` crossing a lake shows 0 violations and a bounded
   submersion profile in the stats line.

**A6. Player-driven camera polish (the "feel" pass).** Head-bob (subtle, walk-only,
   speed-scaled, tied to the smoothed ground speed, off in fly/water/air), landing dip (a
   one-shot eye-offset impulse proportional to impact velocity, clamped), FOV kick on boost.
   All parameters constants in one place; all of it killable with `--no-view-polish` (and off
   under `--verify-frame`/`--autofly` so the mechanical checks stay stable).
**Check:** viewed capture pair (with/without) at eye height walking across a meadow; the
   mechanical `--autofly` checks unchanged (polish is off there); a test asserts polish never
   moves the eye more than 5 cm from the physical pose.

### Group B — Wind: one shared wind field

Everything that moves (grass, canopies, later flags/sails) reads ONE wind system; nothing
invents its own per-feature wind again (the mesh path's `terrain.vsh.hlsl` sine wobble gets
retired into this system — see C-group).

**B1. `world/wind` (or `engine/…` if you judge it engine-level — decide and write why):
   a `WindField` evaluation `wind(pos, t) -> (dir, strength, gust)`**, analytic and
   deterministic: base direction + strength, plus a time-varying gust term from
   scrolling low-frequency noise (FastNoise2 on CPU for placement logic; the SAME formula
   hand-ported to HLSL as a small `wind.fxh` include used by every shader that needs it —
   identical constants, one source of truth: a shared header that generates both the C++
   constants and the HLSL via the existing macro-passing mechanism, or twin functions with a
   test that samples both over a grid and asserts equality — your call, document it).
   Reference: Ghost of Tsushima's model per the research — constant direction, Perlin-varied
   magnitude, visible as gusts sweeping a field.
**Check:** CPU/GPU parity test over a 64³ grid of (pos, t) samples within float tolerance;
   determinism test (same t → same wind, independent of evaluation order); no per-frame
   allocations.

**B2. Wind constants as art-direction data.** Base speed ~3–6 m/s default, gust amplitude and
   frequency, `--wind-speed M` / `--no-wind` flags for A/B captures.
**Check:** `--no-wind` makes every wind consumer (B, C, D groups) provably static (captures);
   flags documented in the usage string; overlay shows current wind at the player.

### Group C — Trees v2: skeletons, pipe model, sway

Reference: `research/tree-motion-growth-and-appearance.md` (the whole §8 pipeline is the
blueprint — space colonization skeleton, pipe-model radii, hierarchy of spring links,
per-lobe/leaf flutter keyed to petiole stiffness; §9's parameter table has the physics
numbers) + `research/micro-voxel-creators-research.md` §5.7 (voxelize polygon assets; place by
ray-traced queries) for the long-term direction, and MishMash's two-tier prop system (§2.3)
for the near/macro split. Constraint reality check: this repo has no asset pipeline and no
textures — trees stay PROCEDURAL (grown, not imported) this pass. That is a deliberate scope
decision, not an omission; write it in the log.

**C1. Skeleton generation: space colonization** (`world/generation/tree_skeleton.{hpp,cpp}`,
   new). Input: a seed + species params + the crown volume; output: a segment list (parent
   index, start/end positions, radius) — the standard Runions et al. algorithm. Deterministic
   per (seed, tree position). Species presets at minimum: the existing three silhouettes'
   rough shapes (Round broadleaf, Conifer, Shrub) so the world still reads the same at a
   glance, plus one "aspen-like high-flutter" broadleaf variant to give the wind work a
   showcase.
**Check:** unit tests — determinism (same seed → identical skeleton bytes), connectivity
   (every segment's parent is earlier in the list; exactly one root), bounds (skeleton fits
   the claimed crown volume), and a minimum-spacing property on branch tips; a `tools/`
   debug dump (text or .obj via the existing mesh_dump machinery) of three skeletons viewed
   (screenshot or obj imported anywhere viewable) before any voxelization.

**C2. Pipe-model radii.** Walk the skeleton tip→root accumulating each segment's distal
   "leaf count" (see C3) and set radius = sqrt(leaf area constant × accumulated count) per
   the Pipe Model (§1.2/§5.3 of the tree research). Replaces the constant
   `kTrunkHalfWidthRound/Conifer` values as the radius source (keep them as the fallback for
   the mesh path if you choose to not re-emit mesh trees this pass — decide and document; the
   minimal-change option is: mesh path keeps v1 trees, svo path gets v2 — see C5).
**Check:** unit test — a hand-constructed small skeleton gets exactly the expected radii;
   visual: trunks visibly taper, branch junctions satisfy da Vinci (parent cross-section ≈
   sum of children's) within 15% on generated trees (test over N random trees).

**C3. Leaf mass from the skeleton** (not a per-species constant): per §5.3, distribute LAI-
   scaled leaf area over segments proportional to sapwood area; each segment stores its
   distal leaf count. This feeds C2 (radii) and the sway model (C4: crown mass) and the
   canopy voxelization density.
**Check:** unit test — total leaf area per tree ≈ LAI × crown footprint (order of magnitude,
   0.5–3.0 LAI band per the research's oak numbers); denser at tips than near trunk (test the
   distribution shape).

**C4. Sway model (the physics): hierarchical spring links.** Each major branch = a damped
   driven oscillator hanging off the trunk's motion; trunk = fundamental mode from the
   cantilever formula (§3.1: f0 ∝ dbh/H² · sqrt(E/ρ), with the architecture correction so
   broadleaf crowns don't sway uniformly); branches semi-independent → multiple-resonance
   damping emerges structurally (§3.3). Evaluate on the CPU at the fixed timestep (it's a
   handful of state variables per tree per frame — budget it in the log; only trees within
   the LOD ring simulate; distant trees use the cheap closed-form phase). Seasonal lever:
   crown mass, not drag (§3.4), as a `--season leaf|bare` flag later — just wire the
   constant, don't build the seasonal art.
**Check:** a pure-function unit test of the oscillator (step response, resonance near the
   predicted f0 for the sycamore-scale parameters from §9: f0 ≈ 0.26 Hz); a determinism test
   (fixed seed + fixed wind → identical motion trace); budget: sway update for all in-ring
   trees ≤ 0.5 ms/frame measured with the attributor at the default world.

**C5. Voxelizing v2 trees into the svo tree.** Replace `TerrainSampler`'s octahedron lobes
   for the svo path with skeleton-driven voxelization: trunk segments as capsules (radius
   from C2), canopy as per-segment leaf clouds (density from C3) sampled into bricks at the
   tree's build resolution. **Memory warning from the research (Laine & Karras):** vegetation
   is the worst-case SVO content class; before committing, measure brick/MB growth at the
   default `--lod-radius 4` pose vs v1 trees — if the tree balloons (>1.5× the current
   ~200–400 MB), reduce leaf-cloud sampling resolution beyond the first LOD ring (the
   coarse-ring trick: leaves are shading detail, not silhouette, past ~8 m).
**Check:** build+upload times and tree MB before/after in the log; `--verify-frame` stays
   ≥ 25% (it reads ~34% today); oracle tests unaffected; viewed captures: v2 trees at 3
   distances (2 m, 10 m, 60 m) on both backends; the terrain-sampler equivalence test
   (terrain-only, no trees) still passes byte-for-byte.

**C6. Wind-driven canopy motion in the marcher.** The hard one — no public prior art for
   animating stored ray-marched voxels (research §2.3 of the grass file; Lin's mechanism
   unpublished). Two-pronged approach, in order:
   1. **Shading-domain motion (cheap, ship this even if 2 stalls):** sway the canopy
      *appearance* — per-hit grass/leaf materials sample the wind field at the hit position
      and modulate albedo/normal slightly with the flutter band (3–5 Hz sunfleck shimmer,
      §6.2 of the tree research); combined with TAA this reads as life at distance without
      moving geometry.
   2. **Geometric motion (bounded experiment):** displace the ray's sample position inside
      canopy-brick regions by the sway state (the C4 oscillator evaluated in-shader from
      per-tree constants uploaded as a small buffer: root pos, f0, phase, amplitude) before
      brick lookup — a domain warp. Risks (document if you hit them): DDA coherence at brick
      boundaries, TAA history rejection fighting the motion (extend the distance-mismatch
      tolerance for foliage materials), self-shadowing artifacts. If the artifacts cannot be
      tamed in bounded time, ship 1 + the D-group overlay near the camera and write the
      honest negative result in the log — that is an acceptable outcome and better than a
      broken marcher.
**Check:** for 1: viewed capture pair (wind on/off) of a canopy at 10 m — visible shimmer;
   `--debug-view lit` unaffected structurally. For 2 (if attempted): captures at 2 m and
   10 m; TAA ghosting evaluated in a slow pan (`--dump-every` sequence); oracle still 0/7,000
   (the warp applies at shading, not traversal, if you did it right — assert that).

**C7. Mesh-path parity (small).** Retire the ad-hoc `terrain.vsh.hlsl` sway: the mesh path's
   foliage displacement now reads the same `wind.fxh` field and a simplified C4 (per-tree
   phase + amplitude baked at emission). `--renderer mesh` remains the fallback world; it
   does not get v2 trees this pass (log the decision).
**Check:** mesh-path capture with wind on/off; the mesh path's own tests stay green.

### Group D — Grass

Reference: `research/grass-rendering-research.md` in full — the ranked options (§5) and the
performance table (§4) are the menu; the recommended combination (§5, last paragraph) is the
plan below. Roblox's clutter model (voxel terrain + geometric blades) is the shipped-engine
precedent; GoT's numbers (83 K blades, 2.5 ms, 16 floats/blade) are the budget anchor; the
raylib hybrid example is the composition template (§3).

**D1. Grass as voxels in the near ring (the John Lin look, static structure).** A
   `world/generation/grass_placement.{hpp,cpp}`: deterministic blade-cluster placement on
   grass-material surface voxels (reuse the tree placement grid/hash machinery; density
   masked by the same slope/height bands the Grass material fills). Voxelization: small
   blade clusters (each cluster a few thin voxel columns, 5–15 cm tall, leaning by a hash)
   sampled into the finest-ring bricks ONLY (beyond the first LOD ring, grass is ground
   tint, not geometry — this is the memory cliff mitigation). New material
   `defs/grass_blade.hpp` (Phase::Foliage — walk-through, not a floor; yields_to_trees =
   true).
**Check:** determinism test; memory/build-time delta measured and logged (target: < +15%
   build time, < +10% tree MB at the default pose — if exceeded, halve cluster density and
   log the tradeoff); viewed captures: a meadow at 1 m, 4 m (the LOD ring edge), 15 m (the
   tint falloff); walk through it — no collision (Foliage), no aim-readout lies (A4 must
   name GrassBlade).

**D2. Wind + player interaction on the blades (the overlay).** The research's #1 option: a
   thin instanced raster overlay composed via the march's depth buffer. Implementation:
   - A `GrassOverlay` in `render/diligent` (new): one instanced draw, blades as 3–7-vertex
     vertical quads/strips, vertex-color material (no textures — this repo's whole aesthetic),
     alpha-tested (clip), depth-test LESS against the march-written depth (the march already
     writes `SV_Depth` — composition is ordinary depth testing; the research §3 confirms this
     is the solved pattern; mind the depth-convention match).
   - Instance generation: CPU, deterministic, per grass cluster within ~8 m of the camera
     (regenerated when the camera crosses a cluster-cell boundary; budget in the log —
     GoT/Roblox numbers say 30–80 K blades is comfortably < 1 ms, and our ring is smaller).
     Instance data ~16–32 B/blade.
   - Vertex shader: layered wind (base + gust from `wind.fxh`, keyed to world position) ×
     height so roots stay planted; player-position sphere mask bend (the research's cheapest
     documented interaction — player pos in the constant buffer).
   - TAA: the overlay renders BEFORE `svo_taa` resolves so TAA covers it (verify the pass
     order in `post_process.cpp` / the svo path's frame composition; if the overlay lands
     after TAA it will shimmer — capture and check; adjust pass order so the overlay is
     inside the TAA'd scene, or extend TAA's history clamp to cover it — document which).
**Check:** the overlay composes correctly with march depth (viewed capture looking along a
   hillside — blades occlude and are occluded correctly); wind sweep visible in a capture
   sequence; walking through grass bends it (capture pair standing still vs walking through);
   frame cost measured (attributor + gpu timer if needed) — the pass must fit inside the
   existing frame budget at 60 fps (log the ms); both backends.

**D3. Distant grass tint.** Beyond the overlay+voxel ring, grass is a shading effect: the
   marcher's Grass-material hits sample the wind field and modulate albedo with a low-
   amplitude, low-frequency shimmer (same mechanism as C6.1 — share the code). This is what
   makes a whole valley read as alive at zero geometry cost.
**Check:** viewed capture at 60 m looking across a valley — visible but subtle motion in the
   green; `--no-wind` kills it; debug views unaffected.

### Group E — Water surface motion

Reference: `research/water-physics-and-wave-simulation.md` §8.2 (Gerstner — the exact
nonlinear solution used by every real-time engine; the steepness budget ΣQ ≤ 1 warning),
§8.3 (spectra — for later; do NOT build FFT this pass), §2.2 (dispersion; deep water
ω=√(gk) applies — our sea is effectively deep relative to ripple wavelengths), §3.3 (breaking
criteria — visual foam trigger only, not physics, this pass).

Scope decision, stated plainly so it is not re-litigated: **this pass adds the VISUAL wave
field (Gerstner displacement + normals in the marcher's water shading) and the player's
interaction with it (buoyancy already exists; add wave-surface sampling for the swim
surface). It does NOT add currents, SWE, foam advection, or breaking physics** — the water
research's §5.3/§9 pipeline (shoaling, reef dissipation, radiation stress) is a future arc
that would need its own pass; write that in the log as the named follow-up. Rationale: the
complaint that motivated this pass is "water is static"; the visual field plus surface
sampling answers it; the physical pipeline is a different magnitude of work.

**E1. Gerstner displacement of the water surface in the marcher.** The water path in
   `svo_march.psh.hlsl` currently ripples via a fixed noise lattice. Replace with 3–5
   Gerstner components (directions spread around the wind direction from Group B — wind and
   waves share the field, which is the physically right coupling): displace the surface
   sample position, derive analytic normals from the same sum (the research's formulation),
   keep the sun glint and fresnel composing on the new normals. Deep-water dispersion
   ω=√(gk); steepness budget ΣQ ≤ 1 with headroom (start ~0.6). Amplitude/period tied to
   wind speed (bigger wind = bigger, longer waves).
**Check:** `--debug-view` still attributes water correctly; viewed captures at 3 wave states
   (`--wind-speed 0.5/3/8`): calm glass, choppy, heavy swell — crests sharpen with wind;
   both backends; `--verify-frame` unaffected (water is a small fraction of the frame; check
   it stays above threshold); GPU march+resolve ms before/after in the log.

**E2. Wave-aware swimming surface.** The swim physics samples the sea level as a constant;
   sample the SAME Gerstner sum on the CPU (parity-tested against the shader) so the player
   bobs on the actual moving surface and the submersion/buoyancy math tracks the true
   surface height.
**Check:** CPU/GPU parity test over a grid of (x,z,t); floating at the surface in a capture
   sequence shows the camera riding the waves; `--autofly --walk` through a lake stays 0
   violations.

**E3. Shore interaction (visual only).** Where the Gerstner surface meets terrain shallower
   than ~½ wavelength, fade amplitude to zero (the research §2.3's depth-vs-wavelength
   criterion — a wave cannot ripple in water thinner than its orbit) and add a subtle
   shore-tint band modulation keyed to the local depth. No breaking, no foam particles this
   pass (log as follow-up with the §3.3 criteria named).
**Check:** capture at a beach: waves fade to flat at the waterline instead of clipping
   through sand; no z-fighting or seam at the fade boundary; viewed on both backends.

---

## 5. Sequencing & budget guidance

- Groups A and B are independent of each other; A is the highest-value-per-effort (the whole
  app feel changes). C and D both consume B. E consumes B. Sensible order: A → B → (C ∥ E) →
  D, with D's overlay after C6 so the TAA/pass-order lessons from C6 inform D2.
- Every group ends with: tests green, both backends captured and viewed, the log updated,
  `docs/goals.md` new goals marked `[x]` with their checks, and `docs/progress.md`'s current-
  state section refreshed (a short paragraph per group, in the established style).
- If a task's Check cannot be met (artifact can't be tamed, budget exceeded), the honest
  negative result + a written follow-up goal is a valid completion — the repo's logs are full
  of respected negative results. Do not silently descope.

## 6. Explicitly out of scope this pass (do not drift into them)

- Voxel editing / destruction (goal 160) and octree-backed collision (173).
- Incremental SVO rebuilds (158) and the growth-swap hitch (175).
- Path tracing / GI (the Lin-look arc is closed; reopening it is a new conversation).
- FFT/Tessendorf oceans, SWE, foam advection, currents, reef bathymetry physics.
- Imported tree/grass assets (PlantFactory/Megascans-style pipeline) — procedural only.
- Multiplayer anything.
- The mesh path gets parity only where a group says so (A via shared physics, B via
  `wind.fxh`, C7); it does not get v2 trees or the grass overlay.

## 7. When you are done

1. `research/gameplay-pass-log.md` — the full decision log with every measurement.
2. `docs/goals.md` — new groups (suggest AD "Player physics", AE "Wind", AF "Trees v2",
   AG "Grass", AH "Water motion") with every task `[x]` and its Check result recorded.
3. `docs/progress.md` — current-state section + numbers updated.
4. `Prompts/README.md` index row for this prompt marked complete (the side session will
   verify against the checks before archiving).
5. Full test suite green on both backends, CI green, `--verify-frame` above threshold on both
   backends, and at least one viewed capture per visual task — the standing methodology.
