# Goals

Living backlog — not a one-shot brief. Mark items done in place (`[x]`) rather than deleting them;
`docs/progress.md` is where a completed thread's *findings* get folded in once it's done. Groups are
independent unless a goal states a dependency; work them in whatever order fits a session, not
top-to-bottom.

## Standing methodology (applies across every group below, not restated per-goal)

**A rendering change is not verified by `--verify-frame`'s number alone, and not by a derivation on
paper — it's verified by actually looking at a captured frame.** This is not a style preference; it's
the direct, expensive lesson of the ribbon bug (`docs/progress.md`): a numeric threshold passed for
two entire "verified" runs while the frame was silhouette slivers. Every visual-affecting goal below
that says "view the dump" means: run with `VOXEL_DUMP_FRAME=<path>`, then actually open/view that
image file yourself before marking the goal done — you are running as Claude Fable with image-viewing
available in this environment, and Tony is explicitly not at the machine to eyeball it in your place.
A goal whose check is "verify-frame passes" without a viewed image is not fully checked.

**Every goal's "Check" is its acceptance criterion.** A goal that looks right without its check
performed is not done, matching the standard every prior brief in this project's history already
set — this document continues that standard, it doesn't relax it.

Table of contents: [A. Documentation migration](#a-documentation-migration) ·
[B. Visual self-verification infrastructure](#b-visual-self-verification-infrastructure) ·
[C. Ambient, AO & color (visual Stage 1)](#c-ambient-ao--color-visual-stage-1) ·
[D. Bloom & tone mapping (visual Stage 2)](#d-bloom--tone-mapping-visual-stage-2) ·
[E. Water, fog & foliage (visual Stage 3)](#e-water-fog--foliage-visual-stage-3) ·
[F. SSAO & G-buffer (visual Stage 4)](#f-ssao--g-buffer-visual-stage-4) ·
[G. Render architecture](#g-render-architecture) · [H. Code quality](#h-code-quality) ·
[I. CI hardening](#i-ci-hardening) · [J. Deferred-item cleanup](#j-deferred-item-cleanup) ·
[K. Gameplay completeness](#k-gameplay-completeness) · [L. Sky & atmosphere](#l-sky--atmosphere) ·
[M. Material palette expansion](#m-material-palette-expansion) · [N. Consolidation](#n-consolidation) ·
[O. Goals surfaced by this pass's own work](#o-goals-surfaced-by-this-passs-own-work) ·
[P. Modular block properties](#p-modular-block-properties) ·
[Q. Blocky/greedy meshing](#q-blockygreedy-meshing) ·
[R. Micro-voxel decorative objects](#r-micro-voxel-decorative-objects) ·
[S. Static, bounded world](#s-static-bounded-world) ·
[T. Storage compression, phased](#t-storage-compression-phased) ·
[U. Redesign consolidation](#u-redesign-consolidation) ·
[V. Chunk-generation load-time optimization](#v-chunk-generation-load-time-optimization) ·
[W. Sparse-brick octree core](#w-sparse-brick-octree-core-micro-voxel-pivot) ·
[X. GPU ray-marched renderer](#x-gpu-ray-marched-renderer) ·
[Y. Micro-voxel measurements & follow-ups](#y-micro-voxel-measurements--follow-ups) ·
[Z. Shading correctness & the Lin look](#z-shading-correctness--the-lin-look) ·
[AA. Body-vs-world collision](#aa-body-vs-world-collision) ·
[AB. The lag, measured](#ab-the-lag-measured) ·
[AC. Materials as components (done)](#ac-materials-as-components)

---

## A. Documentation migration

1. [x] Create `docs/progress.md` and `docs/goals.md` (this pair) in the repo. **Check**: both files
   exist at those paths, not the repo root.
2. [x] Delete `PROJECT_BRIEF.md`, `PHASE_1_BRIEF.md`, `M1_2_BRIEF.md`, `PHASE_1_COMPLETION_BRIEF.md`,
   `ENGINE_HARDENING_BRIEF.md` from the repo root. **Check**: `git status` shows exactly these five
   removed; `CLAUDE.md` and every file under `research/` are untouched — those aren't narrative
   briefs, they're operational reference and primary evidence respectively, and deleting them would
   destroy real value, not reduce clutter.
3. [x] Update `CLAUDE.md`'s own opening line (currently "Read [`PROJECT_BRIEF.md`]... for vision/
   architecture/phase roadmap") to point at `docs/progress.md` and `docs/goals.md` instead.
   **Check**: no remaining reference anywhere in the repo to a deleted filename (`grep -r` for each
   of the five names, other than inside `research/` findings that are historical citations).
4. [x] Update any cross-references inside `research/*.md` files that name a now-deleted brief by path
   (several do, e.g. "Subagent B from `PHASE_1_BRIEF.md` §9") — add a one-line note that the citation
   is historical rather than rewriting the research findings themselves. **Check**: the research
   files' actual findings are untouched; only a citation note is added.

## B. Visual self-verification infrastructure

Foundational — every visual-richness group below (C–F) depends on being able to actually look at a
result, per the standing methodology note.

5. [x] Add a PNG-writing path alongside (or instead of) `VOXEL_DUMP_FRAME`'s current PPM output — PPM
   is uncompressed and not universally viewable; PNG is directly openable by an image-viewing tool
   with no conversion step. **Check**: `VOXEL_DUMP_FRAME=out.png` produces a file that opens
   correctly when viewed directly.
6. [x] If a PNG encoder isn't already available transitively through an existing dependency, use the
   smallest reasonable option (a single-header encoder is proportionate here — this is a debug dump,
   not a shipping asset pipeline) rather than pulling in a general image library. **Check**: the
   encoder choice and why it was proportionate is written down.
7. [x] Add a `--dump-every N` debug flag: write a numbered frame dump every N frames during a run,
   instead of only once via `--verify-frame`'s single capture point — useful for watching a visual
   change settle over the first several seconds of streaming, not just the end state. **Check**: a
   short run with `--dump-every 30` produces a numbered sequence, each one individually viewable.
8. [x] Do one full pass right now, before any other visual work, viewing the current terrain frame from
   several camera angles (default start position, from ground level in walk mode, looking straight
   down) to establish a real baseline — not relying on the two screenshots from the original bug
   report, which predate the winding fix. **Check**: at least 3 viewed images, described in one
   sentence each in a scratch note, confirming what "correct, unimproved" currently looks like.
9. [x] Extend the debug overlay with a one-key screenshot-to-disk trigger (distinct from the exit-time
   `--verify-frame` dump) so a capture can be taken interactively during a longer `--autofly` run
   without restarting. **Check**: pressing the key during a live run produces a viewable file.

## C. Ambient, AO & color (visual Stage 1)

Cheapest, highest-impact-per-effort group — no new render pass, no new G-buffer, just changes to
data already computed at mesh-generation time and the existing lighting math. Do this group first
among the visual work.

10. [x] Design the baked voxel-neighbor AO scheme concretely: per the researched technique (0fps.net's
    "Ambient occlusion for Minecraft-like worlds," and thenumb.at's Exile pipeline note on the
    per-face-not-per-vertex subtlety), each of a quad's 4 corners gets its own AO value from its 3
    adjacent solid/air neighbor checks, stored **per-face-corner, not shared per-vertex** — a vertex
    shared by multiple quads can have a different AO contribution from each, and GPU barycentric
    interpolation is per-triangle, so sharing one AO value across all of a vertex's faces produces
    visibly wrong results at the very seams this technique is meant to soften. **Check**: this
    design note is written down before touching `mesh_extractor.cpp`, including which of the (up to)
    4 occlusion levels maps to which neighbor-count per the 0fps.net scheme.
11. [x] Extend `world::meshing::Vertex` (or the per-quad emission path) to carry an AO value per corner
    without breaking the existing compressed-vertex contract (`GpuVertexCompressed`'s frozen
    `static_assert`ed layout) — this likely means AO is a new packed field, not a change to the
    existing 12 bytes, given that layout is deliberately frozen. **Check**: the `static_assert`s in
    `pso_terrain.cpp` still compile after the change, updated deliberately if the layout genuinely
    grows, not left silently stale.
12. [x] Implement the AO computation in `mesh_extractor.cpp`'s existing padded cross-chunk sampling pass
    (it already resolves neighbor occupancy for the surface extraction itself — reuse that, don't
    add a second neighbor-resolution pass). **Check**: a standalone unit test on a hand-constructed
    small voxel arrangement with a known correct AO value at a known corner (a concave corner with 3
    solid neighbors should read the darkest of the 4 levels).
13. [x] Multiply AO into the pixel shader's final lit color (`terrain.psh.hlsl`), as a straightforward
    multiplicative term alongside the existing diffuse/ambient math. **Check**: view a dumped frame
    of a chunk boundary or a concave terrain feature (a valley, the inside of a slope) and confirm
    visible, correctly-shaped darkening — not just that the shader compiles.
14. [x] Replace the flat `0.25` ambient floor in `terrain.psh.hlsl` with a two-color hemisphere ambient
    term (a warm sky-tint color for upward-facing surface normals, a cooler/darker ground-bounce
    tint for downward-facing ones, lerped by `normal.y`) — the single biggest step away from "flat
    Lambert" per the research, and it composes directly with AO from goal 13 rather than replacing
    it. **Check**: view a dump; upward-facing terrain (hilltops) should read warmer/brighter than the
    underside of an overhang or a steep north-facing slope, distinctly from the old uniform floor.
15. [x] Tune the directional sun to a warmer color temperature (currently implicitly white — `PSOut.Color`
    multiplies albedo by a plain scalar diffuse term with no light color at all) — add an actual
    warm-white/golden `float3` sun color, not just intensity. **Check**: viewed dump shows a warm,
    not clinical-white, lit side on terrain.
16. [x] Add low-frequency, world-position-based noise color variation multiplied into each material's
    base albedo in the pixel shader (a second, cheap FastNoise2-adjacent noise sample, or a simple
    hash-based value noise if pulling FastNoise2 into shader-adjacent CPU precompute is more
    proportionate) — breaks up the current perfectly flat per-material color across a whole chunk.
    **Check**: view a dump of a large flat grass/stone area; color should read as naturally mottled,
    not uniform, at a scale that doesn't look like visible noise-grid artifacts.
17. [x] Re-verify `--verify-frame`'s threshold still makes sense once AO/ambient/color-variation land —
    the pixel-difference-from-sky-reference metric could shift meaningfully with real lighting
    changes. **Check**: re-measure the actual fraction on the standard scene and confirm 25% is still
    a meaningful bar, adjusting with written justification if not.
18. [x] Benchmark the pixel-shader cost delta from goals 13–16 combined (extra ALU per fragment: AO
    multiply, hemisphere lerp, noise sample) — cheap operations individually, worth confirming
    combined cost is still negligible rather than assumed. **Check**: a real before/after frame-time
    number from the existing overlay or a Tracy capture, not an assumption that "it's just a few ALU
    ops."
19. [x] Update `kMaterialColors` in `pso_terrain.cpp` to genuinely richer, more saturated base colors as
    a first pass — the current 6 are deliberately muted placeholders (confirmed by direct reading);
    richer base colors compound with goals 13–16 rather than fighting them. **Check**: view a dump
    side-by-side (before/after) and confirm the change reads as "richer," not "oversaturated/garish"
    — a subjective call, made by actually looking, not by picking hex values blind.
20. [x] Write up this group's combined before/after in one place (a short note, images referenced by
    path) — the first real "does it look like progress toward the goal" checkpoint, since Stage 1 is
    the foundation every later stage builds on.

## D. Bloom & tone mapping (visual Stage 2)

21. [x] Confirm DiligentFX's `Bloom` class is reachable from `render/diligent` given the current CMake
    linkage (it links `DiligentFX` already per the PBR/PostProcess research, but confirm the actual
    include path and library target resolve before writing integration code). **Check**: a trivial
    `#include` + type-exists compile check, isolated from the real integration.
22. [x] Integrate `Bloom` standalone per the researched pattern (`std::make_unique<Bloom>(device)`,
    `PrepareResources()`/`Execute()` each frame against the existing color-buffer SRV, composite
    `GetBloomTextureSRV()` back over the scene) — no `PostFXContext`/motion-vector dependency needed
    for Bloom specifically, per the research; don't pull that machinery in prematurely. **Check**:
    the integration compiles and runs without requiring a G-buffer or motion vectors this project
    doesn't have yet.
23. [x] Tune `BloomAttribs` (Intensity, Threshold, Softness, Radius) against real bright spots in the
    scene (sun-lit water highlights once goal 30+ exists, or bright sky/terrain edges meanwhile).
    **Check**: view a dump; bright areas should glow softly, not blow out the whole image or produce
    a barely-visible effect.
24. [x] Add a tone-mapping pass after Bloom compositing (DiligentFX ships tone-mapping shader utilities
    per prior research) — Bloom's HDR-ish glow needs a deliberate tone curve into the final LDR
    output, not a raw clamp. **Check**: viewed dump shows no harsh clipped-white blotches where Bloom
    is strong.
25. [x] Benchmark the frame-time cost of Bloom + tone mapping as its own pass, the same standard as goal
    18. **Check**: real before/after number, and an explicit note if it meaningfully changes the
    "worst-frame" number the overlay already tracks (Group T's own stutter-sensitivity standard).
26. [x] Re-verify `--verify-frame` against the Bloom-composited image — the reference-pixel-difference
    metric could behave differently against a post-processed frame. **Check**: same standard as
    goal 17.
27. [x] View a dump of the full Stage 1 + Stage 2 combination together (not each stage in isolation) —
    effects can interact in ways that aren't visible testing them one at a time. **Check**: one
    viewed image, one-sentence assessment of whether it reads as real progress toward "colorful,
    less flat," written down.

## E. Water, fog & foliage (visual Stage 3)

28. [x] Design the fresnel water term concretely: `F = F0 + (1-F0)*(1-dot(V,N))^5` (Schlick's
    approximation) mixing a reflection color (a cheap approximation — a fixed sky-tint or the
    existing hemisphere ambient's sky color, not full screen-space reflection) against refraction/
    transparency; deeper water (further below the surface at a given fragment) more opaque, shallow
    water near shore more transparent. **Check**: the formula and the depth-based opacity rule are
    written down before touching the shader.
29. [x] Water currently renders through the exact same opaque path as land (confirmed by direct reading
    of `terrain.psh.hlsl` — no alpha, no blend state) — this needs a real second material path, not
    a tweak to the existing one: either a second PSO with `BlendStateDesc` alpha blending enabled, or
    a forward-transparent pass after the opaque terrain pass. **Check**: the chosen approach (second
    PSO vs. second pass) is decided and written down, with the reasoning, before implementation.
30. [x] Implement water transparency + fresnel per goal 28's design. **Check**: view a dump directly over
    water — should show visible depth-based transparency and a fresnel brightening at grazing
    angles, not the current flat opaque blue.
31. [x] Add an animated ripple normal (a simple scrolling/combined sine or noise-based normal
    perturbation sampled per-fragment with time) so water isn't perfectly flat-shaded even when
    static-camera. **Check**: two dumps taken seconds apart show visibly different ripple pattern.
32. [x] Add a specular sun-glint term on water specifically (a Blinn-Phong or GGX-ish highlight from the
    directional sun, water's roughness being much lower than terrain's). **Check**: view a dump with
    the sun roughly behind the camera looking at water — a visible bright glint, not a flat diffuse
    water surface.
33. [x] Implement exponential-squared distance fog (`f = exp(-(d*density)^2)`, per Inigo Quilez's and the
    OpenGL EXP2 formulation from research), fog color tinted toward the current sky/clear color (or
    Group L's sky gradient once that exists) rather than a fixed gray. **Check**: view a dump with
    distant terrain visible — should show visible atmospheric recession, not a hard pop where
    geometry simply stops rendering at draw distance.
34. [x] Add height-based fog density falloff (denser near ground level, per Quilez's `d(y) = a*e^(-b*y)`
    form) as a refinement once flat-density fog (goal 33) is confirmed working. **Check**: view a
    dump from a hilltop looking down into a valley — valley floor should read hazier than the
    hilltop itself.
35. [x] Benchmark fog's cost (a cheap per-fragment exp, should be negligible) per the goal-18 standard.
    **Check**: real number, not assumed negligible.
36. [x] Design foliage variety beyond the current single box-trunk/octahedron-canopy tree shape: at
    least 2–3 additional silhouettes (a taller conifer-like shape, a shorter shrub/bush) using the
    same primitive-composition approach `tree_decoration.cpp` already established, selected
    deterministically per placement (the existing `placement_key` hash already gives a free,
    deterministic selector value to reuse). **Check**: the shape variants and the selection rule are
    written down before implementation.
37. [x] Implement the additional tree/object shapes from goal 36, reusing the existing deterministic
    placement, materials, and buffer-pool integration `tree_decoration.cpp` already has — this is
    variety in shape, not a new placement or rendering system. **Check**: view a dump of a forested
    area; visibly mixed silhouettes, not uniform copies of one shape.
38. [x] Add simple per-instance color jitter on Wood/Leaves materials (a small random hue/brightness
    offset per tree, seeded from the same deterministic placement key) so a forest doesn't read as
    perfectly uniform green. **Check**: view a dump of a dense tree cluster — visible natural color
    variation between individual trees.
39. [x] Add a simple wind sway animation on canopy/leaf vertices (a small time-based sinusoidal vertex
    offset, scaled by height-within-the-object so trunks stay still and canopies sway) — a real part
    of what makes Lin's foliage read as "alive" per the research. **Check**: two dumps taken seconds
    apart show visibly different canopy positions; trunk positions unchanged between them.
40. [x] Research and decide (a real research task, not assumed): does adding grass/flower ground-cover
    (small instanced billboards or cross-quads per the research, distinct from the tree system) fit
    this project's current scope, or is it a later addition given trees+terrain+water is already a
    substantial visual jump — write the decision down either way with reasoning, don't silently
    expand scope or silently skip it.
    **REOPENED AND ANSWERED THE OTHER WAY by Prompt 007 goals 338/339, and the reopening rule's note
    applies**: the original verdict — "needs instancing+textures" — was correct about a RASTER
    overlay and goal 339 is that overlay, which does need both. What changed is
    `research/grass-rendering-research.md`, which ranks "grass AS voxels in the SVO, finest levels
    only" as this engine's #2 option and shows it needs no new pipeline at all: the octree already
    does distance culling by construction and a voxel blade inherits the march's shadows, AO, grain
    and materials for free. So goal 338 shipped the voxel tier ON by default at +1.29% memory, and
    goal 339 shipped the raster tier present and OFF — not for its 0.21 ms of GPU but for a 10 ms
    synchronous instance rebuild. See Group AN-D.

## F. SSAO & G-buffer (visual Stage 4 — conditional)

Gated explicitly on the outcome of goal 41 — don't build the G-buffer speculatively.

41. [x] Decide, in writing, whether Stage 4 is worth its cost right now: SSAO needs a genuinely new
    world-space normal render target (a real architecture change, not a shader tweak) plus
    satisfying `PostFXContext`'s motion-vector input (a zeroed motion buffer works for a static-ish
    camera per the research, a real cost either way). Weigh this against Stage 1–3's much cheaper
    wins already covering AO-like darkening (goal 10–13's baked voxel AO already gives a real,
    cheap approximation of what SSAO would add in dense foliage). **Check**: a written go/no-go
    decision with the specific reasoning, before any G-buffer work starts.
42. If yes: add a world-space normal G-buffer render target alongside the existing color/depth
    targets — this is the real architecture change goal 41 is gating on. **Check**: the normal
    buffer's contents are correct, verified by viewing it directly as a false-color image (normals
    mapped to RGB), not just assumed correct because the pass compiles.
43. Wire `PostFXContext` with the new normal buffer, current+previous depth, and a zeroed motion-
    vector buffer (per the research's confirmed minimum viable setup for a project without real
    motion vectors yet). **Check**: `PostFXContext::Execute` runs without validation errors under
    `--validation`.
44. Integrate SSAO using the GTAO algorithm (`SSAO_ALGORITHM_GTAO`, the default/highest-quality
    option per the research) against the new G-buffer. **Check**: view a dump of dense foliage or a
    concave terrain feature — visible additional contact darkening beyond what baked voxel AO alone
    gives, not a flat, uniform darkening (which would indicate broken normals/depth feeding it).
45. Benchmark SSAO's real cost — it's the most expensive addition in this whole visual-richness arc
    per the research, and the goal-18 standard applies with extra weight here. **Check**: real
    before/after frame-time number; an explicit decision to keep, tune down (half-resolution/half-
    precision depth flags SSAO supports per its README), or revert if the cost isn't justified by
    the visible improvement over goal 44's baseline.
46. Re-verify `--verify-frame` and the worst-frame stutter metric once SSAO is live during active
    chunk streaming specifically (new chunks appearing mid-scene while SSAO samples a G-buffer that's
    also updating) — a real interaction goal 45's static-scene benchmark alone wouldn't catch.
    **Check**: an `--autofly` run with SSAO enabled, worst-frame number compared against the
    pre-SSAO baseline.
47. If goal 45/46's numbers don't justify SSAO's cost at this project's current scale: document the
    decision to disable/defer it behind a flag rather than removing the work entirely — the G-buffer
    and PostFXContext wiring are the expensive part and stay useful for other future post effects
    (SSR, TAA) even if SSAO itself isn't kept on by default yet.

## G. Render architecture

48. [x] Write down the actual multi-pass pipeline shape now that Stage 2–4 add real passes beyond the
    original single terrain PSO (opaque terrain+trees → transparent water → Bloom → tone map →
    optionally SSAO feeding back into the lighting) — a real architecture diagram or ordered list,
    not passes accreted ad hoc in whatever order they were implemented. **Check**: this document
    exists (in `docs/` or `research/`) before goal 49's refactor starts.
49. [x] Refactor `TerrainRenderer` (currently one PSO, one draw path per the code read) into whatever
    structure goal 48 specifies, keeping `render/interface`'s no-DiligentCore-types boundary intact
    throughout. **Check**: `render/interface` headers still contain zero DiligentCore includes after
    the refactor — a grep, not an assumption.
50. [x] `main.cpp`'s `run()` function is a single large function handling window/device setup, the whole
    per-frame update+render+overlay+report loop, and both exit-condition checks — genuinely
    functional and well-commented as read, but worth splitting into named phases (setup / per-frame
    update / per-frame render / shutdown-checks) as more systems (Stage 1–4's passes, gameplay
    goals) continue to land in it, before it grows further. **Check**: `run()`'s line count is
    reduced and each extracted phase is independently readable, with no behavior change (same test
    suite passes, same `--verify-frame`/`--autofly` results).
51. [x] Confirm the extracted structure from goal 50 doesn't regress the careful member-declaration-
    order discipline this project has twice had to learn the hard way (`docs/progress.md`'s hardening
    section) — any newly-introduced thread-owning member gets the same last-declared treatment.
    **Check**: an explicit review comment or test confirming destruction order, not assumed correct
    by inspection alone.
52. [x] Add a debug/dev toggle (compile-time or a CLI flag) to disable Stage 2–4's post-processing passes
    individually — useful for isolating which pass causes a regression during the visual-verification
    workflow goal 8/20/27 established, without needing to revert code. **Check**: each pass can be
    independently disabled and the frame dump reflects exactly that pass's absence.
53. [x] Re-run the full `benchmarks/` suite against the post-refactor render path and compare to the
    saved baselines in `benchmarks/baselines/` (the project's own established convention). **Check**:
    a new dated baseline file, compared via Benchmark's own `tools/compare.py`, not eyeballed.

## H. Code quality

54. [x] Full read-through of `world/meshing/src/mesh_extractor.cpp` specifically (the file that's had the
    most real bugs found in it historically — the boundary-vertex gap, the `NeighborCache`
    throughput fix) for anything else worth hardening now that AO (goal 12) is adding a second real
    piece of logic to the same padded-sampling pass — two independent concerns sharing one pass is
    worth a deliberate look, not assumed fine because each individually has tests. **Check**: a
    written note of what was reviewed and what (if anything) changed.
55. [x] Audit every `throw std::runtime_error` / defensive check added across this project's history
    (there are several, per direct reading — missing SRB variables, failed buffer/shader creation)
    for consistency: do they all get caught somewhere sensible (currently `main()`'s single
    `catch (const std::exception&)`), or would a more specific error path help diagnose a real
    failure faster. **Check**: a written assessment, changed only if a concrete improvement is found,
    not refactored for its own sake.
56. [x] Review `chunk_streaming.cpp`/`chunk_streamer.cpp` for the same "does this scale sensibly" question
    Group T's stutter work already asked of the upload path — now that Stage 1–4 add real per-vertex
    work (AO, color jitter, wind animation) to every chunk's mesh generation, re-confirm the
    generation-side job-time budget assumptions the streaming system's timing (unload delay, in-
    flight limits) were originally tuned against. **Check**: a real before/after generation-time
    number with Stage 1–3's additions included, not assumed unchanged from the original tuning.
57. [x] Check whether `world/chunk`'s `CoordMap`/`CoordSet` boost-backed aliases are used consistently
    everywhere a chunk/streaming coordinate gets stored, or whether any newer code (trees, the
    upcoming G-buffer/pass bookkeeping) introduced a fresh `std::unordered_map`/`std::map` instance
    that should go through the same hardened alias instead. **Check**: a grep for raw
    `std::unordered_map`/`std::map` outside the alias definition itself and outside genuinely
    unrelated uses, each one justified or migrated.
58. [x] Confirm `engine/events`' `entt::dispatcher` usage (chunk lifecycle events, the debug overlay's
    event-vs-poll consistency check) is the pattern reused for any new cross-system notification
    Stage 1–4 or the gameplay goals introduce, rather than a fresh ad hoc callback/polling mechanism
    reappearing. **Check**: any new cross-system notification added by this document's other goals
    goes through `engine::events::Dispatcher`, or a written reason why it doesn't.
59. [x] Review test coverage for gaps specifically in the NEW code this document adds (AO, water,
    fog, foliage variety, any G-buffer work) against the standard the rest of the codebase already
    sets (boundary cases, not just the convenient common case, per `docs/progress.md`'s own
    assessment of the existing suite). **Check**: each new subsystem has at least one test exercising
    a real boundary case, not only a happy-path smoke test.
60. [x] Confirm `.clang-format` (if one exists — check directly rather than assuming) is applied
    consistently across all newly-added files from this document's work; add one now if it doesn't
    exist yet, given the codebase's otherwise-consistent style is worth protecting as more people/
    sessions touch it. **Check**: a formatting pass runs clean (no diff) across the whole tree.
61. [x] Revisit whether `VOXEL_CLANG_TIDY`'s existing exclusions (test directories, per the root
    `CMakeLists.txt`'s own comment) still make sense given the new test surface from goal 59, and
    whether the `/EHsc` restatement workaround documented there is still needed on the current
    toolchain. **Check**: a real clang-tidy run against the current tree, findings triaged (fixed or
    explicitly suppressed with reasoning), not left unrun since it was last wired up.

## I. CI hardening

Grounded in this pass's own research into GitHub Actions gotchas for GPU-adjacent CMake+CPM
projects — the workflow file exists but has never actually run; this group is what makes that first
real run land clean instead of thrashing through avoidable failures one at a time.

62. [x] Split `.github/workflows/ci.yml` into a **core, no-GPU job** (matrix: Windows/Linux ×
    MSVC/GCC/Clang, `-DVOXEL_BUILD_RENDERER=OFF` per the flag this project already has) and a
    separate **renderer job**, rather than one job trying to build and test everything. **Check**:
    the core job runs and passes without ever fetching DiligentEngine/GLFW/Tracy.
63. [x] Add CPM/dependency caching via `actions/cache` keyed on `hashFiles('cmake/Dependencies.cmake')`
    (already the stated intent per `CLAUDE.md`'s dependency-additions note) with
    `-DCPM_SOURCE_CACHE=<cache-dir>` actually passed at configure time — confirm this is really wired
    into the workflow YAML, not just assumed because the intent was written down. **Check**: a second
    CI run on an unchanged `Dependencies.cmake` shows a cache hit (near-zero dependency re-fetch
    time), not a full re-clone.
64. Add ccache (or sccache) caching alongside the CPM cache — compile-artifact reuse is a different,
    complementary cache from the dependency-source cache. **Check**: a second run with only
    application-code changes (no dependency change) shows meaningfully faster compile times than a
    cold run.
65. [x] **Do not shallow-clone DiligentEngine.** It's pinned to a specific commit SHA (`aca2285`), not a
    branch tip — a shallow clone of an arbitrary SHA is a real, documented GitHub/git failure mode,
    not a hypothetical. Confirm the CPM fetch for DiligentEngine specifically does a full clone (or
    clones the branch then checks out the SHA), not `GIT_SHALLOW ON`. **Check**: the actual CPM
    package declaration for DiligentEngine is read directly and confirmed, not assumed safe.
66. [x] Add the renderer job's actual execution environment: **Mesa Lavapipe** (software Vulkan) on the
    Linux leg, **D3D12 WARP** on the Windows leg, since GitHub-hosted runners have no real GPU.
    **Check**: at least one renderer-job test (e.g. `voxel_app --frames 5`) runs to completion on
    both, or is explicitly marked skipped with a written reason if software rendering proves
    infeasible for a specific test.
67. [x] Treat software-Vulkan CI results as best-effort, not a guarantee of real-hardware parity (a real,
    documented risk per the research — Lavapipe/SwiftShader have known rough edges) — don't gate
    merges on renderer-job flakiness the same strictly as the core job. **Check**: the workflow's
    branch-protection expectations (if any) reflect this distinction explicitly.
68. [x] Add ASan+UBSan as one CI job (they combine safely) and **TSan as a separate job** (mutually
    exclusive with ASan/UBSan) — this project's own `CLAUDE.md` already documents TSan as locally
    unavailable on the dev machine by all three realistic paths, which makes a Linux CI TSan job the
    actual way this project ever gets TSan coverage, not an optional nice-to-have. **Check**: the
    TSan job actually runs (not just declared) and its result is reported, even if slower — budget
    for the researched 5–15× TSan slowdown rather than being surprised by CI runtime.
69. [x] Add clang-tidy as its own CI job using `CMAKE_EXPORT_COMPILE_COMMANDS` + `VOXEL_CLANG_TIDY`
    (both already exist in `CMakeLists.txt`), excluding `_deps`/third-party sources explicitly.
    **Check**: the job runs against real project sources only — confirmed by checking the actual
    file list clang-tidy processes in a run, not assumed correct because the exclusion flag is set.
70. [x] Keep the sanitizer and static-analysis jobs from blocking the fast core-job signal — run them in
    parallel, not as sequential gates (this project's own reference material already states this
    convention; confirm the actual workflow YAML follows it). **Check**: the workflow graph shows
    these as parallel jobs, not a chain.
71. [x] Do the actual first real run on a real GitHub runner (not a local `act` simulation, which
    doesn't catch every runner-environment difference) and fix whatever breaks — expect at least one
    real surprise given this has never executed for real; that's the point of doing it now rather
    than assuming the YAML is correct because it looks right. **Check**: a real, green (or explicitly
    triaged, not silently ignored) run, linked/referenced in `docs/progress.md` once done.
72. [x] Once green, add a CI status badge to whatever now serves as the project's top-level readme/intro
    (or `docs/progress.md` itself) — a cheap, standard signal that the pipeline this group hardened
    is actually being exercised going forward, not a one-time proof.

## J. Deferred-item cleanup

Named explicitly in `CLAUDE.md` as deferred, not forgotten — this group is where they get picked up.

73. In-app RenderDoc capture trigger: vendor `renderdoc_app.h` (the missing piece per `CLAUDE.md`)
    and wire the in-application capture-trigger API. **Check**: a hotkey during a live run produces a
    RenderDoc capture file, openable in the standalone RenderDoc UI.
74. [x] `tools/mesh_dump`'s `.obj` export (named as deferred from the very first phase of this project) —
    implement it now that the mesh format has stabilized through Stage 1's AO-data addition, so it's
    built against the current vertex shape rather than needing a second pass later. **Check**: a
    dumped `.obj` opens correctly in a separate viewer (Blender/MeshLab), matching the in-engine
    render of the same chunk.
75. [x] Revisit the RG16-normal-format A/B named in `CLAUDE.md` as "if slope banding ever bothers" — now
    that Stage 1's warmer lighting and hemisphere ambient (goal 14) make normal-quality issues more
    visually apparent than flat ambient did, actually check whether banding is now visible rather than
    leaving the question open indefinitely. **Check**: a viewed dump of a smooth slope under the new
    lighting, a specific yes/no on visible banding, and the A/B only actually run if the answer is
    yes.
76. [x] Confirm whether `backward-cpp` (researched, pinned, but not wired in per `CLAUDE.md`'s crash-
    handler section) is worth removing from `Dependencies.cmake` entirely now that the custom
    handler covers its use case, versus keeping it pinned as a documented fallback — a dependency
    that's fetched but unused either earns its keep with a clear reason or should go. **Check**: an
    explicit decision, written down, either way.
77. [x] Check whether `unordered_dense` (kept, per `CLAUDE.md`, "ONLY for the comparison harness" after
    losing the hash-map benchmark) is still needed now that the decision is made and documented in
    `docs/progress.md` — a losing candidate kept only for a comparison that already happened is a
    candidate for removal, not permanent residency. **Check**: same standard as goal 76.
78. [x] Re-run the full benchmark suite (`benchmarks/`) now that Stage 1–4's shader/mesh changes exist,
    and refresh `benchmarks/baselines/` with a new dated file per the project's own established
    convention — the existing baselines predate this document's work and comparing new numbers
    against them would be comparing against a stale baseline. **Check**: a new baseline file exists
    and is what future comparisons run against.

## K. Gameplay completeness

79. [x] Swimming: walk mode currently clamps to sea level (you stride on water, no swimming, per
    `CLAUDE.md`) — a deliberate v1 cut, worth revisiting now that water rendering (goal 30) is
    getting real attention anyway. Design: below sea level, gravity reduces/reverses and a simple
    buoyancy-toward-surface behavior replaces the ground-clamp. **Check**: a standalone test analogous
    to the existing ground-clamp tests (dropped from above water, settles near the surface rather
    than the seabed or the sky).
80. [x] Decide explicitly whether cave/overhang terrain (true 3D density rather than the current 2D-
    heightmap-derived occupancy) is in scope for "completing most of the game," given it's a real,
    previously-deferred scope expansion, not a small addition — heightmap-only terrain fundamentally
    cannot represent an overhang or a cave. **Check**: a written go/no-go with reasoning; if yes, this
    becomes its own future goals group rather than a single line item here, since it touches
    generation, meshing, AND streaming simultaneously.
    **REOPENED AND CLOSED THE OTHER WAY by Prompt 006 goal 313, per the reopening rule.** The
    original no-go was correct on its own evidence: full 3D density touches generation, meshing and
    streaming at once. What changed is that the research supplied a way not to pay that — Part 7
    §7.5's **heightfield-first hybrid**, `d = h - y + N3`, where the surface stays 2.5D and only a
    bounded band below it becomes 3D. The band is what made it affordable: `classify`'s
    solid-without-subdividing fast paths stay valid everywhere the band cannot reach, so the cost is
    confined instead of global (measured: +35% sampler time, and net **−35% bricks** because the
    new terrain is smoother). Caves ship, enterable, at 15.5 m mean passage width and 9.18 m height,
    with the 10,000-box classification safety check green.
81. [x] Biome variety beyond the current land/water/wood/leaves palette: at minimum a second terrain
    material (e.g. sand near shorelines, distinguished by height-relative-to-sea-level, reusing
    `HeightmapGenerator::height_at` the same way tree placement already does) — a concrete, bounded
    first step rather than a full biome system. **Check**: view a dump of a shoreline; a visibly
    distinct material band between water and grass/stone.
82. [x] Object count and variety in the debug overlay (currently a single "objects" number per
    `PHASE_1_COMPLETION_BRIEF.md`'s Group W legacy) — break it down by type once goal 36–37 adds
    real shape variety, so the overlay stays useful for understanding what's actually loaded.
    **Check**: overlay shows a per-shape-type breakdown, sums to the existing total.
83. [x] Review whether the spectator camera's fly-mode speed/boost values still feel right once terrain
    reads richer/slower-to-take-in visually (Stage 1–3's changes) — a subjective, but real, gameplay-
    feel question worth a deliberate look rather than leaving untouched by default. **Check**: a
    stated before/after assessment from actually flying through the changed terrain, not assumed
    unaffected by visual changes.
84. [x] Consider a minimal "look at a chunk's material composition" debug query (e.g. a crosshair-raycast
    reporting what material/chunk is under the camera's aim) — cheap, useful for the ongoing visual-
    verification workflow (confirming *which* material is producing an unexpected color, for
    instance) and for future gameplay (any interaction system would need this primitive anyway).
    **Check**: a standalone test against a known chunk's known material layout.
85. [x] Re-run `--autofly --walk` (the existing mechanical fall-through check) after every gameplay goal
    in this group, not just once at the end — swimming and biome changes both touch the ground-query
    path this check exercises. **Check**: `0 of N frames below ground`, re-confirmed after each
    change, not just once.
86. [x] Document the actual current gameplay loop honestly in `docs/progress.md` once this group's items
    land — "fly or walk around generated, streaming, forested terrain with water" is genuinely what
    exists; resist the temptation to describe it as more feature-complete than it is.
87. [x] Identify, concretely, what "a game" still needs beyond what exists after this document's other
    groups land (an objective, a failure/win condition, any player-facing UI beyond the debug
    overlay) — a real, honest gap list, not a rhetorical question, since "complete most of the game"
    was the framing this document responds to and that framing deserves a real answer.

## L. Sky & atmosphere

88. [x] Replace the current flat clear-color sky (confirmed by direct reading — no sky rendering pass
    exists, just a swap-chain clear) with a real sky — a simple analytic gradient (horizon-to-zenith
    color lerp, cheap, no new geometry) is the proportionate first step given the rest of this
    document's cost discipline. **Check**: view a dump; visible gradient from horizon to zenith,
    not a flat color.
89. [x] Tie the sky gradient's colors to the same warm-sun/hemisphere-ambient palette goal 14–15
    established, so sky and terrain lighting read as consistent rather than two independently-tuned
    color schemes. **Check**: a viewed dump where the terrain's lit highlights and the sky's horizon
    color visibly share a warm hue family, not clashing.
90. [x] Consider a simple sun disc/glow in the sky itself (a cheap analytic disc at the sun direction,
    reusing the existing directional light vector) — small addition, real payoff for the "golden
    hour" feeling the research points at. **Check**: view a dump with the sun direction roughly
    toward the camera; a visible, reasonably-positioned sun disc.
91. [x] Re-tie fog's color (goal 33) to the sky gradient (goal 88) rather than a separately-tuned fog
    color, once both exist — atmospheric perspective reads correctly only when fog and sky agree.
    **Check**: viewed dump shows fog blending smoothly into the horizon color at draw distance, not
    a visible seam between "fogged terrain" and "sky."
92. [x] Benchmark the sky pass's cost (should be negligible — a full-screen gradient, no per-object work)
    per the goal-18 standard. **Check**: real number.

## M. Material palette expansion

Distinct from Stage 1's lighting/AO work — this is about the number and range of materials
themselves, directly responsive to "colorful" as a materials question, not only a lighting one.

93. [x] Audit the current 6-entry `MaterialID` enum/palette for what's genuinely missing given goals 81
    (sand) and any biome decision from goal 80/81 — write a concrete target list (a handful of new
    materials, not an open-ended "add lots") before touching the frozen array-size `static_assert`.
    **Check**: the target list exists and is justified (each new material ties to a real terrain/
    gameplay feature, not added speculatively).
94. [x] Extend `kMaterialColors`/`g_MaterialColors[N]` to the new count, updating both files together and
    both `static_assert`s that freeze the current count of 6, per the existing "update both together"
    comment convention already in the code. **Check**: build succeeds, the `static_assert`s reflect
    the new real count, not silently left at 6.
95. [x] Re-run goal 16's color-variation noise against the expanded palette — more materials plus per-
    material variation compounds, worth confirming it still reads as natural rather than chaotic at
    the new material count. **Check**: view a dump of a scene showing several of the new materials
    together.
96. [x] Consider whether any of the new materials warrant their own shading tweak beyond the shared
    Lambertian+AO+hemisphere path (e.g. a material-specific specular/roughness value, even as a
    small per-material constant rather than a full PBR material system) — a real, bounded question,
    not an invitation to build a material system prematurely. **Check**: a written decision, and if
    yes, the specific materials and specific tweak, not an open-ended "make materials better."
97. [x] Update `docs/progress.md`'s architecture section once the palette count changes, so it stays an
    accurate current-state summary rather than silently drifting stale (the exact failure mode that
    made the original six-brief sprawl hard to navigate).

## N. Consolidation

98. [x] Full-suite regression run across everything this document touches — confirm the count only goes
    up from the last reported 70/70, with any net-new test count stated explicitly.
99. [x] Full visual review: view dumps covering every material, the sky, water, fog at distance, a dense
    forest, and a steep slope, in one sitting, as the real "does this read as progress toward
    colorful/John-Lin-adjacent" check — not each piece in isolation as it was built, which is how the
    original ribbon bug survived two "verified" passes. **Check**: a written one-paragraph honest
    assessment, referencing the specific viewed images, not a generic "looks good."
100. [x] Update `docs/progress.md`'s "Current state" section to reflect everything this document's groups
     actually landed — and explicitly mark anything from groups A–N that was decided *against*
     (goal 41's SSAO gate, goal 76/77's dependency removals, goal 80's cave decision) so a future
     session doesn't re-litigate a settled question from scratch.
101. [x] Re-open this same `docs/goals.md` file and add whatever new goals this pass's own work
     surfaced — the standing expectation for a living backlog, not a one-shot list that goes stale
     the moment it's first executed.
102. [x] Confirm every "Check" across groups A–N that was actually performed is genuinely reflected as
     done (`[x]`) — and, just as importantly, confirm nothing is marked done whose check wasn't
     actually performed, matching this document's own standing methodology note rather than treating
     it as aspirational.
103. [x] Write the equivalent of a "what problems does the code have now" honest note (mirroring
     `docs/progress.md`'s equivalent section for the pre-this-document state) — the same kind of
     direct, specific assessment this document opened by giving the *existing* code, now applied to
     what this document's own work added.
104. [x] One last full-scene view, from a genuinely fresh eye if possible — the single check that
     catches what an implementer, close to each individual change, is most likely to miss.

## O. Goals surfaced by this pass's own work (goal 101's standing expectation)

105. Hunt the floating-sliver artifact to ground with RenderDoc pixel history at the recorded
     repro pose (`research/water-foliage-design.md` "NEW ISSUE" -- full bisection state there;
     mesh data proven clean three ways, so the answer is GPU-side or view-geometry). Pairs with
     goal 73's vendored `renderdoc_app.h`. **Check**: the artifact's actual triangle/source
     identified, then fixed or explained.
106. Tune the water sun-glint field: at aligned view angles the near-water sparkle reads as a
     large overexposed patch with moire-like interference from the two-wave ripple normal
     (`research/captures/fresh_eye_seed42.png`, bottom). Candidates: lower glint gain, add a
     third incommensurate wave, or fade the Blinn term by distance. **Check**: viewed dump at the
     same pose reads as sparkle, not a white field.
107. Golden-hour sun option: lower the shared sun direction (sky_common.fxh) toward the horizon
     and re-balance sun/ambient -- unlocks the classic sun-path-on-water glint composition goal
     32's caveat records as geometrically impossible with today's near-zenith sun. **Check**:
     viewed sunset-ish dump with a water glint path.
108. Texture arc (the real next visual step): albedo textures or detail patterns per material
     (mid-slope soil terracing currently reads camo-busy at distance), which also re-opens goal
     41's SSAO gate and goal 40's ground-cover per their own written conditions. **Check**: its
     own goals group, written before implementation.
109. Prove the Lavapipe CI leg end-to-end green -- the MSVC-flag-leak fix (imgui_impl_glfw.cpp's
     COMPILE_OPTIONS) was necessary but not sufficient. Two real blockers found 2026-09-06 (CI run
     34059398741, after the Lin-look pass landed, unrelated to materials-as-components): (1)
     `device_init.cpp`'s Vulkan swapchain creation hardcodes `Win32NativeWindow` instead of the
     portable `Diligent::NativeWindow` typedef -- a real regression from that pass, fails to
     compile on GCC/Linux (`'Win32NativeWindow' does not name a type`); (2) deeper and pre-existing,
     unrelated to any recent pass: `crash_handler.cpp` unconditionally includes
     `<windows.h>`/`<dbghelp.h>` with no platform guard, and `app/CMakeLists.txt` unconditionally
     links `dbghelp` -- `voxel_app` has never actually been buildable on Linux. Fixing (1) alone
     would only trade this compile error for a link error at `dbghelp`; a real green run needs a
     Linux-native crash handler (signal handlers + backtrace -- CLAUDE.md's already-researched
     `backward-cpp` fallback is the candidate) before (1)'s fix is worth making. Deliberately not
     attempted in the same pass that found it: real feature work, unverifiable without a Linux
     machine, and this leg is `continue-on-error` by goal 67's own design (non-gating). **Check**:
     unchanged -- a best-effort-leg run reaching the Xvfb smoke, and goal 64's Linux compile-cache
     measurement from its second run.
110. Second-meaning audit for the AO vertex byte if a FOURTH meaning ever appears (occlusion /
     water depth / tree jitter today, each documented+tested): at that point widen the vertex to
     16B with a dedicated byte instead of packing further. **Check**: the written decision at
     that time cites this note.
111. Blender/MeshLab open-check for mesh_dump's .obj (goal 74's honest partial: the Blender MCP
     addon wasn't running). **Check**: a screenshot of the imported chunk matching the in-engine
     render.

---

## Voxel Representation Redesign (groups P–U)

A real architectural pivot, not a same-weight addition to A–O — full design rationale, sequencing,
and citations live in `research/voxel-representation-redesign.md`. Sequencing is dependency-real
(P before Q before S; R needs Q; T is explicitly phased and gated on S's real measured numbers) —
see that document's §8 before assuming these run top-to-bottom or all at once.

## P. Modular block properties

112. Design the full `BlockProperties` field set (§6.2's sketch plus anything §2's mesher or §3's
     generation-fill logic turns out to need — e.g. a `supports_growth` consumer in tree placement).
     **Check**: every currently-scattered `== MaterialID::X` comparison in the codebase (grep first)
     has a named property it will become.
113. Implement `world/chunk/include/world/chunk/block_type.hpp` per §6.2, `constexpr`, no runtime
     registry. **Check**: `static_assert`s confirm every `MaterialID` value has a table entry (a
     missing entry is a compile error, not a runtime surprise).
114. Migrate every scattered material-property comparison (swimming/buoyancy, ground-clamp, tree
     placement's growth-surface check, the shader's water/leaves special-casing) to read from
     `properties_of()`. **Check**: a grep confirms zero remaining bare `MaterialID::Water`-style
     comparisons outside the table itself and genuinely material-identity-specific code (rendering
     which color to draw, not which *behavior* applies).
115. Generate `pso_terrain.cpp`'s color buffer and `terrain.psh.hlsl`'s expectations from
     `kBlockTable` directly (a build-time or startup-time derivation) rather than a hand-duplicated
     parallel array. **Check**: changing one color in `kBlockTable` changes the rendered result with
     no second edit required anywhere else.
116. Full regression run. **Check**: same pass count as before this group, behavior unchanged for
     every existing material.

## Q. Blocky/greedy meshing

117. [x] Rewrite `world/meshing/mesh_extractor.cpp`'s core algorithm from Naive Surface Nets to
     per-voxel-face emission (naive visible-face culling first — simpler, and a real, valid
     stopping point on its own per the research). **Check**: a hand-constructed single-solid-voxel
     case produces exactly 6 correctly-wound outward faces (the same kind of golden-value test
     Group A's original meshing work used). Verified two ways: the golden-value test checks each
     face's cross-product-implied normal against its stored normal, not just vertex/index counts;
     `signed_volume_x6` (algorithm-independent) confirms the whole cube is consistently outward.
118. [x] Add greedy face-merging on top of 117 (per `block_mesh`'s documented approach: expand a face in
     each valid direction while neighbors share material and remain exposed). **Check**: a flat,
     uniform-material region of N×N voxels produces measurably fewer quads than the naive version —
     a real quad-count number, not assumed reduced. Measured on a real terrain chunk
     (`benchmarks/bench_mesh_extract.cpp`'s new counters): 802 merged quads vs. 6,144 naive exposed
     faces (interior-only) — an ~87% reduction. Merge key is (material, 4 corner AO/depth values)
     exactly, not material alone — see goal 123.
119. [x] Rebuild `test_surface_coverage.cpp`/`test_normal_continuity.cpp`/`test_sliver_hunt.cpp` against
     blocky meshing's actual correctness properties (exact face normals, no averaging question) —
     don't keep smooth-meshing-era tests that no longer test anything meaningful for this algorithm.
     All three rebuilt: coverage now checks column containment in a top-face's 2D footprint rather
     than vertex proximity (greedy merging leaves large flat runs with NO interior vertices at all,
     by design); normal-continuity now asserts every normal is an exact cardinal direction instead
     of "adjacent angles stay small" (blocky terrain SHOULD jump 90° at a real edge); sliver-hunt's
     own height check was corrected to sample the solid side of a boundary, not the boundary's own
     (possibly-short-neighbor's) coordinate — see goal 120's finding below.
120. [x] Re-verify the padded cross-chunk sampling / chunk-boundary logic (`docs/progress.md`'s own
     account of this being "the actual hard part" historically) still produces seamless boundaries
     under face-based meshing — the boundary-ownership rules differ from Surface Nets' dual-cell
     scheme and need re-deriving, not assumed to carry over. **Check**: the cross-chunk seam test
     from goal 119's rebuild, specifically exercised at a chunk boundary. New test: two solid voxels
     facing each other across a chunk boundary correctly block only their shared face (5 of 6 faces
     each, verified by normal, not just count). A real, non-bug finding surfaced along the way: the
     sliver-hunt test initially reported 1,396 "floating" triangles on real seed-1337 terrain —
     traced to the test itself, not the mesher: a riser's vertices sit exactly on a column boundary,
     and checking analytic height at that exact boundary coordinate can read the SHORT neighbor's
     height (58.49) instead of the TALL source column's (61.24, confirmed by sampling one step
     toward the solid interior) — a real ~3-voxel step in this terrain's fBm noise that Surface Nets
     never exposed this starkly since it never places a vertex cleanly on one side of a boundary.
121. [x] View a dumped frame of the new blocky terrain directly (the standing methodology from the last
     pass) — confirm it actually reads as "made of voxels" per the original complaint, not just that
     it compiles and renders something. **Check**: a viewed image, compared side by side with image 1
     from this conversation, with a one-sentence honest assessment. Viewed
     `research/captures/blocky_default.png` at the default seed-1337 spawn: the terrain now reads
     unambiguously as discrete stacked cubes — terraced mountain slopes, sharp step edges, angular
     cliffs — a real, striking departure from image 1's smooth rounded hills and a direct match for
     image 2's blocky aesthetic; no visible seams, cracks, or floating geometry in the capture.
122. [x] Benchmark meshing throughput before/after (Google Benchmark, the existing `bench_mesh_extract`
     harness) — face-based meshing has a different cost profile than Surface Nets' padded-sampling
     approach, worth a real number rather than assumed faster or slower. Real, honest result: ~5.4–
     6.8ms per extraction (run-to-run range) vs. the pre-Group-Q baseline's 3.07ms — roughly 2× more
     expensive, from the added per-face-corner AO sampling and the mask-based sweep's larger total
     sample count. Not optimized speculatively: extraction still runs on background worker threads
     (never blocks a frame), and Group S's goal 131 will re-measure real total generation time at
     world scale, where this cost actually matters and any optimization would be evidence-driven.
123. [x] Re-tune baked AO (§2.2) for the blocky context specifically — confirm the existing 4th-vertex-
     byte scheme still reads correctly per-face rather than assumed unaffected by the meshing change.
     Not a re-tune of an old approximation but the real thing: implemented the true per-face-corner
     0fps.net scheme goal 10 originally researched (each of a face's 4 corners gets its own 3-
     neighbor occlusion check), which Surface Nets could only approximate because it shared one
     vertex across several quads. `test_baked_ao.cpp` rebuilt against merge-aware bounding-region
     checks (a flat run's interior has no vertices at all once merged, so "nearest vertex to point
     P" no longer works) and confirms the same four design levels (0.55/0.70/0.85/1.0) still appear,
     darkest at the real concave corner. Water's depth-as-AO convention carries over unchanged
     (verified exact at the two pools' shared boundary, where the discontinuity is unsmoothed).

## R. Micro-voxel decorative objects

124. [x] Design the local micro-grid format for small decorative objects (size, e.g. 8³/16³; how it's
     authored — procedural per-object-type generator, not hand-authored per-instance). **Check**: the
     design is written down, including how it composes with `tree_decoration.cpp`'s existing
     deterministic placement rather than replacing it. Written up in full in
     `research/micro-voxel-object-design.md`: an 8³ `MicroGrid`, generated on demand and never
     persisted (same "derive at mesh time, throw away after" philosophy `tree_decoration.cpp`
     already uses for trees); its own placement pass (own file, own candidate density, own mask
     thresholds) runs alongside tree placement rather than inside it; meshed by a NEW small
     standalone face-culling+greedy mesher scoped to the grid's own size rather than paying Group
     Q's real 5-7ms full-chunk `extract_mesh` cost per tiny object. First concrete type specified:
     a berry cluster (Leaves blob + a new `MaterialID::Berry` on its surface).
125. Implement at least one micro-voxel decorative object type (a flower/berry cluster, per image 2)
     using 124's format, greedy-meshed via Group Q's mesher and appended into the chunk's mesh the
     same way trees already are. **Check**: view a dump; individually recognizable small cube
     clusters, matching image 2's read, not smooth geometry.
126. Confirm the memory cost of 125 is genuinely bounded (a handful of small grids per chunk, not a
     hidden global resolution increase) — measure it directly rather than assuming §1's "bounded and
     cheap" framing holds without checking.

## S. Static, bounded world

127. [x] Add `kWorldRadiusChunks` (or equivalent bounds) as a real, named config constant, set to the
     §3.2 trial size (48×48 columns) first — not 8km, not yet. `world/streaming/world_bounds.hpp`'s
     `kDefaultWorldBounds` (radius 48, y -3..2), also the new `--radius` flag's default.
128. [x] Replace `ChunkStreamer`'s per-tick desired-set/hysteresis logic with the one-time parallel
     generation pass from §3.3 — enqueue every in-bounds coordinate to the `ThreadPool` at startup.
     **Check**: every chunk in bounds is generated exactly once, verified by a count, not assumed.
     `WorldLoader::begin()` requests every coordinate from `chunks_in_bounds()` (real set) plus its
     1-chunk generation-only halo; `test_world_bounds.cpp` covers the pure shape query directly
     (exact count, no duplicates, both extreme corners reached), and a real run logged "world load:
     486 real chunks, 968 total (incl. halo)" at radius 4 -- matching (2*4+1)^2*6 and
     (2*5+1)^2*8 exactly.
129. [x] Remove `world::streaming::ChunkStreamer`'s now-dead spatial/temporal hysteresis code path
     deliberately (§3.4) — don't leave it inert and untested. **Check**: `git diff` shows real
     deletion, not a disabled-but-present code path; the removed tests are removed, not skipped.
     `chunk_streamer.hpp/.cpp` and `test_chunk_streamer.cpp` deleted outright (git rm); `world_streaming`
     is now a header-only INTERFACE target (world_bounds.hpp + chunk_events.hpp, `ChunkUnloaded`
     removed too -- nothing can ever fire it in a static world).
130. [x] Wire the loading-screen progress feedback from §5 to the generation pass's real completion
     count. **Check**: view a screenshot of the loading screen mid-generation — a real, moving
     progress indicator, not a static "Loading..." string. `DebugOverlay::render_loading` (ImGui
     progress bar, same overlay infra as the debug HUD); viewed two dumps of a real radius-20 load
     seconds apart -- "1968 / 10086 chunks" then "4794 / 10086 chunks" -- genuine, moving progress,
     not a placeholder string.
131. [x] Measure real wall-clock generation time and real memory footprint (Release build specifically,
     per §7's reframing) at the 48×48 trial size. **Check**: both numbers recorded in
     `docs/progress.md`, with the methodology (build config, machine) stated. Honest note on an
     ambiguity in §3.2's own phrasing, caught here rather than silently papered over: "48×48
     columns" reads as a SIDE dimension (matching how §3.2 separately computed "250 chunks per
     horizontal axis" -> 250² columns for the 8km case, i.e. 250 as a side, not a radius), but
     `kDefaultWorldBounds.radius_chunks` is a half-width -- so the constant actually set (48) builds
     a 97x97 world (2*48+1 per side), roughly 4x the literal 48x48 area. Kept as-is rather than
     changed to 24: the real measurement below is for the LARGER interpretation, a more
     conservative test than the literal reading would have given, and the result is good regardless.
     Real numbers, Release build (`--preset windows-release`, MSVC 14.51, RTX 4070 Laptop GPU, D3D12):
     **56,454 real chunks (97x97x6), 78,408 total incl. halo, loaded in 53.6s wall-clock; ~1.41 GB
     process memory** at completion (vs. 343 MiB of that being the actual voxel storage measured in
     goal 135 -- the rest is GPU mesh buffers, DiligentEngine/ImGui/Tracy runtime overhead, all
     normal Release-build weight, not evidence of a storage problem).
132. [x] Decide, from 131's real numbers, whether to scale `kWorldRadiusChunks` toward the original 8km
     ask, and by how much per step — re-measuring at each step per §3.2's explicit plan, not jumping
     straight to the final number. **Check**: each size step has its own recorded measurement. A
     second real measurement, not an extrapolation from one point: radius 72 (145x145x6 = 126,150
     real chunks) loaded in **125.8s, ~2.10 GB**. Per-chunk generation cost is near-constant across
     both points (0.95ms/chunk at radius 48, 1.00ms/chunk at radius 72 -- a mild, not alarming,
     super-linear tendency), and the fixed, non-chunk-proportional baseline (DiligentEngine/D3D12
     driver/ImGui/Tracy overhead) is real and large: fitting both memory points gives ~835 MB of
     fixed baseline plus ~9.8 KB/chunk marginal cost, not a flat per-chunk model.
     **Decision: do not scale toward the literal 8km ask.** Linearly extrapolating the measured
     ~1ms/chunk rate to an 8km-per-side world (side 250, i.e. radius ~125 -> ~378,000 chunks) gives
     roughly 380s (~6.3 minutes) of load time -- clearly unacceptable game-launch UX, and that is
     the OPTIMISTIC (pure-linear) case given the observed mild super-linear trend. The current
     default (radius 48 -> a real 97-column, ~3.1km-per-side world, 53.6s load) already delivers
     the actual thing asked for (a static, bounded, pregenerated world with no streaming lag,
     goal 133) at an acceptable load time, and is kept as the shipping default rather than pushed
     further. Reaching anything 8km-scale later is a real, separate future effort -- profiling
     where the ~1ms/chunk actually goes (noise sampling vs. the per-mesh-candidate snapshot copy
     vs. job-submission overhead) before assuming a bigger radius alone gets there, per this
     project's own "measure before optimizing" standard -- not attempted speculatively here.
133. [x] Re-run `--autofly`-equivalent movement through the now-static, fully-generated world and confirm
     the original stutter complaint is actually gone — frame time should show no generation-driven
     spikes at all post-load, since nothing generates during play anymore. **Check**: a worst-frame
     number from a full traverse of the loaded world, compared against the pre-redesign log's
     collapse-to-1fps behavior. Real run, radius 6, `--walk --autofly --verify-frame` together:
     "1014 / 1014 chunks loaded at exit, worst frame 38.4 ms over the whole run" -- exact chunk-count
     equality proves nothing loaded/unloaded mid-flight, and 38.4ms worst-case (~26fps floor, Debug
     build) is night-and-day from the pasted log's reported collapse to 1-2fps / 999ms frames.
134. [x] Full regression run once Groups P–S land together. **Check**: real pass count, stated explicitly.
     **76/76 tests pass** (Debug build, `ctest --preset windows-debug`) with all of Groups P-S's
     changes in place together -- block properties, blocky/greedy meshing, and the static-world
     loader rewrite, verified as one whole, not just as isolated per-group passes.

## T. Storage compression, phased

> **THE GATE HAS NOW BEEN MET, on the svo path, and this note says why (Prompt 004).** Goal 135
> gated Phases 2 and 3 on a measurement of the MESH world: 343 MiB, sitting still, comfortably
> inside any budget. That reasoning was correct and stays correct for the mesh path. **It does
> not transfer to the svo path, and the reason is that the quantity that mattered changed.**
> The svo world was 395-543 MB *re-uploaded whenever the camera moved 2 m* — a BANDWIDTH and
> LATENCY problem, not a footprint one, and 343 MiB of resident data is not the same kind of
> number as 543 MB crossing the bus every second.
>
> So Prompt 004 reopened it, and the phases map onto Group AK like this:
>
> - **Phase 1's analogue is done**: goal 258's per-brick palette, **543.68 -> 279.46 MB**
>   (-48.6%) for +1.5% of march on vk and no measurable change on d3d12.
> - **Phase 2's analogue — "skip storage entirely for what is known uniform" — was already
>   true** and is now also true per cell: a homogeneous brick collapses to a solid leaf and
>   never becomes a brick at all (which is why the palette probe found no 1-material bricks),
>   and goal 263's `kFlatCellEmpty` records a whole cell as resident-and-empty at zero storage.
> - **Phase 3 (SVDAG interning) is still not started, and now has a specific reason rather than
>   a gate**: goal 275d. Dedup interns identical subtrees, and goal 258 changed which subtrees
>   are bit-identical, so a palette-canonicalisation pass is a prerequisite.
>
> Goals 136 and 137 below therefore stay written against the MESH path, where their gate is
> still unmet and their reasoning still holds. Nothing about them is wrong; they are simply no
> longer the live question, because the live renderer is not the one they were written for.

135. [x] Measure Phase 1 (§4.2) directly: the existing per-chunk palette compression's real total memory
     across the full static world at the §3.2 trial size — this may already be small enough that
     Phase 2/3 are unnecessary, and that's a real, good outcome to confirm rather than assume needs
     more work. `benchmarks/measure_world_memory.cpp`, a one-shot report tool (not a repeatable
     timing benchmark -- there is nothing to time here) generating the full radius-48 world
     (real+halo) and summing each chunk's real `palette_size()`/`bits_per_voxel()`-derived byte
     cost. Real result: **56,454 real chunks = 326 MiB (6,055 bytes/chunk avg); 21,954 halo-only
     chunks = 17 MiB; 343 MiB total; 56.4% of all chunks are homogeneous (near-zero cost)**. This is
     the good outcome: Phase 1 alone comfortably fits any reasonable budget at this world size, so
     Phase 2 (136) and Phase 3 (137) stay correctly un-started, not because they were skipped but
     because the gate they're behind wasn't met. One honest, separate finding surfaced by this
     measurement: `WorldLoader` currently retains halo-only chunk voxel data forever (nothing ever
     calls `store_.erase()` on it after its one use satisfying a neighbor's meshing precondition,
     unlike the old streaming system's `sweep_generation_margin`) -- at 17 MiB this isn't worth
     fixing now, but it is real, measured waste, not assumed harmless.
136. Only if 135's number doesn't fit a reasonable budget: implement Phase 2's sparse chunk-grid
     (skip storage entirely for chunks known-uniform at generation time). **Check**: a real before/
     after memory number, same standard as every other optimization claim in this project's history.
137. Only if 136 still doesn't fit: scope Phase 3 (real SVO→SVDAG construction) as its own dedicated
     design pass — cite §4.1's real numbers as the starting evidence, and treat it with the same
     research depth this project gave the original rendering-API pivot, not a quick bolt-on. **Check**:
     this becomes its own goals group, written before implementation, matching goal 108's own
     precedent for "big enough to deserve its own group."

## U. Redesign consolidation

138. [x] Update `docs/progress.md`'s architecture section to reflect the post-redesign shape (blocky
     meshing, static world, the block-properties table) — the same discipline goal 97/100 already
     established for keeping that section accurate rather than stale. Full rewrite: current-state
     summary, architecture diagram (chunk/generation/meshing/streaming/render/app all updated),
     a new "Decisions that survived contact with evidence" subsection for this pass's own findings,
     and the honest-problems section re-examined (not just appended to) against what the redesign
     actually changed.
139. [x] Full visual review against both images from this conversation specifically — does the redesigned
     engine's own capture read closer to image 2's aesthetic than image 1 did. **Check**: a written,
     honest, specific comparison, not a generic "looks better." New `docs/progress.md` section,
     viewing `research/captures/baseline_default.png` (image 1's complaint made concrete) directly
     alongside `blocky_default.png`/`group_s_static_world.png`. Verdict, specific not generic:
     terrain now genuinely matches image 2's blocky aesthetic (a structural mesher change, not
     shading); small decorative objects (image 2's berries) do NOT yet, since Group Q never touched
     `tree_decoration.cpp`'s smooth primitives — named as an open gap, not glossed over.
140. [x] Write up what changed and why in one place for a future session that wasn't part of this
     conversation — the same "conclusions, not just a list of things touched" standard the last
     consolidation pass set. Added as `research/voxel-representation-redesign.md` §10 (a
     retrospective appended to the original pre-implementation design doc, mirroring how the last
     pass's `visual-stage-log.md` grew a consolidation entry) — per-group conclusions and the
     generalizable lessons (a tuning constant proven at one scale can fail silently at 30x scale;
     an algorithm's output-shape change invalidates a whole style of test, not just individual
     cases; a design doc's own claims about existing code still need grep-verification), plus a
     "start here if picking this up cold" pointer to progress.md -> this doc -> goals.md groups P-U.

## V. Chunk-generation load-time optimization

Real users hit a real problem the redesign pass's own Release-only measurements never surfaced:
launching `voxel_app` via Visual Studio's F5/debugger (the `windows-debug` preset — the only
"run it interactively" option that existed) made the loading screen take a very long time, far
beyond the documented 53.6s Release baseline. This is exactly the profiling work goal 132
flagged as undone ("where does ~1ms/chunk actually go"). Full methodology, root-cause detail,
and every real number: `research/chunk-generation-optimization-log.md`.

141. [x] Add real per-phase timing instrumentation to `WorldLoader` (generation vs. meshing
     CPU-time, summed across every worker thread), logged the moment loading finishes. **Check**:
     a real number, not a guess — directly answers goal 132's own flagged question. Landed as
     `WorldLoader::log_timings()`, called from `main.cpp` right after the existing "world ready"
     log line.
142. [x] Eliminate `consider_mesh_candidate`'s per-chunk deep-copy `ChunkStore` snapshot (carried
     over unchanged from the old per-tick streaming system, where it was a real, necessary safety
     measure against a live store under concurrent mutation) — provably unnecessary once a chunk's
     voxel data is frozen write-once, which it already is by the time this function runs.
     **Check**: real before/after end-to-end load time at the same radius-48 world, not a
     microbenchmark alone. Landed as `world::meshing::NeighborCache` moving from a private
     `mesh_extractor.cpp` implementation detail to a public header type with a
     `resolve(store, coord)` static helper `WorldLoader` calls once, on the main thread, handing
     the resulting 27-pointer array to the worker job by value.
143. [x] Fix `ChunkVoxels` palette-promotion thrashing during `fill_terrain`'s per-voxel loop
     (every bit-width boundary crossed during a fill re-packs the entire 32,768-voxel index
     buffer from scratch, up to three times redundantly for a chunk that introduces several
     materials in scattered order). **Check**: real before/after memory measurement
     (`measure_world_memory`), not just "should be fine" — a pre-widen trades memory for fewer
     repacks, and the trade needs a real number. Landed as `ChunkVoxels::reserve_bits(bits)`,
     called once from `fill_terrain` with `bits_for_palette_size(kMaterialCount)`.
144. [x] Add a `windows-relwithdebinfo` CMake preset so Visual Studio's own configuration dropdown
     has a fast option, not just Debug (measured ~80x slower for concurrent hash-map-heavy code,
     `_ITERATOR_DEBUG_LEVEL=2`) or Release (optimized, but not what VS defaults to). **Check**:
     verified against MSVC's own documented IDL-default rule and CMake's own documented
     RelWithDebInfo flags, not assumed safe — confirmed `/MD`+`/DNDEBUG` keeps IDL at 0. Explicitly
     bumped to `/Ob2` (CMake's own RelWithDebInfo default is the more conservative `/Ob1`) since
     this preset's whole purpose is speed, matching real precedent (OBS Studio's own
     RelWithDebInfo hardening did the same). Deliberately did NOT add `/fp:fast` despite it being
     Microsoft's own general games-optimization advice — this project's already-pinned
     `FastSIMD::FeatureSet` determinism guarantee (CLAUDE.md) would be put at risk by it.
145. [x] Real before/after measurement at the same radius-48/56,454-chunk world used throughout
     the redesign pass, on the same machine, Release build. **Check**: an actual run, not an
     estimate. **Real result: 53.6s -> 29.9s, a 44% reduction.** New breakdown: generation
     54.22s CPU-time/78,408 chunks (0.692ms/chunk), meshing 422.83s CPU-time/56,454 chunks
     (7.490ms/chunk) — meshing is now ~7.8x generation's cost, a genuinely new finding (see
     goal 147). A second real measurement, isolating goal 144's build-config fix specifically
     (both configs already carrying goals 142/143's fixes, same small radius-10/2,646-chunk world
     for both): **Debug 32.1s -> RelWithDebInfo 9.4s, a 3.4x reduction** — the direct answer to
     "launching via the app is slow." Generation improved ~13.7x per-chunk, meshing only ~3.3x —
     consistent with generation's hot path touching `std::pmr::vector` element access repeatedly
     (exactly what `_ITERATOR_DEBUG_LEVEL=2` taxes hardest) while meshing's `NeighborCache` already
     uses raw array indexing from an earlier pass.
146. Deferred, named explicitly, not implemented this pass: per-column heightmap redundancy —
     `generate_column_heights()` recomputes an identical 2D result for every chunk stacked at the
     same (x,z) column across the world's Y-range (up to 8x redundant). Real, standard pattern
     elsewhere (Veloren's `WorldSim`/`SimChunk`, Cuberite's `cHeiGenCache`) confirmed by research,
     but generation is already the smaller of the two phases by ~7.8x (goal 145's own numbers) —
     a smaller win than what's already landed. **Check** (for whoever picks this up): a real
     before/after generation-phase-only CPU-time number, using `log_timings()`'s already-landed
     instrumentation.
147. Deferred, named explicitly, not implemented this pass: meshing (including tree decoration) is
     now the dominant remaining cost at 7.490ms/chunk average. `docs/progress.md`'s existing
     "`extract_mesh` is ~2x more expensive, accepted because it's background-threaded and never
     blocks a frame" reasoning is true for per-frame streaming but was never evaluated for bulk
     upfront loading specifically, where the aggregate cost across every worker thread directly
     gates load time. A bigger, riskier change (the actual per-voxel-face AO-sampling hot loop)
     than this pass's scope. **Check**: its own dedicated profiling pass, isolating AO-sampling
     cost from base face-emission cost before proposing a fix — not a guess.

---

## Micro-voxel pivot (groups W–Y)

The request after Group V: "John Lin style sub-cm instead of blocks". The shipping 1 m greedy-mesh
world cannot get there (mesh size scales with the square of resolution — sub-cm is ~16,000x the
triangles), so the world representation and the renderer both changed: a sparse-brick octree with
distance LOD built in, rebuilt around the camera on a background thread, ray-marched on the GPU in
one fullscreen pass. The mesh path stays intact behind `--renderer mesh`. Full decision log and
every real number: `research/micro-voxel-pivot-log.md`. The research brief that prompted this was
written against the deprecated Rust/VoxelHex track; its techniques transfer, its library picks do
not (the brief's §2.2 "bricked SVO" was built here directly, in `world/svo`).

## W. Sparse-brick octree core (micro-voxel pivot)

148. [x] `world/svo` module: 8³ bricks (material bytes + 512-bit occupancy mask) under an
     SVDAG-layout octree (header word + one pointer per set child bit; kind + representative
     material in the header's spare bits), homogeneous boxes as single-word solid leaves, air
     absent. **Check**: layout round-trip tests; the flat `nodes`/`bricks` arrays ARE the GPU
     format (goal 151 uploads them verbatim).
149. [x] `TerrainSampler`: the existing `HeightmapGenerator` + `fill_terrain` banding generalized
     to meters, trees as implicit shapes shared with the mesh emitter (placement moved to
     `world/generation/tree_placement`). **Check**: byte-identical to `fill_terrain` at 1 m over
     117,600 voxels (`test_terrain_sampler.cpp`); box classification sound against dense sampling
     (6,000 boxes, with and without trees); trees voxelized at their placements.
150. [x] `build_tree`: parallel (32K subtree jobs merged deterministically — byte-identical to a
     serial build, tested), distance-LOD (full resolution within `lod_radius`, halving per
     doubling of distance; tested never coarser than the rule asks), with a sound height-field
     pyramid for empty-space/solid-interior skipping. **Check**: uniform-LOD tree equals the
     sampler at every voxel of a 64³ sphere; real terrain at 7.8 mm near the camera builds at full
     depth underfoot. **Real build cost, 256 m root at 7.8 mm, 16 threads: 12.4 s -> 1.5 s** over
     the pass (per-cell margins, tiered focus fields, slope from the field, the solid soil-band
     rule, deeper split — each measured, `research/micro-voxel-pivot-log.md` §2.4).
151. [x] `trace_ray`: the CPU reference marcher (integer-cell stepping, brick DDA, Laine-Karras
     LOD early-out) plus a brute-force finest-voxel oracle. **Check**: 0 mismatches over 7,000
     random rays on uniform and mixed-LOD trees (origins inside/outside, axis-aligned rays).
152. [x] `tools/svo_render`: whole frames with the reference marcher to PNG (dependency-free
     encoder), the same pose flags as the app, `--verify` applying the app's local-contrast metric.
     **Check**: a ctest (`svo_render_smoke`) in the GPU-less CI jobs; the first sub-cm image of
     this world was viewed from it before any GPU code existed.

## X. GPU ray-marched renderer

153. [x] `svo_march.psh.hlsl`: `trace_ray` ported statement for statement; fullscreen pass writing
     `SV_Depth` so bloom/tonemap/overlay compose unchanged; sky on miss; shading = the terrain
     pass's model + a traced sun-shadow ray + 4-ray short hemisphere AO (`--no-shadows`,
     `--no-ao`, `--no-lod-march`, `--lod-quality`). **Check**: `--verify-frame` **48.0%** on Vulkan
     AND D3D12 (FXC needed masked vector writes — X3500), dumps viewed and identical. First GPU
     frame was sky-only: the CPU reference at the same pose isolated it to Diligent's MUTABLE-once
     SRB rule (fixed with DYNAMIC variables).
154. [x] `SvoRenderer` (buffers, PSO, a STAGED upload — 32 MB per frame into pre-sized buffers,
     swapped in the frame the last slice lands, the previous tree released only once the GPU is
     done with it) and `SvoWorld` (background rebuild on a dedicated thread whenever the camera
     leaves the inner half of the finest ring). **Check**: `--walk --autofly --frames 600`: 0
     ground violations, 3 rebuilds during the run (0.60–0.70 s each, 42–57 ms staged uploads over
     7–8 frames); worst frame 61–80 ms with the first, synchronous upload → **38 ms** staged.
155. [x] `--renderer svo|mesh` (svo is the default now), svo flags (`--voxel-log2`,
     `--region-log2`, `--lod-radius`, `--no-trees`), overlay lines (voxel size, bricks/MB/nodes,
     build/upload times, rebuilding flag), loading screen, CI WARP smoke for the svo path.
     **Check**: world ready in **0.56 s** at the default (mesh path: 29.9 s); 155–159 fps at
     1280x720 panoramic (vsync-capped), 76 fps ground-level with shadows+AO.

## Y. Micro-voxel measurements & follow-ups

156. [x] Real numbers table (`research/micro-voxel-pivot-log.md` §4): bricks per LOD level, MB,
     build/upload times, fps by pose and feature. **Check**: every number from an actual run.
157. [x] **Done as Prompt 004 goal 258** -- a 3-bit index into an 8-entry palette rather than
     4-bit: the registry holds 8 materials, so 3 bits with entry 0 reserved for Air is *provably*
     sufficient instead of sufficient-in-sample, and a `static_assert` makes a ninth material a
     build error. **Check PERFORMED**: resident **543.68 -> 279.46 MB** at the same pose (-48.6%),
     march **+1.5% on vk and no measurable change on d3d12**, oracle 0/7,000, all seven goldens
     pass on both backends. Original entry: per-brick palette / 4-bit materials (brief §3.2's "cheap first" step): 2–4x on the ~350–400 MB
     the surface bricks take at the shipping default. **Check**: before/after tree bytes at the
     same pose, and no `--verify-frame`/oracle regression.
158. [x] **Done as Prompt 004 goal 257** -- per-cell rebuild over the cell grid, with detail
     quantised to a distance BAND so "unchanged" is decidable at all, together with goal 260's
     resident pools so an unchanged cell costs **zero upload bytes**. **Check PERFORMED**:
     rebuild **10.4x faster**. Original entry: incremental rebuild, reuse unchanged subtrees (far rings) and a persistent brick pool with
     partial uploads, instead of the whole-tree rebuild + 200–400 MB upload on every move.
     **Check**: rebuild wall-clock and upload bytes per camera step, before/after.
159. [x] Temporal AA (or supersampling) for the sub-pixel voxel shimmer 2–8 m out — visible moiré in
     every capture, the same reason John Lin's renderer needs TAA. **Check**: viewed capture pair.
     Done as goal 168 (Group Z), together with the averaged-normal blend that removes the moiré's
     lighting component.
160. Editing: the tree is rebuilt from an analytic sampler and never mutated; digging/placing needs
     a mutable structure (HashDAG is the researched shape, brief §2.2). Not started.
161. [x] **Done as Prompt 004 goal 252.** **Check PERFORMED**: the equivalence test now covers
     **196,608 voxels with 0 skipped**, where it previously excluded every negative-height column
     -- roughly a third of the region. Original entry: `fill_terrain` truncates the surface height toward zero (`static_cast<int32_t>`) instead of
     flooring, so underwater terrain sits one voxel higher than the geometric rule the sampler
     uses — a pre-existing quirk found by the equivalence test (which skips negative-height
     columns). **Check**: switch to `floor`, then the equivalence test covers every column.
162. [x] **Answered as Prompt 004 goal 259** (the shipping build's codegen decided, with LTO
     measured and rejected as pay-to-win that does not win here) and re-measured under goal 258:
     `fill_brick` CPU is **19.86 s summed across threads**, **+11.7%** after the palette, with
     **build wall clock unchanged** because the build is not throughput-bound on 3/4 of the
     hardware threads. Original entry: build-time profile is ~60% `fill_brick` (2.9 µs per sampled brick, ~2x sampled per kept)
     with the column grid cache hitting only ~9%: the remaining oversampling is horizontal, not the
     vertical stacks the cache was written for. **Check**: Tracy capture of one build before the
     next optimization, not another guess.
163. **STILL OPEN, deliberately -- see Prompt 004 goal 279.** Deferred for ORDERING, not value:
     dedup interns identical subtrees, and goal 258's palette changed which subtrees are
     bit-identical (two bricks holding the same materials in a different insertion order now get
     different palettes), so a canonicalisation pass is a prerequisite and measuring dedup before
     the payload was settled would have measured the wrong tree.
     Original entry: DAG deduplication as a measurement (brief §5.3, hash-interning): expected small on noise
     terrain; the brief's 16-identical-tiles test is the unit check.

## Lin-look, collision & lag pass (groups Z–AC)

The user's second round on the micro-voxel world, with three screenshots: shadow "circles" after a
rebuild and shadows that appear "when I stay still"; no collision ("I can't go through blocks or
the mountains"); lag "when I go close to mountains" that is "not the rendering"; a "swirly"
artifact on every slope; the look ("fine grains and smooth ... still a voxel but blends") vs. our
blocky cubes; and materials living in parallel tables with hardcoded offsets instead of
self-contained component files. Decision log with every measurement and the bisection tables:
`research/lin-look-log.md`.

## Z. Shading correctness & the Lin look

164. [x] **The shadow rings.** Secondary rays judged LOD by distance from the eye, so the early-out
     fired on the node containing their own origin whenever the camera was >2.4x farther from a
     surface than the tree's build center — a dark disc of self-shadow + self-occlusion around the
     previous build center, appearing when a rebuild lands and sliding with the camera. Secondary
     rays now judge LOD by distance from their own origin (`TraceParams::t_offset` stays 0), which
     cannot self-hit. **Check**: `svo_render --lod-center` 20 m off the camera, `--view lit` and
     `--view ao`: no disc (was: the nearest slope black); CPU shadowed-hit fraction 3.3% → 0.6%.
165. [x] **Layout v2 — a normal and a coverage per node.** Internal nodes and brick leaves carry
     an attribute word (int8 x3 area-weighted exposed-face normal + uint8 volume coverage) built
     bottom-up (`Brick::exposed_face_sum` row bit tricks; solid-vs-absent sibling faces at the
     parent). **Check**: sphere surface bricks within 35° of radial, root coverage 0.128 within
     1% of 33,510/262,144, oracle 0/7,000 mismatches on the new child-slot arithmetic; build time
     unchanged (0.57–0.78 s); +~3 MB on a 350 MB tree.
166. [x] **Staircase self-shadowing.** A slope of tangent s built from steps of any size shadows
     s/tan(sun elevation) of every tread (47% at 45° here) — terracing at coarse LOD, moiré at
     pixel-sized steps. Shadow origins lift one local cube along the averaged normal (AO half a
     cube); solid leaves get no lift (a water top IS its surface — lifting it along a parent's
     normal put origins inside the shore). **Check**: `lit` view flat on sun-facing slopes;
     D3D12 capture without the intermediate water checkerboard.
167. [x] **The swirl.** Shading normal = blend of the hit cube's face (weight 1 above 4.5 px
     projected size) and the averaged normal of the ancestor spanning ~6 px (`--smooth-pixels`);
     per-cube brightness grain (`--grain`, off under 1.5 px, full at 4 px) for the "fine grain".
     **Check**: `facenormal` view shows the staircase moiré, `normal` view does not; viewed
     captures on both backends (`research/captures/lin_*.png`).
168. [x] **Temporal AA** (`svo_taa.psh.hlsl`): Halton-jittered primary rays, hit distance written
     to a second target, reprojection through the previous view-projection (static world: no
     motion vectors), distance-mismatch rejection (2% + 5 cm), 3x3 clamp, 1/8 blend; `--no-taa`.
     **Check**: `--verify-frame` 34.2% on Vulkan and D3D12 (48% before counted the moiré);
     autofly worst frame unchanged; captures viewed.
169. [x] **The water checkerboard** (also in the user's screenshot). Every debug view uniform on the
     water; bisected by swapping `ShadeWater`'s return line and sampling one pixel row: the sun
     glint alone (`pow(., 256)` through a 3–7 m ripple lattice with a near-vertical half-vector).
     Broad dim highlight + half-meter noise. **Check**: row y=690: 105→208→170 became 122→146→140.
170. [x] **Debug views + GPU time**: `--debug-view lit|ao|normal|facenormal|level|steps|coverage|cubepx|smooth|lodcube|material|distance`
     (`svo_render --view`, same names; `--verify-frame` captures without judging them), a
     timestamp query (`gpu march+resolve` in the overlay and the 2 s log; Vulkan faults if the
     app's very first command is a timestamp — skipped for two frames). **Check**: each view
     attributed one bug above.
171. [x] **Coverage-aware secondary early-out**: an LOD node under 35% solid is descended, not hit,
     by shadow/AO rays (`TraceParams::lod_coverage_threshold`; primary rays keep 0 so silhouettes
     stay closed). **Check**: the blocky black patches near ridges in the `lit` view; oracle
     unaffected (threshold 0).

## AA. Body-vs-world collision

172. [x] `world/collision`: `SolidQuery` concept, `move_and_slide` (y/x/z, bisected, 0.25 m
     sub-steps so a 1.1 m boost frame cannot jump a 0.5 m trunk, 0.55 m step-up in walk mode,
     never traps a body that starts inside solid), `TerrainCollider` (the tree's own voxelization
     rule over a cached 16 m / 3.1 cm height grid rebuilt on a background thread + trunk boxes;
     independent of the renderer's LOD). Camera body 0.6 x 1.75 m, collision in fly and walk
     (`--noclip`). **Check**: 9 tests (walls, sliding, ledges, thin-wall tunneling, 40 random
     drops settle ≤11 cm above the footprint's highest column, a 5 m cliff walk, trunk yes /
     canopy no at both speeds); `--autofly --walk`: 0 ground violations.
173. [x] **DONE by goals 226/227** (Prompt 003 Group AJ-A). `world/collision/octree_collider.hpp`
     answers `SolidQuery` from the SAME `shared_ptr<const BrickTree>` the renderer marches, so
     collision agrees with what is drawn by construction rather than by re-deriving it. Not a
     "per-box point-sample near the build center" as sketched here -- an O(depth) integer-voxel
     octree walk with early-out, 7 nodes visited inside solid and 1 in open air, and `voxel_top`
     descends +y-first so it is genuinely 3D and survives Prompt 006's caves.
     **Check PERFORMED**, and it found more than the sketch expected: at uniform LOD the tree and
     the sampler agree over 10,000 random boxes in BOTH directions (0 disagreements), and under
     distance LOD there are genuine HOLES -- 0.41% region-wide -- but **zero within 8 m of the LOD
     centre**, which is where the body always is. See goal 226 for the distance-binned table and the
     consequence recorded there: the LOD centre must track the body.

## AB. The lag, measured

174. [x] A slow-frame attributor (every frame >20 ms logs upload / camera / render / post /
     overlay / present + uploading / swapped / building flags; exit summary by cause) and a
     900-frame walk A/B: 12 of 13 slow frames were `present` stalls while a build ran on all 16
     threads; 8 MB slices made it worse; 12 threads left one (the swap). Fixes: the build pool
     defaults to 3/4 of the hardware threads; the previous tree's GPU buffers are reused as a
     spare pair (25% growth headroom) instead of created per swap; the collider's first cache is
     built before the loop. **Check**: fly autofly 0 frames >20 ms (worst 16.9 ms, was 38);
     walk: 3, all one-time growth swaps (43 ms); GPU march+resolve 3.2–6.3 ms.
175. The growth swap: a tree that outgrows its spare buffers still pays a synchronous buffer
     creation (43 ms at 540 MB). **Check**: create on the build thread's own device context, or
     size spares from the last N trees; measured with the attributor.

## AC. Materials as components

176. [x] Every material is its own definition file (`world/materials/defs/<name>.hpp`, one struct
     of `static constexpr` members satisfying the `MaterialDefinition` concept) composed into a
     compile-time registry (`RegistryOf<Defs...>` in `materials.hpp`): one source for name, albedo,
     phase (gas/solid/liquid/foliage), shading model, liquid physics (the swim constants), the two
     tree-voxelization flags, and `fills()` (the terrain band it claims). **`MaterialID`'s
     enumerators are DERIVED from `Registry::index_of<Def>()`**, so ids are contiguous from 0 by
     construction and nothing hardcodes the order; `kMaterialCount` is `Registry::size`. Consumed
     by: the mesher (`is_occupied`/`is_liquid()`), the chunk fill and the svo sampler (one
     `terrain_material(TerrainQuery)` replacing the three hand-mirrored band rules and their three
     copies of `kBeachBand`/`kSoilDepth`/`kGrassMaxSlope`, now `TerrainBands`), the aim readout and
     `mesh_dump` (`name_of`, replacing two switch tables), the walk physics (Water's own
     `LiquidPhysics`), the tree-voxelization priority (`tree_replaces`, replacing the inline
     `!= Air && != Water || == Wood`), and BOTH renderers (one float4 record per material — rgb
     albedo + shading model in `.w` — uploaded to the terrain and svo palettes; the shaders size
     the array with a `MATERIAL_COUNT` macro and pick the water/foliage path via `MAT_SHADING_*`
     macros, both passed at shader creation from the registry — no `[8]`/`min(m,7u)`/`== 3u`/`== 5u`
     literal survives). **Check**: `world_materials_tests` proves the enum matches the composition,
     that exactly one component claims every terrain voxel over a grid, and that `terrain_material`
     reproduces both old band rules (the chunk path's integer one and the sampler's meters one) byte
     for byte; the full suite stays green on both backends and the `material`/`distance` svo debug
     views now render on the CPU tool too. Decision log: `research/materials-as-components.md` (it
     supersedes goal 113's written "constexpr table, no runtime registry" — still a constexpr table,
     now a composed one). Kept `constexpr`, zero runtime dispatch, as 113 required.

## Gameplay, wind and water (groups AD–AH)

`Prompts/001-2026-09-07-gameplay-physics-world-life.md`. The engine rendered a beautiful
sub-centimetre world you were a *spectator with a body* in: fly by default, walking minimal, no
jump, no crosshair, no wind, static water. Decision log with every measurement:
`research/gameplay-pass-log.md`.

Groups AD, AE, AH and goal 189 are DONE; AF is PARTIAL (skeleton, pipe model and leaf mass in;
sway and voxelizer not). Group AG (grass) is NOT started —
`research/gameplay-pass-log.md` §8 says exactly where the line is and what they build on, and their
goals below are left unchecked on purpose rather than descoped.

## AD. Player physics: a real controller

177. [x] Fixed-timestep simulation. `world/player`'s `FixedStepper` runs the sim at 60 Hz whatever
     the frame rate, so the jump apex, the coyote window and the smoothing time constant mean the
     same thing on every machine. Physics state lives in `PlayerState`, never in the frame loop's
     locals. Went in `world/` and not `app/` deliberately: `app/` is behind `VOXEL_BUILD_RENDERER`,
     so its tests never run in CI's gating no-GPU `core` job, and a state machine whose whole value
     is determinism is exactly what belongs there. **Check**: the accumulator conserves simulated
     time to 1e-8 at any cadence and two cadences agree to within the one tick rounding can defer
     (the first draft asserted exact tick equality and measured 11 vs 12 — 1/60 is not a binary
     fraction; log §3); `step_player` is bit-identical over 300 ticks whether they are delivered in
     one batch or in batches of 7/1/13/29/…; `--autofly --walk` 900 frames = 0 ground violations.
     That last one read **74** at first — `--autofly` teleported per FRAME, after the physics, and
     at 150 fps most frames run zero ticks. Moving the travel inside the tick fixed it; the
     fixed-step change revealed the harness bug rather than causing it.
178. [x] Jump with coyote time and buffering, on a real `Stance` {Grounded, Airborne, Swimming}
     state machine — coyote, buffering, head-bob and the landing dip all need last tick's stance,
     which a per-frame sweep result cannot give. Space is now a jump in walk mode (an EDGE, via
     `InputState::take_jump`, alongside its unchanged fly-mode level). **Check**: 8.5 m/s against
     -32 m/s² is a 1.129 m apex, inside the brief's 1.0–1.25 m band, pinned by a test so a gravity
     change cannot move it silently; coyote expiry, buffer consumption and "a held key jumps once
     in 240 ticks" each have their own test; the in-sim measured apex lands in the same band.
179. [x] Micro-step smoothing on the svo path. The step allowance is a *smoothing budget* there
     (0.04 m, ~5 voxels) rather than a ledge climb — at 7.8 mm voxels every natural slope is a
     sub-centimetre staircase, so the failure mode is micro-jitter, not blocked stairs. The mesh
     path keeps 0.55 m for its 1 m blocks. `--step-height M` overrides. The FEET stay exact; only
     the rendered eye lags (τ 0.1 s, hard-clamped, collapsed while airborne), so every mechanical
     check measures the same body with or without it. **Check**: the lag never exceeds the step
     budget over a 120-tick staircase climb and never exceeds the hard clamp under a teleport;
     `--autofly --walk` still 0 violations. Knowingly NOT captured as an image sequence — a 4 cm
     effect at a 0.1 s time constant is not something a still shows honestly, and the numeric
     assertion is strictly stronger (log §5).
180. [x] Crosshair + a richer aim readout. The crosshair is an ImGui foreground draw-list cross —
     no second fullscreen pass, because the overlay already runs AFTER the TAA resolve, which is
     the only thing the brief's fallback existed to solve. `aim_query` now sees TREES (a
     `TreeLookup` with a per-chunk-column placement cache) and reports hit DISTANCE; a trunk used
     to report as the hillside behind it. **Check**: viewed captures read "Stone @ 80,41,279
     (117 m)", "Water @ 74,0,219 (70 m)" (D3D12) and "Leaves @ 32,51,299 (129 m)" — the last a
     canopy the pre-A4 query could not see; new tests cover the trunk case, the canopy case, the
     distance, and the negative (without the tree source the same ray does not report Wood).
     Captures: `research/captures/gp_a4_*.png`.
181. [x] Swim through and climb ashore. Space/Ctrl are a direct vertical VELOCITY while submerged,
     not an acceleration — buoyancy at full submersion is +32 m/s² net, so any human-scale thrust
     would simply lose to it. Plus rlVoxel's liquid pop-up: blocked horizontally while swimming
     with air a probe-height above gets an upward impulse. **Check**: tests cover
     dive-against-buoyancy, the float equilibrium, surfacing, and the pop-up condition;
     `--autofly --walk` across water = 0 violations.
182. [x] View polish: head-bob keyed to distance walked (not wall-clock, so the pace is
     frame-rate independent), landing dip proportional to impact, boost FOV kick. All render-only,
     all off under `--verify-frame`/`--autofly`, killable with `--no-view-polish`. **Check**: a test
     drives absurd inputs (200 m/s impact, full speed, boosting, 600 ticks) and asserts the eye
     offset never exceeds 5 cm; disabled, it returns exactly zero and decays rather than freezing.

## AE. Wind: one shared field

183. [x] `world/wind` — `sample_wind(params, position, time)`, analytic and deterministic:
     constant direction, gust-varied magnitude (Ghost of Tsushima's model per the grass research),
     plus a flutter band for foliage surfaces. In `world/` and not `engine/` because `engine/` is
     infrastructure a different game reuses unchanged, and wind is *this world's weather*. Sines
     rather than FastNoise2 on purpose: the function has to exist twice, in C++ and HLSL, and
     produce the same numbers — a hash-based noise cannot promise that across two compilers, a sum
     of directional sines can, exactly. **Check**: gusts provably TRAVEL (what a point sees now is
     what the point upwind saw a moment ago, to 1e-4); pure-function determinism under reordered and
     interleaved evaluation; speed stays in the band the parameters describe and never reverses;
     flutter is bounded and spatially varying (neighbouring leaves are not in lockstep); no
     allocations.
184. [x] Wind constants as ONE source of truth, not two. `world/wind`'s own header holds the wave
     shape; `render/diligent/detail/wind_macros.hpp` compiles those same values into every
     wind-aware shader as `WIND_*` macros (the Group AC material-macro mechanism).
     `shaders/wind.fxh` contains no numbers of its own, so drift is *unrepresentable* rather than
     merely tested — the brief's CPU/GPU parity concern answered structurally. Per-run tuning goes
     through the constant buffer so `--wind-speed` works without recompiling a shader. **Check**: a
     shader naming a macro the C++ does not define fails to compile, which is the mechanism; both
     backends build and run.
185. [x] `--wind-speed M` / `--no-wind`, and the overlay shows the wind at the player, sampled on
     the renderer's OWN animation clock (`anim_seconds()`) so the number shown is the one being
     drawn. `--no-wind` sets `still_wind()`, which zeroes the FIELD rather than making each consumer
     test a flag. **Check**: overlay reads "wind: 5.6 m/s from 34 deg (gust -0.14)"; wind-on vs
     `--no-wind` frames diffed — 18,018 pixels change and **98.2% of them are foliage green**, so
     the effect lands on leaves and nothing else and `--no-wind` is provably static.

## AF. Trees v2: skeletons, pipe model, sway — PARTIAL (186-188 done; 190-192 not started)

PARTIAL, not descoped: the skeleton, the pipe model and leaf mass (186-188) are in and tested; the
sway oscillator, the skeleton voxelizer and the geometric canopy motion (190-192) are not started.
`research/gameplay-pass-log.md` §8 records what they build on (the wind field is complete and is
what they were going to consume) and the one non-obvious design question waiting for them.

186. [x] Space-colonization skeletons (`world/generation/tree_skeleton`), deterministic per
     (seed, position), with the existing three silhouettes plus a high-flutter aspen variant.
     Runions et al.: scatter attractors through the crown volume, let every tip with attractors near
     it grow one segment toward their average direction, consume the ones it reaches. Branching is
     not scripted -- it emerges when a tip's attractors pull in genuinely different directions, which
     is why the result looks grown rather than recursed. A bare stem is grown FIRST, because
     colonizing from the ground up branches at ground level and the tree has no trunk at all.
     **Check**: byte-identical skeletons for the same (seed, position, species) and different ones
     for a different seed; exactly one root with every parent earlier in the list (the invariant the
     pipe model's single backward pass depends on); segments one growth step long and joined
     end-to-start; the skeleton fits the crown volume it was given; tips spread rather than piled.
     Viewed dump of all four species before any voxelization, via the new `tools/tree_dump` (.obj
     line elements, rendered to `research/captures/gp_af_skeletons.png`): dome on a bare stem, cone,
     trunkless spreading shrub, tall narrow aspen -- each recognisably its species.
187. [x] Pipe-model radii from accumulated distal leaf area, in ONE tip-to-root pass. **Check**: a
     hand-built two-tip skeleton gets exactly the radius the model asks for and a root whose
     cross-sectional AREA is the sum of its children's; branch junctions satisfy da Vinci within 15%
     over 6 grown trees, excluding junctions where the twig floor clamped any participant. A
     calibration error was caught by the test rather than by inspection: `area_per_leaf_unit` was
     first set an order of magnitude too small (6e-5 against a real tree's ~5.7e-4 Huber value), so
     every radius fell under `min_radius`, the floor became the whole model, and the test reported a
     trunk exactly as thick as a twig.
188. [x] Leaf mass as LAI x crown footprint, shared over whatever tips grew, with the footprint
     measured from the skeleton's OWN horizontal extent. **Check**: LAI is exact by construction
     across 12 seeds, and the grown crown stays within 0.3-1.6x the crown it was asked for. This
     replaced a fixed-area-per-tip formulation that a strengthened test killed: tip count varies ~7x
     between seeds (34 at one, 245 at another), so the same species came out at LAI 0.3 or 8.7
     depending on the seed, and the original single-seed test had simply been lucky. Leaf area is a
     property of the crown; tip count is only how finely it is subdivided.
190. [x] **Closed by Prompt 007 goal 335 — see Group AN-C.** Hierarchical spring sway (trunk fundamental from the cantilever formula, branches
     semi-independent so multiple-resonance damping emerges structurally). **Check**: step response
     and resonance near the predicted f0 ≈ 0.26 Hz for sycamore-scale parameters; determinism;
     ≤ 0.5 ms/frame for all in-ring trees, measured with the attributor.
191. [x] **Closed by Prompt 007 goal 336 — see Group AN-C.** Skeleton-driven voxelization into the svo tree — capsule trunks, per-segment leaf clouds.
     **Check**: brick/MB growth measured BEFORE committing (the research names vegetation as the
     worst-case SVO content class); `--verify-frame` stays ≥ 25%; the terrain-sampler equivalence
     test still passes byte-for-byte; viewed captures at 2/10/60 m on both backends.
192. [x] **Closed by Prompt 007 goal 337, as an honest negative — see Group AN-C.** Geometric canopy motion in the marcher (C6.2's bounded experiment — domain-warping the
     sample position inside canopy bricks). The SHADING half (C6.1) is already done and shipped as
     part of goal 185's field; this is the half with no public prior art. **Check**: captures at
     2 m and 10 m, TAA ghosting evaluated in a slow pan, oracle still 0/7,000 (the warp applies at
     shading, not traversal). An honest negative result is an acceptable outcome here.

189. [x] Mesh-path wind parity (C7): `terrain.vsh.hlsl`'s two hand-picked sine waves are gone; it
     reads the same `wind.fxh` field, so `--wind-speed`/`--no-wind` mean one thing on both renderer
     paths. Foliage leans downwind in proportion to local wind speed (0.03 m per m/s ≈ the old
     hand-tuned amplitude at a 6 m/s breeze, so the mesh world's look is preserved) plus a small
     cross-wind flutter. Logged decision: the mesh path does NOT get v2 trees this pass.
     **Check**: wind-8 vs `--no-wind` frames diffed — 1,473 pixels change, 27.9% foliage green; the
     rest is terrain revealed and occluded behind moving canopy edges, which is the signature of
     GEOMETRIC displacement (a 98% green result there would have meant the displacement was not
     happening). Also fixed the mesh overlay's hardcoded `wind: 0.0 m/s`, caught by reading a
     capture rather than by a test. Captures: `research/captures/gp_c7_mesh_wind_{on,off}.png`.

## AG. Grass — NOT STARTED

Same status as AF. One design question is already answered in `research/gameplay-pass-log.md` §8:
D3 needs the Grass material to be wind-responsive, and Grass is `Shading::Lit`, not
`Shading::Foliage`; changing its shading model would wrongly give ground grass the mesh path's
canopy sway. The materials-as-components answer is a new `MaterialDef` member (`wind_responsive`)
exported to shaders alongside the shading model — not made, because making it without a consumer
would be speculative.

193. [x] **Closed by Prompt 007 goal 338 — see Group AN-D.** Deterministic blade-cluster placement voxelized into the finest LOD ring only, with a
     `GrassBlade` material (`Phase::Foliage`). **Check**: determinism; < +15% build time and < +10%
     tree MB at the default pose, or halve density and log the tradeoff; viewed captures at
     1/4/15 m; walk through it with no collision and no aim-readout lie.
194. [x] **Closed by Prompt 007 goal 339 — see Group AN-D.** Instanced raster grass overlay composed against the march's depth, with layered wind and a
     player-position bend. **Check**: correct occlusion both ways on a hillside; wind sweep visible
     in a sequence; walking bends it; frame cost measured and inside the 60 fps budget; both
     backends.
195. [x] **Closed by Prompt 007 goal 333 — see Group AN-B.** Distant grass tint — Grass-material hits modulated by the wind field, sharing the foliage
     shimmer's code. **Check**: viewed capture at 60 m across a valley; `--no-wind` kills it; debug
     views unaffected.

## AH. Water surface motion

196. [x] Gerstner displacement in the marcher, replacing the fixed ripple lattice. Four components,
     directions spread around the wind's, deep-water dispersion ω = √(gk) so a swell outruns the
     chop instead of the field sliding as one sheet. Wind and waves share ONE field, so `--no-wind`
     is glass by construction. The steepness budget is a CORRECTNESS constraint, not taste —
     Σ Q·k·A > 1 self-intersects the surface into visible loops. The field is DERIVED on the CPU and
     only summed on the GPU. **Check**: the budget test pins Σ Q·k·A at 0.6 for every wind speed
     0–25 m/s, not just the default; the analytic normal matches the real geometric normal of the
     displaced surface (dot > 0.999 over a grid); viewed captures at 0.5/3/8 m/s go glass → chop →
     long crest bands, on both backends; `--verify-frame` 34.7% (vk) / 34.6% (d3d12), unchanged;
     wind-8 vs `--no-wind` diffed — 52,078 pixels change, **98.5% water blue**. The first spectrum
     was wrong and a capture said so (all swell, no chop, flatter than the lattice it replaced); the
     fix was C++-only because the derivation lives on the CPU — log §9.
197. [x] Wave-aware swimming surface: `WorldSense::water_surface_y` is the same Gerstner sum the
     marcher draws, on the renderer's own animation clock, over genuinely submerged columns only.
     **Check**: `--autofly --walk` across water = 0 ground violations with the surface moving under
     the body.
198. [ ] Shore fade in the SHADER. `world/water::shore_fade` exists, is tested (the research's §2.3
     depth-vs-wavelength criterion), and the CPU uses it — but the marcher passes 1.0, because a
     probe ray straight down from the surface hits the water column's own voxels immediately, so a
     depth query needs a water-skipping traversal variant, and that is a change the 7,000-ray oracle
     guards. Note this implementation displaces NORMALS, not geometry, so the artefact E3 exists to
     prevent (crests clipping through sand) cannot occur; what is missing is only that shallow water
     should look calmer. **Check**: unchanged — a beach capture on both backends with the waves
     flattening at the waterline.
199. [ ] The physical water pipeline this pass deliberately did not start: shoaling, refraction,
     breaking criteria, foam advection, currents (water research §5.3/§9). A pass of its own
     magnitude. **Check**: to be defined by that pass.

## AI. The development harness (Prompt 002)

`research/dev-harness-log.md` has the reasoning and every measurement; `docs/dev-harness.md` is the
operational half. The four things measurement changed my mind about are in the log's §4 and §5.

### AI-A. One option layer

202. [x] `engine/cli`: an option is a `constexpr` row and parsing is one runtime loop over the
     table. The whole of the type erasure is one function pointer per row, produced by
     `bind<&Owner::member>()` (a PATH of member pointers, so a row can name
     `svo_settings.wind.base_speed` without a flat mirror struct). **Check PERFORMED**: 14 test
     cases, 80 assertions — every value kind round-trips; a Toggle row reaches one target from both
     spellings and last-one-wins; all three diagnostics name the option; a duplicate long name is a
     compile-time property (`has_unique_names`, `static_assert`ed at every real table, with the
     predicate pinned by `STATIC_CHECK` over three bad tables). Decided against a
     `std::variant`-per-row and against a template-parameter table — reasons in the log §1.
203. [x] `--help`, generated from the table. **Check PERFORMED**: a test asserts all 43 pre-port
     flag names appear (42 in a release build, where `--crash-test` is compiled out of the table by
     an `#if` inside the initializer list), and a second asserts every row and every alias appears —
     so an option cannot be added without documenting itself.
204. [x] `voxel_app` ported; the parse chain deleted. **Check PERFORMED**: 10 test cases assert
     `AppOptions` field-by-field against expectations written from the pre-port chain BEFORE
     deleting it, including the three awkward ones the prompt named (`--crosshair` defaults to
     `!verify_frame`, `--upload-budget 0` means unlimited, `--svo-threads 0` means three quarters of
     the hardware threads). AND: `voxel_app --frames 8` on vk and d3d12, pre-port
     (`C:/b/windows-release`, built 18:25 the same day) vs post-port — **the app's own log lines are
     byte-identical on both backends**. One awkward pair survives, named rather than renamed:
     `--grain` sets an amplitude, `--no-grain` removes the term.
205. [x] `svo_render`, `mesh_dump`, `tree_dump` ported; the drift ended. `--root-log2` is an ALIAS
     of `--region-log2` and `--verify` of `--verify-frame` — row properties, not second rows.
     **Check PERFORMED**: 16 documented command lines re-run, all exit 0 (the `svo_render` ctest
     line verbatim, `--lod-center`/`--view lit` from `research/lin-look-log.md`, `--xz`, all five
     kill switches, both dump tools' positional forms, all four `--help`s); 2 deliberate negatives
     exit 1; `mesh_dump`'s positional and named forms produce byte-identical `.obj`.
206. [x] `@file` response files. **Check PERFORMED**: a 12-flag command line and the config file
     that reproduces it yield a field-identical `AppOptions`; a nested `@file` is rejected naming
     the file and line; a missing file names the path.
207. [x] `SvoRenderer::Settings` independently constructible. **Check PERFORMED**:
     `app::settings_from_response_file()` and the app's full table produce equal Settings across all
     16 fields.

### AI-B. Scenarios

208. [x] `dev/scenario`: `Pose`, `InputFrame` (carrying `world::player::PlayerIntent`, NOT a second
     input vocabulary), the four segment kinds, capture points, assertions. **Check PERFORMED**:
     builds and tests under `-DVOXEL_BUILD_RENDERER=OFF`; a two-segment script produces the exact
     6-frame `InputFrame` sequence at 60 Hz; `jump_pressed` fires on **exactly one** tick of a 1.0 s
     `hold ... jump` (and twice for two such segments); a `look` lands exactly on its target through
     the app's own sensitivity and takes the near way around 350°→10°; `goto` is closed-loop
     (arrives in <100 ticks of a 600-tick budget) and gives up at its timeout.
209. [x] The `.scn` format. Line-oriented, not JSON, not a scripting language — reasons in the log
     §2. **Check PERFORMED**: every checked-in `.scn` parses, re-emits and re-parses to an EQUAL
     model; 11 malformed lines each report file, line number and what was expected; `include`
     resolves relative to the including file and rejects cycles **by resolved path** (a depth limit
     would name the wrong file).
210. [x] Self-registering built-ins. **Check PERFORMED**: a scenario registered from the test's own
     translation unit appears in the registry with no central list touched; `--list-scenarios` prints
     both kinds with their source; a file scenario shadows a built-in of the same name (the intended
     way to iterate on one without rebuilding).
211. [x] `voxel_harness`. `Session`, `run_svo` and `run_mesh` moved OUT of `main.cpp` into
     `app/src/app_run.cpp`, which the harness compiles directly — it does not copy the loop. The
     seam is `app::FrameInput`. **Check PERFORMED**: two runs of `spawn_stand` produce reports
     identical with the timing keys removed except `contrast_percent` (17.3197 vs 17.3107); a
     deliberately failing assertion exits 1 with metric, threshold and measured on one line.
     **Two honest negatives**: capture PNGs are NOT bytewise identical between runs, and headless
     and windowed captures are not either — isolated to the wall-clock ANIMATION phase, because two
     runs of a still pose with `--no-wind` differ by **0.000% of pixels** (max channel 4). See 218b.
212. [x] `--verify-frame`, `--autofly`, `--dump-every`, `--frames` all still work on `voxel_app`,
     and the harness reaches the same readback through the same `capture_phase`. **Check
     PERFORMED**: `voxel_app --frames 8` byte-identical pre/post (goal 204); the harness samples the
     same LOCAL-CONTRAST metric and reads 14.2–52.7% across the library.
213. [x] The starting library: ten `.scn` files, each with a paragraph saying what it is for.
     **Check PERFORMED**: all ten pass on **both backends, headless**; 34 goldens promoted; reports
     committed under `dev/baselines/2026-09-06-*.json`; the captures were VIEWED as a contact sheet
     — and looking at them is what found two real defects (the overlay baked into every golden, and
     `fly_transect` flying 1,920 m off the island).
214. [x] `ctest -L scenario` registers the three cheap headless scenarios; plain `ctest` still runs
     the unit tests and needs no GPU.

### AI-C. Reports and goldens

215. [x] `dev/telemetry`: `FramePhases` and the slow-frame attributor lifted out of `run_svo`'s
     function body. **Check PERFORMED, and it found a real gap TWICE.** The first attempt was
     circular (`wall_ms = phases.sum()`, coverage 100% by construction). With the clock's own number
     the tree-swap frame read **19.2 ms of phases against 205.4 ms of wall time**. The first
     hypothesis — `begin_frame` — was WRONG, measured ~0. An end-to-end probe of the loop body found
     it in `capture_phase`: a staging copy, a full `WaitForIdle` and a libpng encode, between two
     timers and covered by neither. Two phases added; coverage **99.7% → 99.9%**, frames outside
     ±1% **4 → 1**. `voxel_app`'s 2-second stats line and the harness's report now derive from the
     same object.
216. [x] `--report <path.json>`, hand-rolled emitter (the written case is in `json.hpp`; what would
     change the answer is named). **Check PERFORMED**: two runs differ only in timing fields — see
     211. **And the writer shipped BROKEN**: `key()` separated twice, so every key carried a leading
     comma, and the test passed because it checked brace balance and substrings. It now runs a real
     recursive-descent grammar validator, plus a guard on the guard (eight inputs it must reject).
217. [x] The golden metric, CALIBRATED. Mean absolute difference AND changed-pixel fraction — two
     numbers, because a mean hides a localized error and a count drowns in driver noise.
     **Check PERFORMED — the four measurements and the three conclusions:**

     | case | mean/255 | changed % | max chan |
     |---|---|---|---|
     | vk vs vk, TAA **on** | 0.7376 | 0.99609 | 212 |
     | vk vs vk, TAA **off** | 0.7226 | 0.34082 | 213 |
     | vk vs vk, `--grain 0.5` | 1.8007 | 7.19032 | 216 |
     | vk vs **d3d12** | 2.5481 | 12.76360 | 219 |

     Thresholds are the geometric midpoint of floor and signal: **1.2/255 and 1.5%**. Capture
     scenarios run `--no-taa`; per-backend goldens are mandatory (12.8% cross-backend is larger than
     a deliberate shading change); a failure writes a magenta-on-grey diff image, which was viewed.
     **Then the overlay came out** and the floor fell to **0.000–0.134%** — the thresholds now sit
     11–200× above it, which is margin rather than slack.
     **HONEST NEGATIVE, as the prompt anticipated**: a one-pixel change CANNOT be caught by any
     threshold over this metric. The floor is ~1,200 of 921,600 pixels; one pixel is 0.0001%.
     `max_channel_difference` cannot rescue it — it reads 212–219 even on a clean re-run.
     Decided against FLIP (a new dependency for "did this change" rather than "how bad does it
     look"); it is the thing to reach for if 218a is ever wanted.
218. [x] `--accept-golden`. Goldens live in `dev/goldens/<scenario>/<vk|d3d12>/`. **Check
     PERFORMED**: promoting and re-running gives PASS on all ten scenarios, both backends; a
     promotion over an existing golden PRINTS the distance it is about to erase and requires the
     flag.
218a. [ ] Cross-backend image comparison, deferred with its number: 12.8% of pixels differ between
     vk and d3d12 at the same pose. FLIP mean is the metric to try. **Check**: a threshold that
     separates "a different backend" from "a regression", or a written finding that none exists.
218b. [ ] A deterministic animation clock. `anim_seconds()` is wall-clock, so two runs of the same
     scenario show the water and foliage at different phases — the whole of the residual 0.03–0.13%
     run-to-run capture difference (isolated: `--no-wind` on a still pose gives 0.000%). Driving it
     from the tick count would make captures bit-reproducible. NOT done here because it changes what
     is drawn and Prompt 002 §6 puts renderer changes out of scope. **Check**: two runs of a moving
     scenario produce bytewise-identical captures.

### AI-D. Instrumentation that must not cost a frame

219. [x] `--ramp NAME:v1,v2,...` and the `throughput_ramp` scenario. A rung is a REAL run of a real
     scenario with one extra option, not a special measurement path. `mean_primary_steps` is read
     back out of the `steps` debug view, so it is the marcher's own count. **Check PERFORMED**: the
     table is in `research/dev-harness-log.md` §8b, with the monotonicity verdict and the first rung
     under 150 fps and under 60 fps. **AND THE RAMP FOUND A CRASH**: `--lod-radius 32` dies with an
     access violation inside `Builder::build_node` (`tree_builder_impl.hpp:166`) on every worker
     thread at once — see goal 219a.
219a. [ ] `build_tree` has no memory bound. At `--lod-radius 32` on a 512 m region it dies with an
     access violation rather than a diagnosable failure. A rung that crashes also takes the harness
     with it, because the harness IS the app in-process. **Check**: an out-of-memory build reports
     what it needed and how much it had, and the ramp survives a failed rung.
220. [x] Per-pass GPU ranges: march, TAA resolve, post, overlay, plus a whole-frame range. There is
     deliberately NO "present" range — Present is a queue operation and the profiling research's
     §5(b) is explicit that timestamps from different queues cannot be compared; a pair around it
     would measure the CPU submit, which the frame report already carries as a phase. **Check
     PERFORMED**: the ranges sum to **99.9% (vk) / 97.6% (d3d12)** of the whole-frame range on
     `stress_pose` — inside the 10% the prompt asked for. Their COST, three runs each way: mean
     frame time **6.14 ms with the timers on vs 6.15 ms off — 0.01 ms, 0.16%**, against a
     within-condition median spread of ±0.65 ms. They stay on by default. (The prompt asked to
     compare against 217's noise floor; that is an image metric, so the run-to-run frame-time spread
     is the comparable number and is what is reported.) The march is essentially the whole GPU
     frame: 5.00 of 5.11 ms on vk.
221. [x] Tracy, measured three ways rather than asserted. `VOXEL_TRACY=OFF` exists so "compiled out"
     is a real binary to compare against. **Check PERFORMED**: three median frame times in
     `research/dev-harness-log.md` §9.
222. [x] GPU counters: **Nsight Graphics, driven externally against a harness scenario — not the
     Perf SDK in-app.** Four reasons, in `docs/gpu-counters.md`: timestamps already answer the
     gating question; counter access is permission-gated on both machines that matter (the
     `ERR_NVGPUCTRPERM` gate), which is the opposite of where a build dependency earns its keep; it
     is a licensed dependency for a number read by one person on one machine; and occupancy is not
     actionable until Prompt 004's AK-E/AK-F. What would change the answer is named. **Check
     PERFORMED**: the reproducible recipe is in `docs/gpu-counters.md` with the exact `ngfx.exe`
     invocation against `stress_pose`.

### AI-E. CI and hygiene

223. [x] The harness runs headless in the Windows renderer job under WARP, **with `--no-golden`,
     deliberately**: WARP is a software rasteriser and the calibration measured 12.8% between two
     real backends, so a software rasteriser is further away than that. Taking WARP-specific goldens
     would mean a second reference set nobody looks at. The step asserts the non-image half — the
     scenario runs, the script drives the simulation, captures are written, every declared assertion
     holds — and a second step parses the report as JSON. **Check**: CI green with the step visible;
     no `branches:` filter added.
224. [x] Retired what the harness replaces. **Check PERFORMED**: `grep -rn "argv["` across
     `app tools dev benchmarks` finds **zero**; the whole repo has **exactly one** argv-indexing
     site, `engine/cli/src/parser.cpp:140`; hand-rolled `arg == "--..."` comparisons outside
     `engine/cli` number **zero**. `run_svo`'s local `FramePhases` is gone into `dev/telemetry`.
     `--verify-frame`/`--autofly`/`--dump-every` stay, as documented, as thin wrappers.

## AJ. Player embodiment (Prompt 003) -- IN PROGRESS

Reasoning and every measurement: `research/player-embodiment-log.md`.

### AJ-A. Collision that has no edge -- closes goal 173

225. [x] The CPU-side truth is a `shared_ptr<const BrickTree>` shared between the renderer's staged
     upload and the simulation -- option (a), and I ranked it first for the same reason the prompt
     did: it is the only option under which collision agrees with WHAT IS DRAWN, and BrickTree has
     been immutable-after-construction since the pivot, so sharing it is a shared_ptr and nothing
     else. (b) querying the sampler is kept as the tests' reference and IS more accurate near the
     camera, but it re-derives rather than shares, so a future sampler/tree divergence would become
     a collision bug. (c) keeping the analytic collider loses outright: a height field cannot answer
     for a cave, an overhang, or an edited voxel, and it is the thing that already failed.
     **Check PERFORMED**: implemented; the ranking and the losers' reasons are in the log §1.
226. [x] `world/collision/octree_collider`: `overlaps_solid(Aabb)` as an O(depth) octree walk with
     early-out, plus a genuinely 3D `voxel_top(x, z, yStart)`. Solidity is asked of `world/materials`
     (`is_solid()`), never by ID comparison. **Check PERFORMED**: 8 test cases -- exact answers on a
     single voxel (face-sharing, one-voxel-miss, half-open boundaries), an OVERHANG answered
     correctly from above and below (no one-surface-per-column assumption, so Prompt 006's caves
     cannot break it), water and leaves not solid because their components say so, and the walk
     visits **7 nodes inside solid / 1 in open air** rather than point-sampling.
     **The cross-check, with its direction:** at uniform LOD, **0 disagreements in 10,000 voxel
     boxes, both directions** -- the query IS the sampler. At the shipping distance LOD the
     disagreement is NOT merely conservative as I expected; there are genuine HOLES (tree says air,
     sampler says solid) at **0.41% over the whole region**. Binned by distance from the LOD centre:

     | distance | samples | holes | rate |
     |---|---|---|---|
     | 0-4 m | 54 | **0** | 0.000% |
     | 4-8 m | 410 | **0** | 0.000% |
     | 8-12 m | 1068 | 1 | 0.094% |
     | 12-16 m | 1978 | 5 | 0.253% |
     | 20-24 m | 4084 | 13 | 0.318% |
     | 28-32 m | 24224 | 116 | 0.479% |

     **Zero holes inside 8 m of the LOD centre**, which is where the body always is (the LOD centre
     IS the camera, and a rebuild is requested every 2 m). The consequence is recorded rather than
     left implicit: **the LOD centre must track the body**, and if a future change ever separates
     them this guarantee is gone.
227. [x] `SvoWorld::take_finished()` yields `shared_ptr<const BrickTree>`; `SvoRenderer::begin_upload`
     takes the handle; the app hands the same handle to the collider and bumps a generation counter.
     The swap is a plain member assignment on the main thread between ticks -- stated as such rather
     than reaching for an atomic that would advertise a contract that does not exist.
     `update_camera_phase` is now TEMPLATED on the query, so the mesh path keeps the analytic
     collider and the svo path gets the octree, both at zero overhead.
     **Check PERFORMED**: `fly_transect` (1483 frames of continuous motion across several rebuilds)
     reports 0 inside-solid events; 235/235 tests.
228. [x] The analytic backstop is GONE from `step_player`, and its replacement is a counter that
     logs the first offender with its position. The backstop now survives only for a query that
     declares itself an open world (`OpenWorld::open_world_tag`) -- `--noclip` and the
     collision-free tests, which would otherwise fall forever.
     **Check PERFORMED, AND THE COUNTER FIRED IMMEDIATELY**, which is the outcome this goal exists
     to produce. It found THREE distinct real bugs in a row, none of which had ever been visible:
     (1) `pose_ground` resolved against the ANALYTIC height while the body collides against the
     VOXELISED surface one voxel higher; (2) a point height cannot place a 0.6 m BOX -- on the
     31 degree slope at (48, 0) the uphill corner sits 0.19 m above the centre column; (3) even the
     footprint max misses by exactly one voxel, because the sampler samples each voxel's OWN min
     corner, so no point sample of `height_at` can predict a neighbouring column's voxel top
     (measured: feet 66.3438, uphill corner voxel top 66.3516, edge 0.0078). Spawning snapped up
     plus two voxels of clearance, with the body settling under gravity, takes `spawn_stand` from
     **181 of 181 ticks inside solid to 0**.
229. [x] The sweep's sub-step is derived from the body (`substep_for`), and `clip_stress` runs the
     speed sweep. `SweepParams::max_substep` now defaults to 0 meaning "derive it"; the old 0.25 m
     constant was correct against a 0.3 m half-width but for no stated reason, and silently wrong
     the moment anyone shrank the body. **The rule, stated**: the sweep tests END POSITIONS only,
     so with the boxes a distance `d` apart, `d < extent` makes consecutive boxes INTERSECT and
     their union a tube with no gaps -- tunnelling is then impossible for an obstacle of ANY
     thickness, down to one 7.8 mm voxel. Smallest extent covers every direction of travel; halving
     it leaves margin for the step-up's non-colinear boxes and for float. 0.30 m for the current
     body -- FEWER sub-steps than the constant it replaces.
     **Worst-case safe speed: there is none, and that is the point.** The sub-step count is
     `ceil(distance / d)`, unbounded, so speed buys sub-steps, not risk; what it buys is cost
     (goal 230). **Checks PERFORMED**: a body driven into a ONE-VOXEL-THICK (7.8 mm) wall over 1,
     5, 40 and 400 m in a single call is stopped and outside every time; `clip_stress` under
     `--ramp speed-scale:1,4,10,40` (10/40/100/400 m/s) reports **0 inside-solid ticks at every
     rung**. Cost: 0.051 / 0.059 / 0.17-0.28 / 0.95 ms per tick. The 40x rung fails a DIFFERENT
     check -- 220 walk violations -- because at 400 m/s the body crosses ~7 m per tick, far outside
     the 8 m radius in which goal 226 measured the LOD hole rate at zero; Prompt 004 closes that,
     nothing here can.
     One casualty worth recording: `1.5 / 0.25` is six binary-exact pieces and `1.5 / 0.3` is not,
     so an unblocked 1.5 m motion started summing to 1.499999762 and a Group A test that asserted
     exact equality failed. Unblocked axes now snap to the exact wanted delta, guarded by an
     epsilon and NOT by `!blocked` -- the step-up climbs on y with `wanted.y == 0`, and an
     unguarded snap silently deleted the 0.4 m climb (caught by its own test one build later).
229a. [x] **THE REAL BUG, FOUND AND FIXED -- and it was four bugs, of which the first suspect named
     here was right and the second was wrong.**
     (1) **The `started_inside` escape never escaped.** Attribution first: `StepResult` gained
     `started_inside` and the app two more counters, and one run settled it -- `761 ticks ended
     INSIDE solid, 761 of them started_inside, 0 stepped up`. Moving unblocked through solid ends
     every tick still inside, so one bad tick became a permanent state. Replaced with a bounded
     climb-out: probe upward in doubling steps to the body's own height, bisect, lift the least
     that works; past that bound the unblocked move remains, and still SAYS so.
     **`walk_hillside` 761 -> 0, `walk_shoreline` 236 -> 0.**
     (2) **The doubling ladder had a hole.** It reaches 1.024 m and the cap is 1.75 m, so a body
     buried 1.40 m (measured, `macro_ground`) was declared unrecoverable by an arithmetic accident.
     The cap is now probed explicitly; there is a test for exactly that gap.
     (3) **`macro_ground`'s pose was never legal.** "Camera 30 cm above the ground" cannot hold a
     1.7 m body, so under the walk default the scenario buried it 1.40 m and reported it every
     tick, from a scenario in which nothing moves. It is a CAMERA shot and now says so
     (`--fly --noclip`). The scenario set has both kinds and the distinction is now written down.
     (4) **The counter was measuring itself.** `clip_stress` at 4x still reported 358 events with
     ZERO started_inside and, with the step-up disabled entirely, still 358. The dump: feet
     **27.4765**, deepest corner voxel top **27.4766** -- embedded by **0.0001 m**, one tenth of
     the sweep's own 1 mm contact skin, against a 7.8 mm voxel. `SweepParams::skin` documents this
     exact float round trip and the sweep already tolerates it; the counter did not. It now asks
     about a box inset by the same skin. The ratio is what justifies it: artefacts 0.0001 m, real
     embeddings **0.048 m** -- 480x apart, with the 1 mm inset an order of magnitude clear of both.
     **Check PERFORMED**: `walk_hillside`, `walk_shoreline`, `spawn_stand`, `fly_transect`,
     `macro_ground` and `clip_stress` all green with `assert inside_solid == 0` in place, and
     `clip_stress` green at 1x, 4x, 10x AND 40x. 239/239 tests including the four scenario tests.
230. [x] Collision cost budgeted at <= 0.20 ms/tick. **MEASURED, per tick, in nanoseconds** -- a
     budget stated per tick cannot be inferred from a frame phase printed at 0.1 ms. First reading
     (before goal 232's speeds) was 0.051 ms/tick and looked like a clean pass. **Re-measured after
     232 it is a real, consistent MISS in one place, and chasing it killed two plausible stories.**
     Wrong hypothesis 1, sub-steps so cost scales with speed: falsified by a ramp that is NOT
     monotone -- `walk_shoreline` costs 0.040 at 0.2x speed, **0.226 at 1x**, 0.073 at 4x. Cost
     peaks in the middle because at 1x the body spends the run in the water, at 0.2x it never gets
     there, and at 4x it crosses in seconds.
     Wrong hypothesis 2, the bisection: a blocked axis ran all 12 halvings unconditionally, which is
     3 MICROMETRES of precision on a walking tick's 12 mm motion against a 7.8 mm voxel. Real waste,
     genuinely fixed (the search now stops under a 0.1 mm tolerance; the count is a ceiling, so a
     2.7 m boost-fly frame still gets 12). Measured gain: **~8%**. Nowhere near the explanation.
     What it is, from counters added to `OctreeCollider`: `walk_shoreline` issues the FEWEST queries
     (5.8/tick) and the DEEPEST (76.5 nodes each), and does **fewer total node visits than
     `walk_hillside` -- 444 against 701 -- at 3.5x the cost**. So neither queries nor nodes is the
     unit of cost. What separates them is what the query FINDS: `overlaps_solid` early-outs on the
     first solid voxel, and over water there is none, so the traversal runs to exhaustion through
     dense liquid brick leaves. **The expensive query is the one that finds nothing.**
     **Position**: budget met on land (0.026-0.116 ms/tick across the scenario set), missed over
     deep water (0.20-0.28). Not fixed here -- the fix is a "contains solid" summary bit on the node
     header so a water-only subtree is rejected at its root, which is a tree-layout change and
     belongs with Prompt 004's work on that layout. **Goal 276.**

### AJ-B. One body, always

231. [x] `PlayerState::mode` defaults to `MoveMode::Walk`. `--walk` is a documented no-op alias;
     `--fly`, `--noclip` and the `G` toggle sit behind ONE `--dev` door, as the prompt recommended.
     **Check PERFORMED**: `voxel_app --fly` without `--dev` REFUSES by name and exits non-zero
     rather than silently ignoring the flag. The harness sets `dev` itself, because a scenario is a
     developer context by definition. Six `test_spectator_camera` cases failed the moment the
     default changed and now state `mode = Fly` explicitly -- the change being visible rather than
     silent, which is the point.
232. [x] **Realistic scale, chosen, with every number from the research.** walk **1.4 m/s**
     (measured human 1.39, band 1.3-1.5), sprint **7.0 m/s** (fit-human band 6-8, NOT Bolt's 12.32),
     backpedal x0.75 and lateral x0.8 (ARMA's shipped values, inside the measured human 70-80%
     band, applied anisotropically to the wish direction rather than as four discrete states).
     `walk_speed` and `sprint_speed` are first-class m/s fields on `PlayerTuning`;
     `walk_speed_factor` is **deleted**, not deprecated, because the `move_speed x factor` product
     it named IS the thing that was wrong. `boost_factor` became `fly_boost_factor`: it applies to
     one mode now and says so. Fly keeps `move_speed` untouched.
     **The argument, recorded**: a 7.8 mm voxel world exists so you can see individual cubes; a body
     crossing it at 10 m/s makes them a blur. And 10 m/s was chosen by nobody -- it is a spectator
     default times a fudge factor. `research/player-movement-in-games.md` 5.7(a) makes the same
     recommendation for this engine by name.
     **Check PERFORMED**: a test asserts each speed is inside the band the log names AND that the
     walk is below the 1.89-2.16 m/s walk-run transition while the sprint is above it -- so a later
     "just bump it" has to move the evidence with it.
233. [x] **Earth gravity, deliberately, as ONE decision with the jump.** `gravity` -32.0 -> **-9.81**
     and `jump_speed` 8.5 -> **3.43**, apex 1.129 m -> **0.600 m**. Having just made the ground
     speeds real, a 3.26x gravity would be incoherent: the body would walk like a human and fall
     like a stone. Earth scale also HELPS collision, capping fall speed lower (60 m drop: 34 m/s
     against 62), and 60 m/s bodies are what stress the sweep.
     Departure named: a real standing vertical is 0.4-0.5 m at ~3 m/s takeoff, so 0.600 m is 20-50%
     high on purpose -- a 0.45 m jump does not read on screen -- while 3.43 m/s is within 15% of the
     human takeoff velocity, which is the half that governs how it looks.
     **Checks PERFORMED**: the apex test still pins `v0^2/(2|g|)`, retargeted to 0.600; a NEW test
     pins the FALL TIME from 5 m (1.010 s at Earth, 0.559 s at the old -32), so the choice is
     visible in two places. The integrated apex reads 0.6283 against the closed form's 0.6000 -- a
     4.7% overshoot because the jump impulse lands inside a tick that has already paid its gravity
     decrement. That was equally true before; at a 1.13 m apex it hid inside a 0.25 m band.
234. [x] **Sprint is a ramp, and the FOV follows speed rather than the key.** Ground acceleration
     **10 m/s^2** (braking 14: you can plant a foot), so `t = v/a` -- 0.70 s to sprint, 0.50 s back
     to a stop, numbers the tuning DECLARES rather than asymptotes.
     **DECIDED AGAINST**: the research's own critically-damped velocity spring. At sprint a
     0.25-0.5 s time constant implies a peak acceleration of v/tau = 14-28 m/s^2, above the
     10 m/s^2 the same document boxes as the human limit; a constant cap sits exactly on it.
     The FOV kick was keyed to `PlayerIntent::boost`, which with a ramp would snap the lens open a
     second before the body got there and hold it open while sprinting into a wall at zero speed.
     It is `clamp((speed - walk)/(sprint - walk), 0, 1)` now. The head-bob's full-scale speed came
     off the same fix -- it was a hard-coded `10.0f`, the old walking speed spelled a third time.
     NOT added, per the prompt: stamina, crouch, prone.
     **Checks PERFORMED**: tests assert the ramp reaches sprint in `sprint_speed/ground_accel` and
     releases in `sprint_speed/ground_decel` (and that release is faster); that it never overshoots
     at any dt including 4 s; and that TURNING at constant speed is charged the acceleration rate,
     not the braking rate -- otherwise strafing round a corner is grabbier than accelerating out of
     one for no reason anybody chose.
     NOT DONE from 234's Check list: the uphill-sprint slope relationship at 0 / 15 / 30 degrees.
     It depends on 235's slope handling, which is not built.
235. [x] **The walkable slope is 40 degrees, and above it the body slides.** The band it was chosen
     from, all recorded: Minetti's measurements span +/-45% grade (**+/-24.2 deg**) but that is a
     TREADMILL protocol's limit, not human capability; mountain paths optimise at 20-30% grade
     (11-17 deg), which is comfort not possibility; shipped engines default near 45 (Unreal 44.765,
     Source 45.57), a number that exists for level design with 45 deg ramps. 40 sits above every
     hill the generator makes and below every cliff.
     **The limit and the slide are ONE number**: the walkable limit IS the friction angle, so net
     downslope acceleration is `g(sin t - tan(limit) cos t)` -- exactly zero at the limit, growing
     above it. A separate limit and slide strength could disagree; this cannot. Capped at
     `max_slide_speed` (a slide is not faster than a run) and it BLEEDS OFF on walkable ground
     rather than stopping dead, so 41 deg -> 39 deg is not a wall.
     Three things changed together and the third is the load-bearing one: the uphill component of
     the wish is refused before the ramp sees it; **the step-up gets no budget on steep ground**
     (at 7.8 mm voxels every slope is a sub-centimetre staircase and a 4 cm step climbs ANY
     staircase -- leaving it on would walk straight up a face the limit had just refused); and the
     slope comes from the ANALYTIC field by central difference at 1 m, because the voxel surface's
     local slope is either 0 or 90 degrees and nothing between.
     **Checks PERFORMED**: the 20/30/40/50/60 ladder, each compared against the DECLARED limit
     rather than a literal; stability at 2 degrees either side held for 60 ticks with **zero** state
     changes; the friction-cone identity (exactly zero slide at the limit); the cap; the bleed-off;
     and that an airborne body is not sliding. Viewed captures:
     `research/captures/aj_slope_limit_refused.png` (the body pressed against a wall of cubes after
     sprinting at it for six seconds) and `aj_slope_limit_climbed.png` (the same scenario under
     `--max-walk-slope 89`). New scenario `walk_cliff`, in the ctest list.
     **AND THE FINDING THAT WAS NOT THE POINT**: `walk_hillside` is not a hillside. Along its own
     line the slope is **57-71 degrees** (25.88 -> 33.61 -> 39.91 -> 50.38 -> 61.87 m over sixteen
     metres, measured with `tools/svo_render --xz`), and the shoreline is 58.8. The body had been
     walking up it only because the step-up climbs staircases. **The current generator produces
     terrain that is mostly unwalkable at any realistic slope limit** -- a requirement for Prompt
     006, recorded so it is inherited rather than rediscovered.
     Side effect worth having: with no step-up on steep ground, `walk_hillside`'s collision cost fell
     from 0.061 to 0.040 ms/tick and its queries from 20.5 to 8.7 per tick.
236. [x] **Swimming re-checked against the moving surface -- two fixes, and the first was for the
     wrong bug.** `waterline_hold` stands at x=111.5 (ground 0.37 m, sea level 0) for 30 s and counts
     stance transitions. **Measured: 33**, against a 2.86 s dominant wave period -- about 10.5 crests,
     so roughly three flickers per crest. Real, and exactly what the prompt suspected.
     Fix 1, the one the prompt pointed at: hysteresis on `inWater`, with a physical reading -- you
     start swimming thigh-deep (0.6 m) and stop once only your shins are under (0.2 m), instead of
     crossing one threshold in both directions. **It moved nothing; the count went to 33.**
     So the counter was made to say WHICH transition, for the fourth time this pass:
     `grounded->airborne:16  airborne->grounded:16  airborne->swimming:1`. **Sixteen and sixteen, and
     exactly one involving water.** The waterline was never the cause: the shore there is a 58.8
     degree slope (goal 235's measurement), so the body was SLIDING, and a sliding body loses contact
     for a tick at a time. Every such tick read Airborne. The landing dip and the coyote timer both
     react to that, so it is a defect and not just a noisy counter.
     Fix 2, one line, following from 235's own model: **a body sliding down a face is in contact with
     it**, so a sliding tick the sweep did not ground keeps its Grounded stance. `result.sliding`
     already requires contact within coyote time, so sliding off the bottom of a cliff into real air
     still goes Airborne within 0.1 s.
     **33 -> 4**, and the four are DISTINCT one-offs (spawn settle, entering the sea), not a repeating
     pair -- the prompt's "not more than once per crest" met with a factor of 2.6 to spare. The
     hysteresis stays: it was a fix for a bug not yet firing, and it is why exactly one of the
     original 33 involved water instead of several.
     **Second check PERFORMED**: `swim_cycle` walks in, swims out, turns, swims back and climbs out
     under `--ramp wind-speed:0.5,3,8` (wind and waves share one direction and strength, so wind
     speed IS sea state). 11 / 11 / 9 stance transitions and **zero inside-solid events at every sea
     state**; the count does not grow with the waves, which it would if the surf drove the predicate.
     "Zero stuck frames" was operationalised as zero inside-solid plus zero walk violations plus a
     bounded stance count -- there is no stuck-detector in the harness, and saying what was actually
     measured beats inventing one for a single check.
     New metric `stance_changes` runs through scenario -> harness -> report; the app prints the
     transitions broken down by (from, to) beside the dominant wave period, because a flicker count
     is unreadable without one.

276. [ ] A "contains solid" summary bit on the octree node header, so `overlaps_solid` can reject a
     water-only or air-only subtree at its root instead of descending it to exhaustion. Goal 230
     measured the cost of not having one: collision over deep water is 0.20-0.28 ms/tick against a
     0.20 budget, while the same body on land is 0.026-0.116, and the discriminator is provably not
     query count or node count but whether the query finds anything. Tree-layout change; belongs
     with Prompt 004.
277. [ ] A hard-edged planar sliver appears over distant terrain on the SVO path at grazing angles
     -- captured at `research/captures/ajb_lod_sliver_fly_transect.png` (fly_transect's final frame,
     vk, 40+ m from the LOD centre). Pre-existing and unrelated to Group AJ (same terrain, different
     camera), and it reads like an LOD seam rather than the mesh path's known sliver curtains
     (goals 73/105). It is baked into that scenario's golden as current behaviour, deliberately --
     a golden's job is to detect change -- but it is a defect and it is written down here rather
     than accepted silently.

278. [ ] Restore goldens for MOVING captures once Prompt 004 removes the rebuild storm. Measured
     while re-taking every golden for AJ-B: two identical back-to-back runs differ by **0.0001-0.11%
     for captures taken AT REST and by 5.3-35.5% for captures taken after sustained motion**, against
     a 1.5% gate. Prompt 002 saw one instance (fly_orbit vk 70.4% while d3d12 was bit-identical) and
     treated it as a settling problem; it is not -- `walk_cliff` waits two seconds and still reads
     34.6%. At speed the body outruns the LOD rebuild, so which tree is resident when it stops
     decides what it sees, and that is wall-clock dependent.
     The moving goldens are **deleted rather than loosened**: a 40% gate detects nothing, and a check
     that cannot fail is the vacuous-check pattern this pass hit four times. Twenty golden files
     remain, all at rest; eight scenarios carry a GOLDEN POLICY note naming which of their captures
     are for eyeballing only.

### AJ-C. The head, per the vision research (237-240)

237. [x] **Auto-exposure, metered at the crosshair, pooled over ~6 degrees.** Built from nothing:
     there was no metering pass, no adaptation state and no exposure input to the tone curve.
     Reduction on the GPU (scene -> 64x64 -> 8x8), ADAPTATION on the CPU -- which looks backwards
     until you notice this goal's own check wants the exposure TRACE in the report, so the number has
     to reach the CPU regardless; doing the adaptation there removes a shader, removes a ping-pong
     pair of targets, and makes the two-timescale rule ordinary testable C++. Readback runs three
     frames behind through a fenced ring, so nothing stalls (~25 ms at 120 fps against 0.35-1.20 s
     time constants).
     **The mask is an ANGLE, not a pixel count** -- `tan(pooling half-angle)` and `tan(vFOV/2)` into
     the shader, weight `exp(-(tan t / tan t0)^2)`. At 3 degrees and a 70 degree vFOV the pool is
     7.5% of the half-height; a test pins that to a 5-10% band so a future FOV change cannot quietly
     turn it into a frame average.
     **Checks PERFORMED.** The asymmetry, from `exposure_sweep`'s trace: 2.1 s after a DARKENING step
     the adapted value lags the measured one by **0.259 EV**; 1.7 s after a BRIGHTENING step of the
     same size the lag is **0.001 EV**. Same magnitude, less time, already settled -- "fast up, slow
     down" doing what it is for.
     The A/B needed a sweep, because at the first pose the two metering modes differed by 0.037 EV --
     nothing. By pitch: -5 deg 0.165 EV, **-15 deg 0.547 EV**, -40 deg 0.500, -70 deg **-0.183**
     (crosshair BRIGHTER, the mask working the other way). At -15 -- half sky, crosshair on shadowed
     ground -- the frame-average build under-exposes what you are looking at by more than half a
     stop. Capture: `research/captures/aj_exposure_metering_ab.png`.
     **A correction recorded**: I looked at that pair and called them "nearly identical". They differ
     by 19% of mean level across 100% of pixels. A uniform level shift between side-by-side panels is
     exactly what the eye cannot judge.
     **THE KEY WAS A CATEGORY ERROR.** Shipped first at the photographic 0.18, which darkened every
     scene 28% (mean 136 -> 98) at a pose whose exposure should have been neutral. This renderer's
     values are authored artist colours near 0.5, not scaled radiance, so mapping them to an 18% grey
     is a global REGRADE wearing an exposure's clothes. The key is **0.36** = 2^-1.47, what this
     world's typical daylight pose actually meters at; the multiplier there is now **0.9857**, and
     auto-exposure became what it should be -- nothing at the reference scene, a real correction on
     departures. `macro_tree` (crosshair on sky at -0.541 EV) closes to 0.52x, golden moved 51/255
     across 100% of pixels, VIEWED and correct: look at the sky and the sky stops being blown out.
     Three bugs, each found by making something disagree: a startup crash with no stack from
     `psoCI.pPS = createShader(...)` binding a TEMPORARY RefCntAutoPtr that released the shader
     before PSO creation; a readback that silently never landed because the fence was signalled for
     the slot about to be overwritten and then checked against that same just-enqueued value; and
     `dt == 0` SNAPPING the exposure, caught by a test whose comment contradicted its own assertion
     (a zero time CONSTANT means "instant", a zero time STEP means "nothing happened").
238. [x] **Bloom behaves like veiling luminance.** Threshold becomes a fixed number of stops above
     the ADAPTED level (`2^(adapted + 1.6)`) rather than an absolute scene value -- that one line is
     the idea: the same absolute luminance blooms in a dark scene and not in a bright one. Intensity
     x(1 + 1.5*darkness) and Radius x(1 + 0.8*darkness) over a 6-stop ramp; the research gives the
     direction and the mechanism (a dilated pupil has a wider PSF) but not a coefficient, so both are
     chosen and both are written down.
     **The controlled experiment**: "the same bright source at two adaptation levels" cannot be
     staged by moving the camera, because that moves the source too. `--exposure-pin-ev` paired with
     `--exposure-key` holds the exposure MULTIPLIER constant (all four runs reported 0.1800) while
     the adaptation bloom sees changes, and bloom's own contribution is the difference against a
     `--no-bloom` run of the same configuration. Bright-adapted (0 EV): 0.274/255 energy over 0.26%
     of pixels, peak 8. Dark-adapted (-4 EV): **40.233/255 over 100% of pixels, peak 62** -- 147x the
     energy and a kernel that goes from a fringe to the whole frame.
     **AND THE HONEST NEGATIVE**: across the adaptation range this world ACTUALLY produces (-0.96 to
     -1.55 EV) the same measurement gives 0.258 and 0.170 with peaks of 5 and 2 -- both
     indistinguishable from no bloom, and the difference between them is noise. The reason predates
     this goal and is already in `post_process.cpp`: this renderer's HDR output rarely exceeds 1.0,
     so there is nothing bright enough to bloom. **A correct mechanism with no subject**, waiting on
     the water sun-glint that file already names as the intended first real emitter.
     **Check PERFORMED**: `--verify-frame` local contrast 27.75 (exposure on) vs 27.76 (off) on vk
     and 27.68 vs 27.69 on d3d12 -- 0.01 percentage points, far inside goal 217's noise floor. Not a
     vacuous comparison: the same pair of runs differs by 28% of mean level, so the metric really is
     insensitive to exposure rather than the toggle being ignored.
239. [x] Motion blur, lens ghosts and the luminance vignette are DECIDED AGAINST, in writing, each
     with its research section. **Check PERFORMED**: three entries in `docs/progress.md`'s
     decided-against list. No code change, which is the point -- the value is that the next pass does
     not spend a day on it.
     Motion blur: the eye smears during a gaze shift and DELETES the smear; the display's own
     persistence already supplies the pursuit smear; a shutter angle simulates a camera the player is
     not; and a controlled study measured no player-experience benefit. Lens ghosts: internal
     reflections between the elements of a MULTI-ELEMENT CAMERA LENS -- the eye has one lens and no
     aperture blades and cannot produce them. Vignette: acuity and contrast sensitivity fall off
     differently with eccentricity, so a LUMINANCE vignette is not an approximation of peripheral
     acuity at all, and it darkens exactly the region the periphery is specialised for, so it reads
     as tunnel vision rather than as reduced detail.
240. [x] View polish re-verified against the perception thresholds, with a captured landing sequence.
     Every constant now sits against a number from `human-movement-and-perception-research.md`
     Part 2 (vertical translation detected at ~2.13 cm/s; retinal slip costs acuity past ~4 deg/s).
     **Nothing was below threshold, so nothing was deleted**; three constants moved.
     `bob_frequency` 1.9 -> **1.36 cyc/m**: it is cycles per METRE, so at the old 10 m/s walk it ran
     at **19 Hz** -- a flicker, not a bob. 1.36 puts it at 1.9 Hz, the measured human step frequency.
     `bob_amplitude` 0.025 -> **0.020 m**, DERIVED: the constraint is that gaze perturbation while
     fixating 4 m ahead stays inside 4 deg/s, giving A <= 0.0234; the shipped value produced
     5.97 deg/s, past the acuity threshold and into the 6 deg/s degradation band.
     `landing_dip_max` 0.06 -> **0.045**: it EXCEEDED `polish_max_offset`, so the budget clamp
     truncated a hard landing and the two constants disagreed about what was allowed.
     **Check PERFORMED, and the sequence found two things a still could not.**
     (a) Peak render-only eye offset was **+0.1117 m** -- the eye 11 cm from the body, because
     `polish_max_offset` (0.05) and `eye_smooth_max_lag` (0.25) are independent clamps that stack to
     0.30 m, AND they pulled opposite ways: the dip pulled down 3.4 cm while the smoothing held up
     11 cm, so a landing read as the view FLOATING rather than absorbing. `eye_smooth_max_lag` is
     0.05 now and a landing zeroes the smoothing outright. Peak **0.1117 -> 0.0559 m**.
     (b) **The polish was switched off in every scenario.** With the terms reported separately it
     read polish +0.0000 on all ten frames: the gate was `!verify_frame`, and the harness sets
     verify_frame on every scenario (that is where the contrast metric comes from). The instrument
     built to photograph the polish photographed it turned off. Removing that term is checkable
     rather than hopeful -- the mechanical counters read `transform.position` and the polish is added
     to the camera COPY, so it cannot move what they measure.
     **The fifth instrument in this prompt to quietly measure nothing**, after `walk_violations`, the
     JSON brace check, the inside-solid counter, and `--speed-scale`. Roughly one per group, every
     one found by making the instrument disagree with something rather than by reading it.
     Strip: `research/captures/aj_landing_strip.png`; scenario `landing_strip`; the per-capture eye
     offset is logged split into polish and smoothing.
     Two workflow defects fixed on the way: **`capture ... no-golden`** is now part of the .scn
     grammar (round-trips through `emit_scenario`, beats `--accept-golden`) because goal 278's policy
     was a comment plus a manual `rm` that the tool silently undid twice; and the scenario ctest
     tests take **`RESOURCE_LOCK gpu`** after `valley_far` failed its golden under `ctest -j 2` and
     then passed three standalone runs at 0.002%.

### AJ-D. What the player can see of themselves (241-243)

241. [x] The aim query asks the OCTREE -- `world::svo::trace_ray`, the same traversal the body
     collides against and the shader mirrors -- with the LOD early-out and smoothing OFF, because the
     readout wants the voxel that is there, not the cube a distant pixel is shaded with. The analytic
     march stays for the mesh path and as the tests' second opinion.
     **Check PERFORMED: 1272 / 1272 material matches over 2,000 random rays, 0 mismatches** -- and
     the three wrong answers before it are each named, because none was a defect in the query.
     27 mismatches probing a quarter voxel ALONG THE RAY: all Stone-vs-Dirt or Dirt-vs-Air at brick
     level, which are VERTICALLY ADJACENT bands -- on a shallow ray a quarter voxel of travel crosses
     into the next voxel down, so the check sampled a different voxel from the one hit. Probing along
     the HIT FACE NORMAL: 2. Snapping to the voxel CENTRE: still 2, which ruled out rounding. Those
     last two printed **t = 0.0000** -- the ray STARTED INSIDE SOLID, where the tracer reports the
     origin's own voxel and its normal is a default. Test origins are uniform over a region so some
     are underground; a real crosshair's origin is the camera, which goal 228's counter asserts is
     not inside solid. Second case: octree vs analytic straight down over 92 land columns, 92 agreed.
242. [x] The readout's range is **34 m**, from `human-eye-and-vision-research.md` Part 1 8.1: a 1 cm
     detail is resolvable to 34 m at 20/20 (1 arcmin MAR). **The criterion**: 1 cm is the scale of the
     detail that distinguishes one material from another here -- a 7.8 mm voxel -- so beyond 34 m,
     naming the material is a claim the eye cannot check. It replaces 300 m, a round number nobody
     derived. Rejected, with reasons: 54 m (same detail at the 0.64' 94-ppd ceiling -- the best
     measured eye rather than the nominal one; `--aim-range` exists for it), 619 m (an 18 cm face as
     a BLOB -- detecting something is there is not identifying what it is made of), 1719 m (a 0.5 m
     trunk, same objection further out).
     **Check PERFORMED**: a test re-derives both distances from `d = s / tan(MAR)` rather than
     trusting the constants, and asserts the readout goes blank past its range while the same column
     still hits at 300 m -- so "blank" is demonstrably a range limit, not a missing surface.
243. [x] A visible player body is OUT OF SCOPE for this pass, recorded in the decided-against list:
     no model, no self-shadow, no first-person arms, each a content decision the owner has not asked
     for. The engineering groundwork is noted so it is not re-derived: the collision box is
     **0.6 x 1.75 m** with the eye at 1.7 m, it is what the sweep moves and what goal 228's counter
     watches. The one open question named: a model's feet follow the PHYSICAL eye, not the smoothed
     one, since goal 240 establishes the smoothing as render-only.


## AK. Frame time and the GPU architecture (Prompt 004)

Every measurement, every rejected alternative and every vacuous instrument caught on the way:
`research/frame-time-and-gpu-architecture-log.md`. The architecture it arrived at:
`docs/gpu-architecture.md`.

The prompt's premise was *"76 fps at 13.15 ms with only 3.2–6.3 ms of GPU march means roughly half
the frame is not the marcher — find the other 7–10 ms."* **The first measurement falsified it** and
that reframed the whole pass, so AK-A's ordering is load-bearing rather than procedural.

### AK-A. Find the frame time before optimising it (goals 244–248)

244. [x] Every millisecond of the worst frame accounted for. Found first that **vsync was hardcoded**
     (`Present(1)`, so every percentile downstream was measuring the panel, not the renderer); it is
     `--vsync`/`--no-vsync` now, on by default because that is the shipping behaviour, and every
     number in this group is vsync-off.
     **Check PERFORMED**: `stress_pose`, vk, RelWithDebInfo — frame **mean 5.36 / median 5.21 ms**,
     p95 7.09, p99 10.23, max 13.68, **GPU march median 4.93**, phase coverage 96.6%.
     **The brief's missing 7–10 ms does not exist**: the median frame is 5.21 ms (192 fps) and 4.93
     of it is the marcher. The 76 fps baseline predates Prompt 002's overlay change and Prompt 003.
     Verdict recorded in the log §1: **the frame is GPU-bound on the march, the average is already
     past 150 fps, and the owner's complaint is entirely about variance.**
245. [x] Counted what the marcher does, per pixel, as a debug view and as a report counter.
     **Check PERFORMED**: `stress_pose` averages **91.9 primary traversal steps** per pixel with 52%
     of pixels hitting; `--debug-view steps` renders the map the counters are summed from. One
     premise did not survive: see goal 269.
246. [x] Priced the secondary rays. **Check PERFORMED**, march GPU ms at `stress_pose`:
     shipping 4.90 (vk) / 5.12 (d3d12); `--no-ao` 3.09 / 3.70; `--no-shadows` 4.41 / 4.83; both off
     **2.50 / 3.18**. So **AO is 1.81 ms = 37% of the marcher** for four rays, shadows 0.49 ms = 10%
     for one, and **secondary rays together are 49% (vk) / 38% (d3d12)**. The primary ray alone is
     2.50 ms — 400 fps — so *any plan that optimises primary traversal is optimising the smaller
     half*, which is the reframing this goal existed to produce.
247. [x] Priced the rebuild storm. **Check PERFORMED**, `fly_transect` with and without rebuilds:
     median 4.64 → 4.01, p95 7.17 → 5.96, **p99 13.16 → 6.89**, slow frames 7/2014 → 3/2265, and
     **upload- or build-caused stalls 5 → 0**. The storm owns the p99, exactly as the brief's §0
     said. Also found here: the 180 ms frames in both conditions were **the harness's own PNG
     capture**, and the live slow-frame line was printing seven phases summing to 0.7 ms of 180
     because `capture` (Prompt 002's eighth phase) was missing from it. A breakdown that does not
     add up to its own total is not a breakdown; `capture` is in that line now.
248. [x] Yardstick and budget. **Check PERFORMED**: the budget is a GPU-timestamp **p95 across a
     fixed camera path**, per backend, asserted in the scenario files and in `ctest` — 4.4 ms vk /
     6.0 ms d3d12 at `stress_pose` when set (recalibrated in goal 273a below, with the reason).
     fps is explicitly rejected as a signal: this machine's 165 Hz FIFO_RELAXED panel caps it at
     155–159, so it cannot express a regression at all.

### AK-B. Stop the rebuild storm (goals 249–253) — a stopgap, and labelled as one

249. [x] The rebuild trigger is decoupled from `lod_radius`. It was `> lod_radius * 0.5f`, which at
     the 4 m default demanded a full 400 MB rebuild **every 2 metres** against a 40–160 m/s fly speed
     and a 0.6–1.3 s build. It is now a named policy on `SvoWorldOptions`: trigger distance,
     hysteresis, minimum interval, and a speed gate.
     **Check PERFORMED**: `fly_transect` median 4.64 → **4.02**, p95 7.17 → **5.89**,
     **p99 13.16 → 8.52 (−35%)** against a rebuilds-disabled floor of 6.89 — about two thirds of the
     available win. Slow frames 7/2014 → 4/2271, upload/build-caused 5 → 2.
250. [x] No rebuild while the camera is moving fast. **Check PERFORMED**: p99 by trigger distance is
     **9.32 / 8.19 / 8.06 / 8.28 ms at 4 / 8 / 16 / 24 m** — flat inside the run-to-run spread,
     because the speed gate already suppresses rebuilds during a fast flight. The distance is
     therefore chosen on IMAGE STALENESS, not frame time: LOD-centre offset 4 m → 0.2% of pixels
     changed, **8 m → 2.9%**, 16 m → 36.7%, 24 m → 45.3%. **Free to 8 m, off a cliff by 16**, so
     8 m ships. Capture: `research/captures/ak_lod_staleness.png`.
251. [x] The fine height field is shared rather than regenerated: `TerrainSampler` gained
     `FocusTiers` and the tiers are shareable across builds.
     **Check PERFORMED, and it is a negative that removed code**: the column-grid cache inside it
     measured a **0% hit rate at both 32 m and 64 m regions over a 900-frame flight** — the reason is
     arithmetic, in the log §8 — so the cache is gone and the sharing primitive stayed. `sampler` is
     **0.15–0.19 s of a 1.89–3.60 s build, 5–9%**, which is why this was never going to be the win.
252. [x] Goal 161's floor-truncation quirk fixed (`static_cast<int32_t>` → `floor`).
     **Check PERFORMED**: the sampler/`fill_terrain` byte-equivalence test now covers
     **196,608 voxels with 0 skipped**, where it previously excluded every negative-height column —
     roughly a third of the region. A test that skipped the disagreeing cases is worse than no test.
253. [x] The stopgap is reported as one. AK-B does not make rebuilds cheap; it makes them rarer. The
     p99 floor with rebuilds disabled entirely is 6.89 ms and AK-B reaches 8.52, so **1.6 ms of p99
     is still rebuild cost that only AK-C/AK-D could remove** — which they then did (goal 257).

### AK-C. Make the structure streamable (goals 254–259)

254. [x] The paged layout designed on paper, in the log, before any code — including its cost model.
     **Check PERFORMED, and the design's own arithmetic was wrong**: writing the model down and
     evaluating it *before* implementing is what caught it (log §5 and the correction that follows).
     The ranking, and where I ranked differently from the brief, are recorded with reasons.
255. [x] Implemented in the CPU reference first: `CellGrid` (a grid of shallow trees walked by
     Amanatides–Woo DDA), `TreeView` (a non-owning window onto one), `FlatCellGrid` (the GPU-shaped
     concatenation). **Check PERFORMED**: the 7,000-ray oracle at **0/7,000**, and **52% fewer
     traversal steps** than the single deep tree — while goal 254's cost model had predicted a
     *rise*. The measurement, not the model, decided it.
256. [x] Mirrored in the shader, both backends. **Check PERFORMED**: march GPU ms
     **d3d12 5.12 → 3.99 (−22%)**, vk unchanged, so **the two backends converge** rather than
     d3d12 trailing by 30–35%. The collider and the crosshair aim query moved to the grid in the
     same change — not optional, because a camera that collides against a different structure from
     the one it sees is the bug class this engine already had once.
257. [x] Per-cell rebuild replaces whole-world rebuild — **closes goal 158**. Detail is quantised to
     a distance BAND (`lod_bands.hpp`) so a cell's content depends on which band it is in, not on
     the exact camera position, which is what makes "unchanged" decidable at all.
     **Check PERFORMED**: rebuild **10.4× faster**; the blocker goal 255 predicted was real and is
     named in log §20.
258. [x] The payload shrunk — **palette compression, implemented and measured** (advances goal 157).
     `tools/palette_probe` measured a real shipping build first rather than trusting the standing
     theory: **55.2% of bricks hold 2 distinct materials, 43.4% hold 3, 98.6% hold ≤3, and nothing
     in a real build exceeds 5.** The brick is now 16 mask words + 52 index words (ten 3-bit palette
     indices per word) + a 2-word 8-entry palette = **70 words, 280 B, down from 144 words / 576 B**.
     Ten indices per word rather than the 10.67 that would fit is deliberate: 3 does not divide 32,
     and wasting two bits per word buys a material fetch that is **exactly one load with no
     boundary case** — the term this goal says to measure, since the march is GPU-bound.
     **Check PERFORMED**: `stress_pose`, same seed and pose, three runs each —
     **resident 543.68 → 279.46 MB (−264.2 MB, −48.6%)**, peak GPU 648.12 → 333.14 MB, and
     **bricks, internal nodes and every other report counter byte-identical** (the tree's shape did
     not change, only its payload). March median **vk 3.45/3.44/3.45 → 3.50/3.50/3.50 (+1.5%)** and
     **d3d12 6.05/6.10/5.51 → 5.60/5.52/6.06 (ranges overlap, no measurable change)**. So:
     **264 MB for 0.05 ms** — the extra dependent load is paid once per hit (52% of pixels) while
     the bandwidth saving applies to every brick word the traversal touches. The build cost is
     recorded too because it is real and invisible: **`fill_brick` CPU 19.86 → 22.18 s summed across
     threads, +11.7%**, with **build wall clock unchanged** (3.40 → 3.27 s, inside the spread)
     because the build is not throughput-bound on 3/4 of the hardware threads.
     Determinism preserved (a test asserts two identical fills produce byte-identical words), the
     oracle at 0/7,000, and **all seven goldens pass on both backends**: `stress_pose` reads
     **0.1009% of pixels changed against a pre-palette golden**, *below* the 0.1317% a pre-palette
     run scores against that same golden — the change is inside the run-to-run noise floor.
     **The encoding is not the one §7 proposed, and the reason is a boundary case rather than a
     preference**: a 2-bit index with a >4-material fallback gives bricks TWO SIZES, and
     `BrickPool` — the fixed-size slot allocator the whole AK-D resident cache is built on — assumes
     one. The shipped 3-bit/8-entry form is *provably* sufficient rather than sufficient-in-sample
     (`kMaterialCount` is 8, entry 0 is Air, entries 1–7 are exactly the seven possible non-Air
     materials), and a `static_assert` turns a ninth material into a build error that names both
     alternatives instead of a rare corrupted brick at run time.
     **Also decided against, as instructed: no apron** — research §1.3's 1.95× blow-up would undo
     the entire lever and then some. And **DAG dedup (goal 163) deliberately not attempted here**,
     for an ordering reason rather than a value one: dedup interns identical subtrees, the palette
     changes which subtrees are bit-identical, so measuring dedup before the payload was settled
     would have measured the wrong tree. Palette first, then dedup — goal 163 stays open with that
     note attached (goal 275d).
259. [x] Goal 162 — the build profile decided, and **LTO is pay-to-win that does not win here**.
     **Check PERFORMED**: log §6 has the before/after; the recommendation is recorded with its
     numbers rather than adopted on principle.

### AK-D. The resident cache and ray-guided streaming (goals 260–265)

260. [x] Fixed-size GPU pools with slot allocation: `BrickPool` (fixed capacity, LRU stamps,
     `kNoSlot` when full) and `ResidentGrid` (persistent slots, dirty runs, structural repack).
     **Check PERFORMED**: an unchanged cell now costs **zero upload bytes**. The split between what
     is pooled (bricks, 94.6% of resident bytes) and what is repacked (nodes) was chosen from where
     the bytes actually are, not from the architecture diagram — and log §21 books the bill that
     simplification later presented.
261. [x] The marcher marks cell usage and files requests through a UAV.
     **Check PERFORMED**: **no measurable cost** — but only after a real 30% regression was found
     and fixed. See goal 262.
262. [x] GPU-side LRU with stream compaction, and a **16 KB/frame readback**.
     **Check PERFORMED**: the compaction is checked against **10,000 generated patterns**. And the
     gate from goal 273 caught a **30% regression** that had nothing to do with the LRU: **a bound
     pixel-shader UAV costs ~30% of the march even when nothing writes to it.** Fixed with two PSOs
     behind an `SVO_MARK_USAGE` define, so the non-marking path binds no UAV at all. This is the
     single best argument in the pass for having built the gate before the feature.
263. [x] Never stall a ray: an always-resident coarse proxy answers for a cell that is not resident.
     **Check PERFORMED**: 29 dumps over a 240-frame run — **frame 8 is the complete landscape at 1 m
     voxels with zero fine bricks resident**, and **near-black pixels stay at 0.00% on every frame
     of the sequence**, which is the mechanical form of "no holes, no black, no sky where terrain
     should be". Capture: `research/captures/akd_never_stall_convergence.png`. A distinction the
     test forced into existence: **"not loaded" and "resident and empty" are opposites**, and the
     proxy must answer for the first and stay silent for the second — `kFlatCellEmpty` exists
     because a coarse proxy was overriding space the fine build had correctly found empty.
264. [x] The producer side, bounded. **Check PERFORMED, and the starvation failure mode was
     reproduced first**: the initial version put **1,710 ms on one frame** (synchronous proxy build
     plus 4,096 cell submissions at once) — research §1.9(a)'s warning, reproduced exactly. Split
     into `start_stream` (PLANS) and `pump_stream` (SUBMITS, bounded, nearest-first):
     **7 slow frames of 2,000, none on swap, none while uploading.**
265. [x] The staged bulk upload is retired on this path — cells install into fixed pools and only
     dirty brick runs are sent. `--upload-budget` keeps its exact meaning (a per-frame byte ceiling)
     and is applied to dirty runs.
     **Check PERFORMED**: 2,000 frames, stationary, `--cell-log2 5 --stream-cells` — **4,090 of 4,096
     cells resident, 3,011.6 MB total, 1.51 MB/frame mean, zero swap stalls, zero upload stalls.**
     **And the honest reading of that 3 GB**, which is larger than a single 292.9 MB whole-tree
     upload would have been: almost all of it is the node array being re-sent whole on each of ~511
     install frames — goal 260's documented simplification meeting its bill. **Streaming's win here
     is latency and smoothness, not total bytes**, and pooling nodes too would cut it to ~300 MB.
     That is now a measured justification for doing it rather than a guess.

### AK-E. The marcher itself (goals 266–270)

266. [x] A coarse start-`t` pre-pass, built, correct, and a **measured wash** — closed as a negative.
     **Check PERFORMED**: the CPU reference (8 cases, 8,189 assertions) shows the conservative bound
     reaches **~80% of the oracle ceiling and skips 40–53% of primary traversal steps**; on the GPU
     **the march really does get 19% faster on vk and the pre-pass really does cost 0.69 ms, and
     they cancel.** No tile size wins — the best total is worse than doing nothing, because 14,400
     tile pixels cannot fill this GPU. The follow-up that could change the answer (the beam and the
     march are serialised) is recorded rather than attempted.
267. [x] The compute port, answered as a **decisive negative, with the mechanism measured
     separately** — which is why the negative is trustworthy. `SV_Depth` forces ROP ordered export;
     **not writing it is 3.35/3.37 vs 4.03 ms on vk (17% of the march) and 10% on d3d12** — and
     nothing was reading it. Removed. The port's main published justification was escaping exactly
     that, so it is already banked. Then goal 271's warp-efficiency measurement closed the rest:
     **92.0–94.1% on every pose**, step divergence ~7%, so there is no occupancy prize left for
     compute to win. **Recommendation recorded, not acted on: do not port to compute yet.**
268. [x] The secondary rays cheapened using what the structure already knows — **the largest single
     win in this prompt**. **Check PERFORMED**: two AO rays instead of four is **−13% vk / −9%
     d3d12**; clamping the AO ray to ≤2 m is **−9% / −5%**; **both together −18% vk / −13% d3d12**,
     and the image difference is **mean 0.097/255 over 0.11% of pixels, max 10** — visually
     indistinguishable, which is the whole argument. The unbounded ray length was the real defect:
     `rayLength = max(0.15, aoRadiusPx * hitDistance * pixelAngle)` grows without limit with
     distance, so distant pixels were paying for AO rays that could never occlude.
269. [x] `kMaxIterations`, the stack and register pressure examined. **Check PERFORMED**: 2048
     iterations (22 stack entries, shipping) measures 4.97/4.96 vk and 5.10 d3d12; the alternatives
     move it by less than the noise on one backend and **16% the wrong way on the other**, which is
     a shader-compiler artefact rather than a register-pressure finding, and is reported as such.
270. [x] TAA history length revisited — **free to lengthen, and the decision is deliberately Prompt
     005's**. **Check PERFORMED**: the cost of a longer history is **exactly zero** (an exponential
     history is not a ring of N frames — the same two texture reads regardless), so this is purely a
     look decision. At rest 8 and 32 frames are indistinguishable (16.64% vs 16.62% contrast); one
     second after motion the longer history is **19% softer**. Capture:
     `research/captures/ake_taa_history_8_vs_32.png`.

### AK-F. Measurement, counters, and the standing regression gate (goals 271–275)

271. [x] Both unverified measurement questions resolved **on this machine, by checking rather than
     assuming**. `VK_KHR_performance_query`: **not exposed by this NVIDIA driver.** Nsight Perf SDK
     / Nsight Graphics: **not installed**, and would additionally need a privileged registry change
     for counter permissions even after installing — so **no counter SDK this pass**, stated as a
     decision with its cost rather than an omission. What replaced it is better targeted anyway: a
     warp-divergence estimator over the real per-pixel cost map (`warp_divergence.hpp`), **falsified
     before it was trusted** — it moves in the predicted direction when fed a deliberately divergent
     map. Result: **92.0–94.1% warp efficiency across six poses and every tiling**, which is what
     closes goal 267.
272. [x] The in-app RenderDoc trigger — **built, absent path verified, active path honestly
     UNTESTED**. Passive `GetModuleHandleA("renderdoc.dll")`, no vendored header; because the API
     struct prefix could not be checked against the real header on this machine it **validates
     itself at run time** (requests exactly 1.6.0, checks `GetAPIVersion`, and disables itself if
     `IsFrameCapturing()` says no capture started), so a wrong offset turns the feature off and says
     so instead of calling an arbitrary pointer.
     **Check PERFORMED, part one — the absent path**: `renderdoc: not injected` at start-up,
     **333/333 tests including all seven GPU goldens byte-identical**, and frame time unaffected
     (`stress_pose` vk p95 3.667/3.669/3.677/3.687 ms with the trigger compiled in).
     **Check NOT PERFORMED, part two — the active path**: RenderDoc is not installed on this machine
     (no install directory under either `Program Files` root, no DLL on the volume, no registry
     entry — checked, not assumed). The capture path has never run and that is not claimed.
     **A bug worth recording**: the first wiring bracketed the *dump block* inside `capture_phase`,
     which runs after the draw calls — it would have captured the staging read-back and nothing
     else, and it would have looked like it worked. The bracket now spans `begin_frame()` to after
     `present()` in both loops, sharing one `frame_dumps` predicate.
272a. The sliver curtains (goals 73/105) stay **BLOCKED**, and the nature of the blockage changed:
     it was "no in-app trigger exists", which is now done; it is now "RenderDoc is not installed",
     which is a five-minute install after which `--dump-every` on the repro in
     `research/water-foliage-design.md` is a one-command investigation. **Check**: run it and report
     what the capture shows — even "the capture shows X, which does not explain it" is progress.
273. [x] Frame-time regression as a standing gate. **Check PERFORMED**: **six of six pass at the
     budget and six of six fail when it is tightened by 20%** — the falsification this goal asks
     for. Registered in `ctest -L scenario` on `stress_pose` and `fly_transect`. Two things had to
     be measured to make it real: a p95 needs about **800 frames** before it is a statistic (at 320
     the 95th percentile is the sixteenth-worst frame, one hitch from meaningless), and the gate must
     be **per backend**, because vk and d3d12 differ by 30–35% at this pose and `ctest -L scenario`
     runs vk only — one shared threshold would have to be set by the slower backend and would let a
     30% vk regression through. **CI does not run it**: the GitHub Windows runner has no Vulkan ICD.
     Said plainly rather than left implied.
273a. [x] A **moving** scenario is not gateable, measured — and the gate is what showed it
     (log §24). `fly_transect`'s gate was removed together with the measurement that justifies it.
273b. [x] The gate recalibrated after it flaked, **and chasing the flake found something worth more
     than the gate**. **Check PERFORMED**: nine standalone runs spread **0.3%** (p95 3.67–3.73)
     while the same scenario inside a full 333-test `ctest` measured **4.511 and failed, about one
     run in three** — with a bimodal frame histogram and 15–18 ms `present` stalls. That failing
     run's **median was 3.49, unmoved**, so the gate became two metrics: a median that carries the
     regression signal and a p95 that guards the tail. Then vk began failing a threshold it had
     passed twelve times that morning — **median 4.01–4.14 against 3.49–3.50 three hours earlier,
     same binary, same pose** — and `nvidia-smi` sampled *during* a run reads **2445 MHz of a
     3105 MHz maximum at 23 W with no thermal-slowdown flag**. 3.50/4.13 = 84.7% against
     2445/3105 = 78.7%: **the GPU's boost state explains essentially all of it.**
     **An absolute millisecond threshold on this machine therefore has an ~18% noise floor that
     belongs to the laptop, and nothing in the frame data shows it.** Gates are set from the full
     observed range including the low power state: **vk median < 4.8, p95 < 5.2; d3d12 median < 7.2,
     p95 < 10.0.** The 20%-tightening Check was performed and passed (measured 3.495 against a
     tightened 3.2) **but in the high power state — a check at a moment, not an invariant**, and
     **d3d12's gate is not falsifiable at 20% at all** because its 18% spread is wider than the
     tightening margin. Said in the scenario file rather than papered over; vk carries the Check,
     which is also the backend `ctest -L scenario` runs.
274. [x] The throughput answer, restated on the finished architecture. **Check PERFORMED**:
     **260 M primary rays/s, 23.9 G traversal steps/s, 937 M rays/s counting every ray cast**, and
     **21–26% faster at every rung than before this prompt, with p95 improving more than p50
     (20–38%)** — the second number is the variance answer the owner actually asked for.
     **The form Prompt 007 needs: 457 million voxels at 7.8 mm, 544 MB resident, at 150+ fps on both
     backends** (conservative, both-backends figure). **And it is a modest result for the hardware,
     which is said plainly** rather than presented as a win: the literature comparison is in log §17.
275. [x] What this pass did not do — recorded with reasons and follow-up goals, below.

### AK-G. What this pass did not do (goal 275)

*These were drafted as goals 276-283 and renumbered to 275a-275h: **Prompt 005's Group AL owns
276-294**, and two goals sharing a number is exactly the kind of drift `Prompts/README.md` exists
to prevent. Commit `ed273cf`'s message refers to "goal 281", which is 275e here.*

275a. Editing (goal 160), and whether the new structure makes it reachable — **it does, and the
     answer is explicit**: the resident cell grid is already a page table with per-cell trees, fixed
     slots and dirty runs, which is structurally most of what HashDAG's page + hash-table design
     needs (research §3.3). What is missing is the hash-interning of subtrees and an edit path that
     rebuilds one cell rather than one world — and goal 257 already made per-cell rebuild 10.4×
     faster, so the expensive half exists. **Check**: an edit that changes one voxel re-uploads only
     that cell's dirty brick runs, with the bytes measured.
275b. Hardware ray tracing and SER — **not attempted, and the reason is scope, not value**: it needs
     the RT pipeline rather than a shader change (research §4.6), which is a larger change than the
     compute port this pass already declined. The documented prize is real (38% → 70% active
     threads). **Check**: a prototype that traces the same scene through the RT pipeline and reports
     march ms and active-thread percentage against the 92–94% warp efficiency measured here.
275c. NAADF's in-cell distance-field caches — **the highest-leverage unexplored idea found in the
     research, and it stays unexplored**. Research §3.5 reports *"3-5x compared to ... variants of
     directed acyclic graphs"*, doubling again to ~10× with AADFs, and there is an open-source
     implementation. It was not attempted because AK-C/AK-D consumed the structural budget and a
     distance-field cache belongs inside a cell, so it wants the cell grid to exist first — which it
     now does. **Check**: build it in the CPU reference, measure traversal steps per primary ray
     against the 91.9 measured here, then decide.
275d. DAG deduplication (goal 163) — deferred **for ordering, not value**; see goal 258.
     **Check**: measure on the post-palette tree, with the brief's 16-identical-tiles unit check.
275e. A **clock-independent** regression metric, which is the real fix for goal 273b. A gate on
     wall-clock GPU milliseconds measures the machine as much as the renderer (the 18% finding
     above). `mean_primary_steps` — mean primary traversal steps per pixel — is **deterministic**:
     the same scene and pose give the same number on any clock, on either backend, forever. It
     already exists in `run_ramp.cpp`, derived from a `--debug-view steps` capture; it is not yet a
     scenario assertion. **Check**: assert it on `stress_pose`, confirm it is bit-identical across
     backends and across power states, and confirm it fails when LOD or traversal changes.
275f. `--dump-every` produces an **all-black frame on alternate dumps** past roughly the fiftieth
     frame, with TAA off and GPU timers off. Found while capturing goal 263's convergence sequence,
     which it made impossible to read at a fixed camera; not attributed to this pass's changes.
     **Check**: a 200-frame `--dump-every 4` run in which every PNG has non-zero contrast.
275g. The **Vulkan timestamp query pool exhausts** (`Failed to allocate Vulkan query for type
     QUERY_TYPE_TIMESTAMP`) on long runs with frequent dumps — new since `GpuPass::Beam` took the
     range count from four to five. **Check**: a 5,000-frame run with dumps that allocates no query
     it cannot free.
275h. `research/gpu-voxel-streaming-and-profiling-research.md` §8's gap list: §8.1 was closed by
     recovering the lost stretch into the research appendix (§9); §8.4 was closed by goal 271 as a
     negative. The remainder is not closed and is not claimed to be.

## AL. The fine-grain look (Prompt 005)

Every measurement, every rejected version and every honest negative:
`research/fine-grain-look-log.md`. What each appearance term is and what filters it:
`docs/the-look.md`.

The prompt's own framing was that the moiré would be one of six named shading terms and that the
structural fix would be about **normals**, following Crassin's open question. **Both turned out
wrong, measured**: all six candidates contribute zero, the normals are already clean, and the
unfiltered quantity is the **material**. That reframing is AL-A's actual content.

### AL-A. Find the moiré, then filter it properly (goals 276–283)

276. [x] A moiré metric, built first and **falsified twice before it was trusted**. The first
     hypothesis — that ring moiré is a low-frequency ENVELOPE on a high-frequency carrier — scored
     57.4 vs 114.7 and was **refuted**: restricted to textured pixels the separation collapsed to
     1.28×, and the two envelope spectra are nearly identical. What the same printout showed is the
     textbook signature: the target's high-frequency energy is spread over 2–8 px, the aliased
     frame's is **91.5% crammed into 2–4 px**. So the metric is
     **E(2–4 px) / E(4–8 px) over textured pixels**, computed with a difference-of-box filter bank
     rather than an FFT (validated against the FFT form: 9.5× vs 9.2× separation).
     **Check PERFORMED**: known-good `lin_water_checkerboard_after.png` **1.144**, known-bad
     `svo_ground_hilltop.png` **10.877**, **separation 9.51×**. Band-limited-noise synthetics whose
     answer is known by construction: 6 px **1.19**, 4 px 5.84, 2.3 px **34.88**, white noise 9.71 —
     and the target's 1.144 lands on the 6 px synthetic, which independently gives goal 284 its
     number. Responds to supersampling (4.72 at 1 spp → 3.52 at 9 spp — **only 25% for 9× the rays**,
     which independently confirms the Luanti report that SSAA does not fix lattice moiré).
     Baseline on the three named poses: `stress_pose` **3.816**, `macro_ground` 2.368,
     `valley_far` 4.333. `voxel_harness --moire FILE.png` reproduces every number above; four unit
     tests; a `moire_ratio` scenario assertion and report field.
     **Two limitations pinned by tests rather than hidden**: a pure 5 px sinusoid scores 12,957 (its
     *radial* period is 3.54 px — the synthetic was wrong, not the metric), and a textureless frame
     scores 4.57 on numerical noise, so `valid` is false below a carrier floor.
     **And a finding handed back to Prompt 004**: `svo_ground_hilltop.png` measures 10.877 and the
     same pose today measures 3.816 — **two thirds of the moiré was already removed by goal 268's AO
     changes**, made for frame-time reasons. Said now so this pass cannot claim it later.
277. [x] The attribution — **and it is none of the six candidates the prompt names**.
     **Check PERFORMED**, CPU reference, `stress_pose`, TAA off: `--no-grain` **3.127** (identical to
     base), `--no-mottle` **3.134** (the prompt's *"strongest suspect"*, innocent), `--no-ao` 3.140,
     `--no-shadows` 3.179, `--no-lod-march` 3.127, **everything off together 3.218 — worse than the
     full shading**. The debug views killed three more in one look: `lodcube` uniform, `level` bands
     large and smooth, `smooth` populated almost everywhere (so the 804,157 solid leaves are **not**
     the hole the prompt expects), `lit` near-uniform.
     The CPU reference gained **`--flat-albedo`**, because no existing flag could test the one
     remaining term — every other toggle removes something applied *after* the material is chosen:
     base **3.127** → **1.962**, carrier RMS −38%. **Discrete sampling of the per-hit MATERIAL is the
     dominant cause at 59% of the excess over the target** — Laine & Karras' *"blockiness caused by
     discrete sampling of shading attributes"* under its published name. Captures:
     `al_debug_views_attribution.png`, `al_moire_material_attribution.png`.
     One masking effect recorded: with the material sampled per hit, removing every other term makes
     the metric *worse*; with a flat albedo it makes it better. The material aliasing swamps the
     others, which is why all six read zero against the shipping configuration.
278. [x] Pre-filtered shading — **an average, not a representative**, and **three rejected versions**
     on the way, each rejected by a viewed capture or a test rather than by argument.
     **Rejected 1**: the smoothing ancestor's *representative* material. Metric 3.127 → 1.795,
     **image worse** — fine speckle became large blotches, because a majority vote is a coarser
     quantiser, not a filter, and no ancestor span fixes it (2.41 / 1.95 / 1.80 / 2.04 at 2/4/6/12 px,
     non-monotonic). *The metric improved and the picture got worse* — the case the prompt warns
     about, recorded rather than shipped.
     **Rejected 2**: a real average read at the normal's 6 px ancestor. **Local contrast fell to 5.7%
     against a 6% floor** and a test went red. Two quantities, two correct scales.
     **Rejected 3**: the average read at the hit node, weighted by *volume* — every green hillside
     turned **olive**, because a volume average includes the stone buried under a grass cap.
     **SHIPPED**: `Brick::exposed_albedo_sum()` weights by **exposed face count**, the same quantity
     `exposed_face_sum()` already accumulates as a vector; `NodeSummary` carries the sum and its
     denominator in world units² so children mix by real surface area; **solid leaves get one too**,
     which is what reaches the 804,157 of them.
     **Check PERFORMED**: `stress_pose` **3.814 → 1.800 (−53%)**, `macro_ground` 2.367 → 1.665
     (−30%), `valley_far` 4.333 → 1.756 (−59%); **the two backends agree to 0.03%**; GPU march median
     vk 3.89 on / 3.94 off and d3d12 5.32 / 5.34 — **free**; **memory zero** (packed R4 G6 B4 into
     the node header's unused bits — a second attribute word would have cost ~5.0 MB, and the prompt
     asked for it to be measured rather than assumed). 335/335 tests, oracle included.
     **And the cost that is not free, stated rather than buried**: local contrast drops 47–56% at the
     distant poses. The filter cannot tell wanted texture from unwanted, and `valley_far`'s "before"
     carries a hatching that genuinely resembles the target. AL-B owes that back.
279. [x] Filter the albedo — **answered by 277's negative**. Goal 279 is conditional on 277
     confirming the mottle; it did not (`--no-mottle` 3.134 against a base of 3.127). The mottle's
     features are 1/24 m and 1/7 m in world XZ, far larger than a pixel at these poses, so it does
     not alias and needs no distance fade. **A completed negative, not an unimplemented task.**
280. [x] The reconstruction question, decided. Prompt 004 goal 270 settled the cost — an exponential
     history is two buffers and a blend weight, so **32 frames costs exactly what 8 costs**.
     **Check PERFORMED**: at rest, 8 frames **1.009** vs 32 frames 1.042 — indistinguishable — and
     goal 270 measured 32 as **19% softer one second after motion**. **Decision: keep 8 frames**, and
     the reason is a budget rather than a preference: goal 278 has already spent half the local
     contrast and a longer history spends more of the same currency.
281. [x] The shadow-lift discontinuity — **conditional, and the condition was not met**.
     `--no-shadows` measured **3.179** against a base of **3.127**: removing shadows entirely makes
     the metric *worse*, so the LOD-quantised lift is not a contributor at these poses. No change
     made, recorded as unmet rather than silently skipped.
282. [x] The AO dither — **partially answered**. `--no-ao` measured **3.140** against **3.127**: no
     measurable contribution on a still. The *crawl* question a still cannot answer is left open, and
     note that Prompt 004 goal 268 already halved the AO rays and clamped their length, so the
     pattern the prompt describes is not the one that ships.
283. [x] The total, reported — **including the number that qualifies all of it**. Every attribution
     figure above is TAA-**off**, because that is what the golden scenarios use. With TAA **on**, the
     shipping configuration, `stress_pose` reads **1.090 filter-off vs 1.024 filter-on — a 6%
     difference, not 53%**, and on `taa_pan`'s moving captures the filtered version measures slightly
     *worse*. **TAA was already averaging away most of the pixel-scale flicker.** Reporting the 53%
     without this would have been true and misleading.
     What survives: the residual speckle is visible and it goes away
     (`al_taa_masks_the_filter.png`, viewed); it is free; and **TAA's help is conditional where the
     filter's is not** — a temporal average is rejected at silhouettes and under fast motion, which
     is when this world moves.
     `stress_pose` now measures **1.024 with TAA on, against the target capture's 1.144** — i.e.
     **smoother than the reference**, which is the arithmetic form of the contrast cost.

### AL-B. The grain, as a deliberate style (goals 284–289)

284. [x] The target capture characterised numerically. **Check PERFORMED**, radial FFT of a
     100%-stone 128×128 window: **dominant radial period 10.67 px** (0.0938 cycles/px), **amplitude
     RMS 6.37/255 = 5.4% modulation**, **anisotropy 641× max/min** with energy at 30–45°.
     **The prompt asks me to confirm or refute that it is directional: CONFIRMED, and not
     marginally.** It is a hatch, not a dither — and it is on **stone only**; the reference's green
     is broad and faceted and its sand smooth, which is why the amplitude became a per-material
     component. **The multiplier, in writing as demanded**: I adopt the prompt's reading of *"much
     finer"* as **3× → 3.56 px**. Sanity-checked against the eye at 70°/720 rows: the target is
     **0.96 c/deg** and 3× finer **2.9 c/deg**, against a 30 c/deg acuity limit — **achievable
     optically by a factor of ten**. The eye is not what binds; see 285.
285. [x] A world-locked stipple at a chosen angular frequency. **Bénard, Bousseau & Thollot (I3D
     2009)** with their four octaves and their exact weights on a zoom cycle — the published
     resolution of the three mutually contradictory constraints; directional along a world vector
     (because 641×); **per material** via a new `Stipple` component every def must answer (stone
     0.076, dirt 0.030, everything else 0); band-limited below 2 px so it cannot become the artefact
     278 removed. Packed into spare slots — the amplitude in the fractional part of the material
     record's `w` (`MaterialShading` now reads the model with `floor`, not `uint(w + 0.5)`, which
     would have rounded a large amplitude into the next model) and the knobs in `g_WaveParams.zw`,
     which were unused: **no cbuffer layout change**, so Prompt 004's field-order trap cannot recur.
     **Check PARTLY PERFORMED, and the gap is named.** Verified: it reaches the frame and reads as a
     directional hatch on stone with grass untouched (`al_stipple_pair.png`,
     `al_stipple_mechanism.png`, both viewed); both backends (vk 1.040, d3d12 1.019, no FXC errors —
     and this change moved a `round` to a `floor` and added bit-packing, exactly where they have
     diverged before); 335/335 tests.
     **NOT verified: that the apparent frequency is constant with distance.** Measured by
     differencing two renders (the terrain's own facets cancel), monotonically increasing requests
     delivered **25.6 / 32.0 / 18.3 / 32.0 px** — noise. **The instrument is the problem, not the
     constant**: a landscape pose spans many distances at once, so "the delivered screen period" is
     not a single quantity there. A calibration fitted to one of those points made the spread worse
     and was reverted; the shipped constant is the derivation and the shader says so.
     **A bug worth recording**: the first version passed a distance-dependent world scale into a
     construction that *already* compensates for distance, so it compensated twice and landed an
     octave and a half too coarse to see (1.028 against 1.024 — no effect at all).
285a. Build the instrument goal 285's Check actually needs: **a fixed camera at 0.3 / 2 / 10 / 60 m
     from ONE stone slope**, so "constant apparent frequency" is a measurable claim. Until it exists
     `--stipple-period` is an honest relative knob and a dishonest absolute one. **Check**: the four
     captures, and the measured screen period equal across all four within 10%.
285b. Apply the stipple **after** the TAA resolve, or add a reactive mask. **Check**: the delivered
     amplitude at `--stipple-period 3.56` reaches the ~8.8 RMS the coarser settings manage, instead
     of collapsing to 1.77.
286. [x] Directionality — **kept, and not a judgement call**: 284 measured 641× anisotropy, so the
     prompt's *"if directionality does not visibly help, drop it"* was settled by measurement rather
     than taste. A fixed world-space direction; per-material hatch directions are not implemented and
     nothing measured here needs them.
287. [x] Close-range edge quality, and **it depends on TAA**. **Check PERFORMED**:
     `research/captures/al_close_range_taa.png`, viewed — `macro_ground` at 0.3 m, `--no-taa`
     against `--taa`. Without TAA the cube edges **stairstep visibly**, with a fine crenellated
     texture along every cube boundary; with it they are smooth. **Nothing else in the pipeline
     antialiases a cube silhouette**: goal 278's albedo filter deliberately stands aside at close
     range (`faceWeight` ~1 when cubes are several pixels across — the John Lin close-up the look
     wants) and the stipple is a shading term that does not touch edges. So `--no-taa` is not a
     neutral A/B here, and any future work that weakens TAA — a reactive mask, a shorter history,
     goal 285b's post-resolve grain — has to keep the edges in mind. Recorded because it is a
     dependency worth knowing, which is exactly what the goal asks for.
288. [x] The reference reproduced side by side — *"the capture the owner asked for"*.
     **Check PERFORMED**: `research/captures/al_reference_side_by_side.png`, committed and viewed.
     A pose was chosen for matching COMPOSITION (water, shoreline, green slopes and exposed stone
     in one frame) and then re-shot at matching APPARENT SCALE, because the first attempt framed
     the terrain four times smaller than the reference and no grain comparison is meaningful
     across a 4x scale difference.
     **What matched**: the stone carries a fine, dense, DIRECTIONAL diagonal hatch that reads as
     the same kind of mark as the reference's; the grass is broad and calm with no stipple at all;
     the stipple is on stone only. That is the arrangement 284 measured in the reference and it is
     reproduced.
     **What did not**: the FORMS (ours is a smooth cone where the reference has rounded organic
     shapes) — **Prompt 006 owns that and this comparison must not be used to judge it**; the
     reference's stone has more tonal variation between light and dark patches than ours; and the
     reference's green carries a red speckle which is its own ALIASING and which ours deliberately
     lacks. **The grain question this pass exists to answer is answered; the form question is not
     this pass's.**
289. [x] One knob, not fifteen. **Check PERFORMED**: `--look shipping|raw|flat|hatched` selects a
     preset, the individual flags still work and still override, the values are enumerated in
     `--help`, and **five test sections assert each preset's field values** so a refactor cannot
     silently change the shipped look.
     **Only the seven APPEARANCE fields are in a preset**, not all nineteen — a test asserts that
     every preset leaves shadows, AO, TAA and the LOD multipliers untouched, because a "look"
     that silently turned shadows off would be a performance setting wearing a costume.
     A plain function rather than a policy template parameter, for the reason
     `templates-and-metaprogramming.md` §3 implies: every axis here is a bool or a float with
     identical behaviour and only its value differs, so a policy would buy nothing and cost a
     recompile per look. Ordering is a stated contract — `--look` applies where it appears — with
     a test in each direction.
     **A tooling bug found doing it**: the tests passed by hand and failed under `ctest`, because
     `catch_discover_tests` passes a test's NAME to the binary as an argument and Catch2 read a
     name beginning with `--` as an unknown option. A test name is an argument; do not start one
     with a dash.

### AL-C. Cost, regression and handoff (goals 290–294)

290. [x] The cost table. **Check PERFORMED**: goal 278's filter is **free** in
     `release-codegen-and-tradeoffs.md` §1's classification — zero memory (unused header bits), GPU
     march median vk 3.89 vs 3.94 and d3d12 5.32 vs 5.34, both inside noise. Goal 285's stipple adds
     four `sin` per shaded pixel behind a flag and no storage at all. **The shipped default remains
     above the target: vk march median 3.89 ms at `stress_pose`**, against Prompt 004's gate of 4.8.
291. [x] Goldens re-accepted on both backends for every scenario the change moved, since the look
     changed on purpose — and **the standing aliasing gate is wired and calibrated**.
     **Check PERFORMED**: `assert moire_ratio < N` on `stress_pose` (2.2), `valley_far` (2.1) and
     `macro_ground` (2.0), all three in `ctest -L scenario`.
     Calibrated from three consecutive runs each: measured **1.8426/1.8435/1.8432**,
     **1.7712/1.7710/1.7713**, **1.6700/1.6702/1.6700** — a spread of **0.01–0.05%**, because this
     is a deterministic IMAGE measurement rather than a clock reading. **Contrast that with
     Prompt 004's frame-time gate, whose ~18% noise floor belongs to the GPU's boost state**: this
     is the gate goal 275e wishes it had, reached from the other direction.
     **Falsified by running it, not assumed**: with `--no-filter-albedo` injected, all three fail
     (3.792, 4.245, 2.361). The specific falsification differs from the one the prompt anticipated
     — it names the mottle's distance fade, and goal 277 measured the mottle at zero, so disabling
     it would have proved nothing; the gate is falsified against the change that actually moves
     the metric.
     **And `contrast_percent` now stands beside it in the same files**, because the two ask
     opposite questions and goal 278 traded one for the other — halving the contrast while halving
     the aliasing. Gating both is what makes that trade impossible to slip through unnoticed.
292. [x] The three standing defects re-checked — **and two of them are not this renderer's**.
     **Check PERFORMED**: `research/captures/al_standing_defects.png`, viewed.
     **`banding_slope.png` and `sliver_closeup.png` are MESH-PATH captures** — both carry the mesh
     renderer's own overlay (*"chunks ready: 294"*, *"visible after culling"*, *"chunk GPU
     memory"*), none of which the svo path prints. The sliver curtains and the slope banding were
     diagnosed on `--renderer mesh`, which this pass does not touch, so they cannot regress from a
     change to `svo_march.psh.hlsl` and an svo-vs-mesh comparison would not be a verdict.
     **Shadow rings (goal 164): SAME, no regression** — `--debug-view lit` at the hilltop is
     uniformly white over every terrain pixel, with no discs, no terraced darkening and no rings.
     That is the direct check on the right renderer, and goal 164's fix holds through both of this
     pass's changes.
     The honest form of the answer: the defect that is on this renderer did not get worse, and
     claiming "unchanged" for the other two would have been unearned.
293. [x] Both backends agree. **Check PERFORMED**: goal 278 vk 1.79963 vs d3d12 1.80010 (**0.03%**),
     goal 285 vk 1.040 vs d3d12 1.019, no FXC errors through two changes that are exactly the kind
     the two compilers have disagreed over (a `round`→`floor`, and new bit-field unpacking).
294. [x] What this pass did not do — 287, 288, 289, 292, 291's gate, 285a and 285b above, plus:
     **textures and an asset pipeline** stay decided against and the gate is unchanged (this pass
     added no texture need — the stipple is procedural and the albedo filter is a per-node average);
     **SSAO / a G-buffer** — goal 41's gate is *"reopen with textures or a real G-buffer need"*, and
     **the answer is still no**: the filtering added here is per-node and lives in the octree, not in
     a screen-space buffer, so it creates no G-buffer need; **path tracing / GI** stays excluded, and
     the reasoning is unchanged and now better evidenced — `research/micro-voxel-creators-research.md`
     confirms Lin's own method *is* a path tracer, so the gap is deliberate rather than an oversight;
     **NAADF's 32-frame history** is measured as free and deliberately not adopted (goal 280).


---

## Group AM — The terrain pipeline, rebuilt on Earth's own physics (Prompt 006, goals 295–325)

Full record with every measurement: `research/earth-terrain-pipeline-log.md`. How to run it:
`docs/terrain-pipeline.md`.

**The pass's shape, stated once because it explains most of the entries below.** The generator was
four octaves of Simplex; it is now a baked 8 km macro field carrying eight planes, built by an
eight-stage pipeline, read through the same `height_at` interface. The research's ten acceptance
tests are a library and a five-seed gate. **Seven of the ten pass on the shipped seed, up from an
untested world.** Three do not, each with a measured cause and an open goal.

**And the finding that outranks all of them: for the whole of Groups AM-A and AM-B the pipeline was
never rendering.** Three independent reasons (goal 321), all found by opening a PNG rather than by
any number, which is the pass's own vindication of rule 2.

### AM-A. The baked field behind an analytic interface (goals 295–299)

295. [x] The architecture chosen: a baked macro field plus an always-present analytic detail term,
     behind an unchanged `height_at`. **Check PERFORMED**: option (b)'s absent-until-baked residual
     was rejected in writing — a world whose shape depends on where the player has been is a
     determinism hazard, and determinism is this pass's first rule.
296. [x] Tile size from the research's own numbers. **Check PERFORMED**: 8 km at 16 m cells
     (500×500, 8.00 MB). The arithmetic is in the log §1: A_c = 0.1–5 km² against a 512 m playable
     region means the region holds 2.6 channel-head areas at best, so the erosion must run on
     something much wider and the region is a window into it.
297. [x] The detail term. **Check PERFORMED, and re-swept four times** — see 320's follow-up and
     `DetailParams`' header, which carries all four tables and the reason they differ.
298. [x] The field wired into the app and the tools. **Check PERFORMED**: bakes in 0.545 s.
299. [x] Per-stage cost reported rather than one number. **Check PERFORMED**: continents 0.021,
     climate 0.006, fill_depressions 0.018, flow 0.020, incise 0.365, diffuse 0.044, climate_final
     0.009, biomes 0.050 s. **The incision is 67% of the bake.**

### AM-B. Continents, climate and the fluvial core (goals 300–309)

300. [x] Determinism under the solver. **Check PERFORMED**: same seed → identical field, asserted
     per stage; the incision's summation order is fixed and serial.
301. [x] Continents with Earth hypsometry. **Check PERFORMED**: land median/max **0.208** against a
     Gaussian field's 0.5 — §9.3's land-half shape. **An adaptation stated, not slipped in**: §9.3's
     ~29% land is a WHOLE-EARTH statistic and this field is 1.3×10⁻⁷ of the planet; land fraction is
     reported without a band because a patch cannot carry a planetary number (log §6).
302. [x] Orographic climate. **Check PERFORMED**: false-colour map viewed
     (`research/captures/am_precip.png`) — ocean uniformly dry, wet bands on upwind-facing coasts,
     dry tails downwind, dendritic wet fingers on incised valley walls. Wet/dry **10.3 : 1** against
     §6.2's ~10:1 anchor. Lapse rate asserted at **6.5 °C/km** exactly. Spillover asserted as a test
     (crest 2.47 → one drift length 1.07 → background 0.10). **No FFT to cost**: the substitute is
     24 ms of a 485 ms pipeline. Four wrong answers on the way, each corrected by its own instrument
     (log §9). **Latitude deliberately enters as a constant**: 8 km is 0.07 degrees of arc.
302a. [ ] The real Smith–Barstad LT model, when there is a reason to trust a parameter set for it.
303. [x] Priority-flood. **Check PERFORMED**: zero internal basins asserted mechanically over the
     whole field, not sampled.
304. [x] Flow routing and accumulation. **Check PERFORMED**: one pass, donor-before-receiver order
     asserted.
305. [x] Implicit stream-power incision. **Check PERFORMED**: a real dendritic network; slope–area
     relationship where noise had none (R² 0 → 0.83).
306. [x] Hillslope diffusion. **Check PERFORMED, and it exposed a bug**: the sea-level guard skipped
     exactly the coastline, where the sharpest curvature is, so the ridge-curvature test measured
     the worst Laplacian as **bit-identical before and after diffusing**. Guard removed.
307. [x] The channel threshold. **Check PERFORMED**: D = **5.58 km/km²**, inside §9.4's 2–12 band.
     **And a finding larger than the question**: the research's own two bands do not overlap —
     A_c ∈ [0.1, 5] km² implies D ∈ [0.22, 1.58], while D ∈ [2, 12] implies A_c ∈ [0.0017, 0.0625].
     The generator sits exactly where D ≈ 1/(2√A_c) says it should. A_c chosen from the density,
     recorded rather than silently adopted.
308. [ ] Tarboton's constant-drop t-test **FAILS**, swept rather than run at one threshold.
     **Check PERFORMED**: |t| = 13.7 at the shipped A_c; the two thresholds that "pass" have five and
     three higher-order samples, so they are the test running out of data, not converging. **And
     pure fractal noise passes it (|t| 1.57, 4/5 seeds) while the eroded terrain fails** — on an
     8 km field this test certifies noise and rejects erosion. Needs a larger domain, not a
     different threshold.
309. [x] Meanders, base level, lakes and deltas — **as polylines, because rivers here are sub-cell**.
     **Check PERFORMED**: largest basin 5.10 km² → 0.48 m³/s bankfull → **2.42 m channel against a
     16 m cell**. λ/W measured **11.9** (band 10–14); sinuosity 1.52 (band 1.2–2.2); 1,160 reaches
     all terminating; **170 of 170 lakes spill**; viewed capture of a meandering river reaching its
     delta at 0.44 m/pixel (`research/captures/am_river_delta.png`). Bankfull discharge from Petit &
     Pauquet (1997), 4–2,700 km² Ardennes catchments — written up with CONFIRMED/INFERENCE labels in
     `research/bankfull-discharge-ratio.md`. **An independent width route disagrees by a measured
     2.8×**, printed alongside so the gap is visible.

### AM-C. The stencil passes (goals 310–316)

310. [x] Glacial carving — **the negative is the result**. **Check PERFORMED**: snowline computed
     from the climate field at **2153.85 m**; the highest ground in the world is **44.55 m**, so the
     stencil selects zero basins and carves zero cells. On a synthetic V above a snowline it works:
     b 1.00 → 1.11, V-index up. **1.11 is below §9.5's glacial band of 1.5–2.0 and is reported, not
     tuned** — the trough width comes from an ice-flux proxy that on a uniform synthetic does not
     widen enough to dominate a ridge-to-ridge transect. Open.
311. [x] Coastal erosion and deposition. **Check PERFORMED**: two synthetic coasts identical except
     for slope are treated differently — cliffs get a shore platform cut at wave base (the
     diagnostic feature; a cliff without one is just a steep slope), gentle coasts get a beach wedge.
     Shipped world: **1,423 coast cells, 0 cliffed, 1,423 gentle** — which is what a coastal plain
     is. **The material still comes from `TerrainBands::beach_band`**, not a new rule.
312. [x] Karst on a lithology mask. **Check PERFORMED**: **229 dolines at 13.6/km², mean diameter
     40.3 m**, on carbonate only. **Goal 312's Check demanded the drainage-sink question be answered
     explicitly: this stage RE-RUNS THE FILL** rather than locally exempting them, because an
     exempted sink would have to be carried as a special case through the flow router, the river
     extractor and the coherence metric — and a filled doline is a shallow closed depression brimming
     to its rim, which is what a doline with a blocked throat is. Zero internal basins asserted after.
313. [x] **Caves — goal 80 reopened and closed.** **Check PERFORMED**: void fraction 0.90%, mean
     passage **15.5 m wide and 9.18 m high** (enterable by a 1.8 m body with room), **zero samples
     below the water table**, and **10,000 random boxes verified against pointwise truth — classify
     never reports uniform over a box that is mixed.** The occupancy-rule plan is written at the top
     of `caves.hpp` as the prompt required. The safety property is `caves_possible_in_band`:
     conservative by construction, false means provably cave-free, so a box the band cannot reach
     keeps its fast path and a box it touches is subdivided.
314. [x] Deserts and dunes. **Check PERFORMED**: the phase diagram selects on **both** axes (asserted
     — a diagram returning one answer everywhere is a sand texture with extra steps); the desert
     carries **three** morphologies; wavelength and height inside Part 5 §3's bands; the dune field
     sits where the biome says desert, which is where the rain shadow is — a check on 302 as much as
     on this.
315. [x] Stratigraphy feeding the erosion. **Check PERFORMED mechanically, because the capture
     cannot show it**: over a ramp spanning every bed, soft beds lose **31.89 m** and hard beds
     **30.36 m** — a 5% contrast, about 1.5 m of bench. **The Check's capture is a negative**: this
     world has no cliff (0 cliffed coast cells, 8° mean land slope), so no pose shows a ledge.
     `research/captures/am_strata.png` is a wooded hill — correct, and benchless. **The contrast is
     calibrated against the acceptance suite**, not chosen for looks.
315a. [ ] **The three new materials are NOT shipped and this is not a silent omission.** Limestone,
     sandstone and clay would take the material count from 8 to 11, and Prompt 004's brick palette is
     3 bits with `static_assert(kMaterialCount <= 8)`. The options are a 4-bit palette at a
     calculable +17% brick size (70 → 82 words) or a packer that tolerates per-brick palette
     overflow. That needs its own measured decision against Prompt 004's 279.5 MB result.
315b. [ ] Warped beds. The warp needs a per-column offset reaching `world/materials`' band
     predicates, and `TerrainQuery` carries no x/z on purpose — that is what keeps the sparse-brick
     and chunk paths byte-identical. Horizontal beds needed nothing new.
316. [x] Biomes and vegetation densities. **Check PERFORMED**: nine biomes, each carrying its
     stems/ha target, the research band, and the citation — **a test asserts every target is inside
     its own band**. Shipped field: ocean 32.6%, temperate forest 30.0%, desert 11.3%, grassland
     10.4%, shrubland 8.6%, beach 5.7%, wetland 0.9%, alpine 0.5%. Forest's mean precipitation is
     asserted at more than twice desert's. §6.4's "edge sharpness = f(cause)" implemented as the
     difference it claims: moisture ecotones grade, elevation and waterlogging thresholds snap,
     measured 0.0007 mixed-neighbourhood fraction at a threshold against a far higher one at the
     ecotones. **Whittaker's temperature axis does not exist here** — 13.50 to 14.00 °C across the
     entire world — so the biome map is honestly a moisture map with a thin alpine belt, and the
     header says so.
316a. [ ] Restore the temperature axis by raising the world's relief. At 112 m and 6.5 °C/km there is
     half a degree to classify on, no snowline (310), and alpine covers 0.5% of the field.

### AM-D. The acceptance suite as a permanent gate (goals 317–320)

317. [x] `world/generation/validation`: the ten tests as a library. **Check PERFORMED**: all ten run;
     **every band is a named constant with its citation in a comment beside it**; and every metric
     has an instrument test against an analytically known answer — a plane reads 45.000°, a
     synthesised k⁻² surface reads β 1.85, a bowl reads exactly one internal basin, a plane's
     variogram reads H 1.00 and white noise 0.00, the FFT puts a pure tone in one bin and satisfies
     Parseval, synthetic V and U valleys read b 1.0 and 2.0 apart. **The FFT is written, not
     depended on** (rule 7), and only 1D — a 2D radial average of the same surface goes as
     k^−(2H+2) and would read a full unit high against a band quoted for the 1D slope.
318. [x] `tools/terrain_dump`. **Check PERFORMED**: every plane dumps (elevation, flow, precip,
     temperature, biome, rivers, with `--zoom`); PNGs re-saved through PIL before committing; **and
     the tool's two private copies of library metrics were deleted** — they had already diverged,
     one sampling only link-end cells and reporting first-order drops LARGER than higher-order while
     the library reports the opposite sign. Two implementations of one test, opposite answers.
319. [x] The five-seed suite as a gate. **Check PERFORMED**: **three metrics hold on all five seeds**
     (skew sign, drainage density, coherence) and three more are gated on their mean plus a majority
     (β, hypsometry, Hurst) — each has one seed within 2% of a band edge, and a gate one seed sits
     2% outside is a flaky gate. **Spread reported, not just the mean**, which is the point: at the
     shipped field size β reads 1.51–2.13 (mean **1.967**, the research's target is 2) and drainage
     density **4.56–6.29**. Runs in `ctest`, no GPU.
320. [x] The old terrain measured as the before column. **Check PERFORMED, after fixing a
     methodology error** — the first version compared internal basins AFTER filling both columns and
     got 0 vs 0, which proves only that the fill works, since the old terrain was never filled at
     all. Measured before any fill: **noise 3,536 internal basins, pipeline 80.** Two surprises
     against the prompt's own predictions: **noise PASSES the constant-drop test** and it initially
     **beat the pipeline on spectral β** — the latter a real regression this pass introduced and then
     fixed (below).
320a. [ ] Express the detail amplitude as a fraction of the macro's LOCAL RELIEF. It has now been
     re-swept four times because the sweep's conditions moved; an absolute amplitude cannot track a
     varying macro, nor a varying field extent.

### AM-E. Integration and cost (goals 321–325)

321. [x] **The pass's largest finding, and it was not on the list.** The pipeline was never
     rendering, for three independent reasons, all caught by opening a PNG:
     - `generate_column_heights_spaced` — the bulk path that fills every brick, essentially all the
       world's geometry — called the raw four-octave noise and ignored the macro field. `height_at`
       read macro + detail. **The world being rendered and the world being collided with were
       different surfaces**, and every acceptance statistic in this pass was measured on the one
       nobody could see.
     - `macro_field` defaulted to **false**, so `voxel_app` ran the old terrain for the whole pass.
     - The playable region is a 512 m window at world (0,0), and on the shipped seed that was open
       ocean. Fixed by a SEARCH, not a stamp: `recentre_on_land` shifts the field's world origin onto
       good ground without touching one elevation. (The stamp was tried first and measured worse than
       the problem — a 2.5 km land bias took sampled land fraction to **100%** and the world lost its
       coastline.)
     **And a fourth, older bug it exposed**: the Remap treated FastNoise2's FBm as [-1,1] when an
     N-octave stack spans ±Σgainⁱ, so five octaves at gain 0.71 delivered **2.82× the stated
     amplitude** — measured as γ(7.8 m) = 16.6 m², a 5.8 m height change over 7.8 m of ground, **a
     37° slope everywhere**. That single bug explains the "57–71° hillsides" CLAUDE.md records from
     Prompt 003. Normalising dropped γ(7.8 m) to 2.10, the predicted 2.82².
     **The equivalence test's fate, decided**: it runs with caves DISABLED. Its purpose is to prove
     the two representations share the same BANDING RULES; caves are a feature the mesh fallback does
     not have and is not getting. The property that makes that sound is asserted separately — at
     threshold zero the sampler is bit-identical to the cave-free world, and with caves on it
     genuinely differs (334 of 20,000 voxels). Not weakened, not deleted.
322. [x] Collision agrees with the new terrain, including caves. **Check PERFORMED**: the octree
     reads the built world so it inherits caves for free. **The dangerous direction is exactly zero**
     — the tree never says AIR where the sampler says SOLID, so the body cannot fall through ground
     that is there. The other direction moved from 0 to **108 of 10,000**: the finest leaf absorbing
     cave tapers thinner than itself, which blocks the body at a passage's very edge rather than
     letting it fall. Bounded at 2% against a measured 1.08%.
323. [x] Trees read the biome. **Check PERFORMED**: density was one number for the whole world; it
     now reads the biome's measured stems/ha and scales acceptance to reproduce it. Determinism
     unchanged — the accept/reject reads the same key every other tree property does. Viewed:
     `research/captures/am_biome_trees.png`, thin in foreground grassland, dense on the forested
     ridge.
324. [x] Cost, end to end. **Check PERFORMED, and it went the other way from the research's own
     warning** that caves and vegetation are the worst-case SVO content classes:

     | | old noise terrain | pipeline + caves + biome trees |
     |---|---|---|
     | macro field bake | — | **0.545 s** (8.00 MB) |
     | bricks | 338,602 | **219,344** (−35%) |
     | internal nodes | 183,618 | 91,685 (−50%) |
     | solid leaves | 804,040 | 350,762 (−56%) |
     | tree memory | 109.1 MB | **68.9 MB** (−37%) |
     | build time | 1.25 s | **1.01 s** (−19%) |
     | sampler time | 0.20 s | 0.27 s (**+35%**) |
     | boxes classified | 1,575,541 | 769,889 (−51%) |
     | bricks sampled | 1,024,654 | 525,141 (−49%) |

     **Caves do cost what `caves.hpp` predicted** — +35% sampler time, the band's forced subdivision
     — and it is swamped by the surface being smoother: mean land slope 42.6° → 8°, so there is far
     less surface to represent. World-ready **1.56 s** against the prompt's ~2 s ceiling, so AM-A's
     tiling answer is not needed yet.
325. [x] What this pass deliberately did not do, each with a reason and a follow-up: real-time
     erosion (the field is baked once); tectonic simulation (§10.1 puts it on the "fake convincingly"
     side and the orogen is stamped); a glacial LEM (a stencil that passes the cross-section test is
     a success, not a compromise — §10.1); vegetation succession (there is no clock in the world);
     planet-scale worlds (Group S decided static and bounded, and it holds); rivers as flowing water
     (this pass carves channels, it does not fill them); sediment transport as a live system (the
     `Sediment` plane exists and is unwritten); seasons (the climate field is an annual mean by
     construction).

## Tooling defects found in passing (goal 101's standing expectation)

200. [x] `--dump-every` wrote nothing and reported nothing — `dump_frame`'s result was
     `(void)`-discarded and its path was relative to whatever the working directory happened to be.
     A whole capture session produced no files and no error. It now honours `VOXEL_DUMP_FRAME` as
     the stem (absolute, numbered) and logs `written`/`FAILED`. **Check**: a capture session
     produces the files it claims to, with a log line naming each.
201. [x] `--crosshair` / `--no-crosshair`. The crosshair is suppressed under `--verify-frame` by
     default so a HUD cross cannot inflate the local-contrast metric; the override exists so a
     capture can show it. **Check**: `--verify-frame` reads 34.7%/34.6% without it, and every
     crosshair capture was taken with it.



## Group AN — View distance and living cover (Prompt 007, goals 326–345)

Full record: `research/view-distance-and-cover-log.md`. The operating manual is
`docs/view-distance.md`. This group closes Group AF's 190–192 and Group AG's 193–195 in place, and
reopens goal 40.

### AN-A. The perceptual criterion (goals 326–332)

326. [x] `render/lod/perceptual.hpp` — every distance criterion as a named constant or function with
     its citation beside it. Header-only, dependency-free, registered OUTSIDE the renderer guard
     because `world/svo`'s LOD ladder cites it and it must build in the no-GPU CI job.
     **Check PERFORMED**: it reproduces every worked number in the eye research — 1 cm resolvable to
     **34.0 m** at 20/20 and **54.0 m** at the 94-ppd ceiling, an 18 cm face to **619 m**, contrast
     transmission **82/68/46%** at 0.5/1/2 km in V = 10 km and **91/82%** in V = 20 km, own horizon
     **5.03 km** at 1.7 m eye height, a 1000 m massif to **127 km** and a 4000 m peak to **249 km**.
     Three corrections are pinned as tests so they cannot drift back: 20/20 is **60 ppd** and not
     Campbell & Green's 120 (a 2x budget error); the CENTRE pixel subtends **10.3%** more angle than
     deg/px, which is exactly where the player is looking; and the two visibility conventions are one
     model with two thresholds — WMO-No. 8 prints **3/sigma** and 3.912 appears nowhere in it — with
     the **1.3059** ratio asserted.
327. [x] Fog derived from an authored visibility. What was there: `0.0030 * (0.80 + 0.20*exp2(-y*0.012))`
     with a falloff SQUARED in distance — a bare constant, a Gaussian where Koschmieder's law is an
     exponential, and an unexplained 1/58 m height e-fold. Now sigma from `--visibility` and the
     BAROMETRIC `exp(-y/H)` with H = 8500 m (1.3% of sigma over this world's relief — physically
     right, practically negligible, correct in advance if the world ever gets mountains). The lerp
     toward `SkyGradient(dir)` STAYS: it is Narasimhan & Nayar's two-term model with the airlight
     radiance being the sky in that direction.
     **Check PERFORMED**, pixel-sampled rather than formula-read, and the first attempt was wrong in
     an instructive way — solving a linear interpolation in tonemapped sRGB 8-bit space read
     T = 0.603 against a predicted 0.679. Re-measured with `--no-tonemap --no-bloom`, sRGB-linearised,
     looking straight down from 1000 m: V = 5/10/20/40 km gave T = **0.4102 / 0.6301 / 0.7898 /
     0.8884**, implying one ray distance of **1139 / 1181 / 1206 / 1210 m** — agreement to within 6%
     across an 8x span of V, which is the distance-INDEPENDENT form of the check and the stronger
     one. My assumed 990 m was wrong; the shader was not.
328. [x] The LOD knob expressed as an angle. `target(d)/d` is CONSTANT beyond the radius, so
     `lod_radius` was never a distance — it was the denominator of an angle in the least legible
     possible units. The shipped 4.0 m at a 7.8 mm finest voxel **IS 6.71 arcmin**, 6.7x coarser than
     20/20 resolves; stated, not changed. `--lod-arcmin` is the knob in the research's units, and NOT
     `--lod-quality`, which already exists and scales the MARCHER's pixel angle at shading time while
     this sets the BUILDER's target at construction time.
     **Check PERFORMED**: `--lod-radius 4` produces today's tree byte-for-byte, because a zero
     `lod_quality_arcmin` recomputes NOTHING. (Round-tripping 4.0 m through a rounded 6.71 arcmin
     lands 4.0026 m instead — a 0.16% difference that showed up as 219,703 bricks against 219,344,
     which is exactly why the alias does not go through the conversion.) And the measured answer to
     "why not just use the eye's limit": **6.71' = 219,703 bricks / 69.0 MB / 0.98 s; 4.50' = 449,958
     / 143.2 / 2.38; 3.00' = 1,233,952 / 406.8 / 7.83; 1.00' did not complete in 900 frames.** A 2.24x
     radius costs 5.6x the bricks — an exponent of **2.1**, between the area and volume laws. The
     eye's own limit is roughly an ORDER OF MAGNITUDE outside the budget, and a test pins the exponent.
329. [x] The region at 4096 m, 8x the view. **V was never the binding constraint**: V = 19 at 4 km
     against `kMaxVoxelBits = 24`, and V stays under 24 well past the research's 3.44 km criterion,
     so no cascaded root was needed and none was built. Cost was the constraint.
     **Check PERFORMED**: 512 / 1024 / 2048 / 4096 / 8192 m gave **219,344 / 316,179 / 392,109 /
     445,038 / 498,334 bricks**, **68.9 / 98.4 / 121.6 / 138.0 / 154.6 MB**, march **2.69 / 3.21 /
     3.33 / 4.09 / 5.55 ms**, at 165 fps except the last at 107. **256x the area for 2.27x the
     bricks**, because everything a larger region adds is at coarse LOD — the exact inverse of goal
     328's finding. Frame verification **32.0% -> 51.2%**.
     **THE CHECK'S NEGATIVE, REPORTED**: it asked for a capture showing the far edge is beyond where
     fog has taken the image to sky. **It is not.** At clear-air V = 20 km, transmission at the
     2048 m boundary is **0.670** — a visible line. Hiding it needs V = 2670 m, i.e. thick haze,
     which throws away goal 327's whole point. Clear air and a 2 km region are incompatible; the far
     silhouette tier is what the boundary needs, and goal 345 lists it.
330. [x] The far plane, which goal 329 had made smaller than the region. The marcher has no far clip
     but the depth it writes comes from the projection, so geometry it had already found was being
     clipped in depth; 1.42% of pixels change.
     **Check PERFORMED**: one float32 ULP of [0,1] depth in metres, far = 2000 vs 4096 — at 100 m
     0.51 vs 0.48 cm, at 500 m 19.2 vs 13.6, at 1000 m **65.4 vs 40.4**, at 2000 m 238.1 vs 133.2.
     Raising the far plane IMPROVES precision at every distance that matters, which is the opposite
     of the usual intuition: in a standard [0,1] projection the NEAR plane dominates the distribution.
     **Reversed-Z assessed and DEFERRED with its reason**: nothing z-fights, because the marcher is
     the only writer of terrain depth; switching touches the projection, every PSO's depth state, the
     TAA reprojection and the post chain — real risk against no present symptom.
331. [x] Fixed foveation, measured at 23.8%, shipped OFF.
     **A REAL CONCEPTUAL ERROR ON THE WAY, and only the capture caught it.** The first version wrote
     `lodAngle *= 1 + m*e`, treating Guenter's tolerated MAR as a multiplier — but that model is
     stated against a 1-arcmin fovea and this renderer's foveal LOD is already 6.71 arcmin, so it
     compounded a **53x** factor at the screen edge onto an already-coarse baseline. GPU fell
     5.35 -> 0.20 ms, a 27x "saving", and the image became metre-wide blocks edge to edge. The RATIO
     form — coarsen only where the eye tolerates MORE than the renderer already delivers — is what
     foveation means. Then Hsu et al. 2017's **7.5 degree** inner radius, because the corrected
     version still started coarsening at 4.3 degrees.
     **Check PERFORMED**: at Guenter's 1.32 arcmin/degree with Hsu's inner radius, **5.33 -> 4.06 ms,
     23.8% saved**, clearing the prompt's 10% bar. **And the periphery is still visibly degraded** —
     the outer thirds are plainly blockier in a still image without hunting for it. So it ships off.
     Not "the saving was too small"; "the saving is real and the cost is visible", and on a desktop
     the cost is paid wherever the player happens to be looking. Tursun et al.'s 1.1-1.8x on a 1440p
     desktop and 0.9x — slower — on a simple-shader scene reached the same place from the other side.
332. [x] The distance fades in arcminutes. The smooth-normal blend and the grain fade thresholded
     `cubePixels` at 1.5 / 4.5 / 4.0 — resolution-DEPENDENT in the worst way, since the same cube at
     the same distance is filtered differently at a different viewport. At 4x resolution the old
     "1.5 px" is 2.507 arcmin, a quarter of the angular size.
     **Check PERFORMED**: both fades now threshold ANGULAR size, at 10.030 / 30.089 and 10.030 /
     26.746 arcmin at every viewport, and the shipped image is unchanged — **11 of 921,600 pixels
     differ (0.0012%)**. Getting there took two tries: the first conversion used 4.0 px where
     `(cubePixels - 1.5) / 3.0` means 4.5, a supposedly no-op refactor that moved **42%** of pixels.
     **AND A MEASUREMENT GOTCHA**: comparing two builds with wind ON showed 38% differing even after
     the threshold was right, because the shimmer is driven by WALL-CLOCK seconds — any A/B involving
     it must pass `--no-wind`, which is how the 0.0012% was measured. Goal 337 later made that a
     flag rather than a discipline.

### AN-B. The world reads as alive at distance (goals 333–334)

333. [x] (= goal 195) The distance shimmer. **Check PERFORMED**: a viewed capture across a valley —
     the ground carries visible wind-swept brightness variation at zero geometry cost; `--no-wind`
     kills it (**21.24%** of pixels differ) and is bit-identical to the pre-change image by
     construction, because the block is gated on `g_WindDirSpeed.z > 0.0`. Both backends differ by
     **0.77%** at the same pose, inside Prompt 002 goal 217's 12.8% floor.
334. [x] `wind_responsive` as a material component, exported to shaders as a registry-derived
     bitmask. A MEMBER rather than a shading model, and the distinction is the whole point: the
     marcher's wind block was gated on `Shading::Foliage`, so making ground grass respond would have
     reclassified Grass and given a lawn a tree crown's sway.
     **Check PERFORMED**: a test asserts the mask equals the registry for every material and
     recomputes it independently — a test that called the renderer's own function would agree by
     construction and prove nothing. **IT IMMEDIATELY EARNED ITSELF**: the mask came out ZERO,
     because `make_def` used POSITIONAL initialisers and the new trailing member was silently
     value-initialised for every material. Nothing failed to compile. `make_def` now uses DESIGNATED
     initialisers, which turns exactly that omission into a compile error.

### AN-C. Trees that move (goals 335–337 = goals 190–192 completed)

335. [x] (= goal 190) Hierarchical spring sway, as BRANCH CHAINS rather than segments — and that was
     measured, not assumed. The first version gave every segment its own oscillator and **the pole
     rang at 1.03 Hz against a predicted 0.26**: a serial chain is `M theta.. + K theta = Q` with K
     diagonal but **M DENSE**, so dropping the off-diagonals leaves each joint at `sqrt(k_i/J_i)`,
     3x the collective mode for a 40-segment pole. The fix is fewer, better coordinates. Within a
     chain, segment j takes `w_j ~ (L-s_j)*ds_j` — the static tip-load curvature — which makes
     `K/lever^2 = 3EI/L^3` exact and `omega^2 = 12 EI/(rho A L^4)` land **2.9%** from Rayleigh's
     12.727, from two independent shape functions.
     **Check PERFORMED**: closed form **0.260064 Hz** for a 20 m sycamore at 25.2 cm dbh (slenderness
     79, inside the forest-broadleaf band) against §3.1's FE-simulated 0.26; leaf-off shift
     **18.4964%** against §3.4's measured 18-19%; the log-decrement instrument recovers a given zeta
     to 0.02% (asked 0.039, measured 0.039009); **damping by branching is real but modest** —
     child-reaction on vs off gives zeta_eff **0.0861 -> 0.0901** and f0 0.914 -> 0.831 Hz;
     determinism bit-exact. **FRAME BUDGET, with the attributor**: 129 trees / 3,650 chains /
     13,272 segments in the 120 m ring cost **0.049 ms/frame mean, 0.246 worst** against a 0.5 ms
     budget, and 1,552 trees at a 400 m ring cost **0.585 mean, 2.933 worst** — over. Cost is linear
     in stepped chains, so the ring could reach ~370 m before the budget binds. **120 m is that
     number, not a taste.** Captures: `an_sway_period.png` (four frames across one period at 14 m/s,
     rest pose in grey behind) and `an_sway_breeze_4ms.png`.
     **TWO THINGS ONLY A PICTURE FOUND**: a horizontal branch could not move sideways, because
     rotation vectors were projected onto the HORIZONTAL plane (true of a trunk, false of everything
     else — a horizontal branch broadside to the wind has a purely vertical bending torque and the
     projection deleted all of it); and **the wind field had a hole exactly where trees resonate** —
     a 0.05 Hz gust, a 4 Hz flutter, and nothing between, against fundamentals of 0.26-1.0 Hz. Fixed
     in `world/wind` as `wind_buffet`, three waves at 0.17/0.59/1.87 Hz at a stated 0.20 turbulence
     intensity; mean crossings in 60 s went **6 -> 28**.
336. [x] (= goal 191) Skeleton-driven voxelization — capsule branches and leaf clouds from the pipe
     model's own radii and leaf areas.
     **Check PERFORMED, growth measured BEFORE committing**, two runs each at 2 m from a trunk:
     bricks 1,061,035 -> 1,063,288 (**+0.21%**), tree memory 350.3 -> 350.9 MB (**+0.6 MB, +0.17%**),
     build 10.09/9.85 -> 10.54/10.88 s (**+8%**), plus 183 skeletons / 29,465 primitives / 1.84 MB /
     38 ms. **Against Prompt 004's resident-cache budget (279.46 MB resident, 333.14 MB peak GPU),
     0.6 MB is 0.21% of it** — and the research names vegetation as the worst-case SVO content class,
     which on this world it is not, because a crown of overlapping leaf balls has less surface than a
     smooth octahedron of the same volume. `--verify-frame` **24.19 -> 25.17%** (note the Check's
     ">= 25%" is cleared by the SKELETON run and not by the implicit baseline). The terrain-sampler
     byte-equivalence test is untouched, because it runs at skeleton_radius 0; the oracle stays
     **0/7,000**. Viewed captures at 2 / 10 / 60 m on both backends.
     **The build cost was +37% and is now +8%**, from two fixes found by measuring: `material_at` ran
     `std::sort` to deduplicate a POINT query that lands in exactly one cell, and the acceleration
     grid's cells were sized to the LARGEST primitive (a metre-wide leaf cloud), so every cell held a
     dozen clouds that every voxel distance-tested. **The canopy took three tries and a picture each
     time**, ending with the leaf-area density DERIVED as `total leaf area / crown volume` — a fixed
     2.0 m^2/m^3 had been packing a whole crown's leaf into a fifth of its volume.
     **AND THE THING THIS GOAL ACTUALLY FOUND**: `tools/svo_render` and the harness's `pose_ground`
     resolver were working in the pre-Prompt-006 NOISE world, because the macro-field bake lived in
     an anonymous namespace inside the app. Measured: over a 320 m square the two surfaces differ by
     a **mean of 35.5 m and a worst of 107.1 m**. The CPU reference renderer had been a reference for
     nothing since Prompt 006, and every `pose_ground` had been resolving against ground that is not
     there — the collision counter had been reporting it all along as "ticks ended INSIDE solid".
     Found by putting a CPU frame and a GPU frame of the same coordinates side by side. Fixed by
     extracting `bake_playable_field`, with three tests pinning it.
337. [x] (= goal 192) Geometric canopy motion: **attempted, measured, REJECTED, shipped off**, which
     the goal named in advance as an acceptable outcome.
     **Check PERFORMED**: captures at 2 m and 10 m — the warp changes 17.9% and 28.2% of pixels and
     **none of it says the tree is in motion**, because a crown reads as moving when its EDGE moves,
     the edge is where traversal stopped, and this goal excludes warping traversal. **TAA ghosting:
     none** after a 50-degree pan, because the warp changes shading without changing motion vectors —
     the good half of the same fact that makes it invisible. Oracle **0/7,000 and 0/4,000**, by
     construction rather than by luck. FXC accepts it, so both backends run it.
     **Two real finds**: the first version gated on `wind_responsive`, and ground grass IS
     wind-responsive (goal 334, deliberately) — a 6 cm domain warp on a lawn reads as the ground
     CRAWLING and was the dominant change in the difference image. And **the animation clock was the
     wall clock**, so no wind-driven A/B was reproducible: three runs capturing at the same scripted
     second measured 0.122 / 0.255 / 0.192% — a spread three times the effect. `--anim-step` fixes it
     for scripted runs, with the stated limit that a capture at a scripted TIME still lands on a
     different frame index, so a reproducible wind-driven capture needs `capture frame N` too.

### AN-D. Ground cover (goals 338–340 = goals 193–195)

338. [x] (= goal 193) Voxel blade clusters in the finest ring. **THIS REOPENS GOAL 40** — "grass
     ground-cover geometry, needs instancing+textures" — and the new evidence is
     `research/grass-rendering-research.md`, which ranks grass-as-voxels #2 for this engine and notes
     it needs no new pipeline at all. Its two stated costs are accepted rather than argued away.
     **THE PALETTE WAS THE REAL WORK.** A blade must be `Phase::Foliage` where ground grass is
     `Phase::Solid`, so this needed a NINTH material against an eight-entry brick palette whose
     static_assert named two ways out, both expensive: a wider index is **+20% on every brick in the
     world** (84 words / 336 B against 70 / 280), most of goal 275's measured -264.2 MB handed back;
     a second size class is a different allocator. The third way shipped: keep 3 bits and 280 B and
     make the invariant PER BRICK, backed by `tools/palette_probe`'s own measurement that 98.6% of
     bricks hold three or fewer distinct materials and nothing exceeds five. Overflow has a
     deterministic answer and a counter that a test asserts is zero on real content and non-zero on a
     constructed nine-material brick. **Two existing tests had encoded the old guarantee and both
     were WRONG rather than stale** — `tree_replaces` was asserted against a hand-written restatement
     naming the yielding materials by identity.
     **Check PERFORMED, per-biome density against the research**: the table is Part 6 §7's plants/m^2
     with each citation beside it, and the two entries the research measured as COVER rather than a
     count say DERIVED in theirs. A TUFT stands for `plants_per_tuft` real plants, named as the LOD
     device it is, because 60 plants/m^2 over a 24 m ring is 108,600 of them. Cost at 24 m:
     **20 plants/tuft (3 tufts/m^2) = +1.54% bricks, +1.29% MB, +4.6% build** — ships; 6 = +5.08% /
     +4.27% / +14.9%; 2 = +14.06% / **+11.72%** / +18.7%, which exceeds the Check's <+10% MB and is
     the documented ceiling. Viewed captures at **1 / 4 / 15 m on both backends**. **Walked through
     it**: 420 ticks sprinting, **0 frames below the ground surface**, collision **0.0359 ms/tick**
     against a 0.20 ms budget — a blade is Phase::Foliage so the collider never sees it, asserted at
     the registry. **The honest look note**: at an affordable density this reads as scattered dark
     tussocks, not a sward. That is what 10 plants/m^2 IS.
339. [x] (= goal 194) The instanced raster overlay. **The research's recommended technique assumes a
     depth buffer this marcher does not write** — goal 267 removed SV_Depth after measuring it at 17%
     of the march on vk. The engine already exports what is needed under another name: the march's
     second render target is the hit distance, so a blade compares its own view distance against that
     texture and discards where the world is nearer. Grass-against-grass gets the otherwise-untouched
     DSV. Two mechanisms, each doing the half it can.
     **Check PERFORMED**: 22,950 blades, its own GPU timer — vk march 6.80, **grass 0.21**, whole
     frame 6.92 -> 7.09; d3d12 march 9.99, **grass 0.05**, 10.10 -> 10.22. **32 B/blade** against
     Ghost of Tsushima's 16 floats (64 B) for ~83,000 blades in 2.5 ms, i.e. **0.0092 us/blade on vk
     against GoT's 0.030** — 3.3x cheaper per blade on vk and 14x on d3d12, which is a desktop GPU a
     decade newer, a 6-triangle blade against GoT's 15-vertex one, and no compute culling. Occlusion
     correct both ways on a hillside; wind on vs off moves **49.7%** of pixels; the player bend moves
     **1.79%** looking down (0.30% at a level pose, because a 1.5 m radius from a 1.7 m eye is a small
     patch at the bottom edge); **TAA does not ghost**, and that is a design decision rather than
     luck — the overlay writes its OWN distance so the resolve reprojects a blade against the blade's
     distance. Both backends.
     **It ships OFF, and not for the GPU cost**: a full instance rebuild is **10 ms worst, 0.148
     ms/frame mean**, once per quarter-radius of camera movement — a dropped frame every few seconds
     at walking pace. That cost was also MIS-ATTRIBUTED at first, appearing as a 17.3 ms `sway` phase
     against the 0.049 ms goal 335 measured; it is now timed separately.
     **A bug worth recording**: the first version drew nothing at all. `mul(v, M)` compiles fine and
     silently transposes (this project uses `mul(M, v)`), and then, while bisecting THAT with a debug
     constant colour, the pixel cbuffer went unreferenced, the compiler stripped it, and
     `GetStaticVariableByName` threw at PSO setup. Two independent invisibilities stacked.
340. [x] The three tiers agree at their boundaries — **structurally**. `grass_blade(tuft, index,
     count, params)` is the ONE place a blade's geometry exists and both geometric tiers call it;
     `grass_wind_phase(tuft)` is the same for the phase. The first version of goal 339 re-derived the
     fan from the tuft's id with its own copy of the turn angle, which is exactly how a ring appears
     six months later when one copy is tuned.
     **THE THIRD TIER DOES NOT HAVE TUFTS**, and saying so is more useful than pretending: the
     distance shimmer is a per-pixel brightness term with no per-tuft identity, and what it shares is
     the wind field.
     **Check PERFORMED**: 1,000 tufts, 5,000 blades, every endpoint asserted IDENTICAL (not
     approximately) between the voxel capsule and the shared segment the raster instance is built
     from, every id distinct, every phase reproducible — **48,004 assertions**. **No ring, measured**:
     the overlay ends at 14 m and the voxel tier at 24 m, which at the capture's pose fall at image
     rows 335 and 301; the per-row fraction of blade-dark pixels ramps 0.247 -> 0.338 with a largest
     smoothed step of **0.0103 at ROW 634** — a hill crest, 4.1x the mean step and three hundred rows
     from either boundary. The reason is the shared call: crossing the overlay edge removes the extra
     raster blades and leaves the shared ones standing as voxels, in the same place.

### AN-E. The budget, and the whole view (goals 341–345)

341. [x] The frame budget, spent and accounted. Measured at `stress_pose` by turning this prompt's
     default-on features off one at a time: shipped default **845,260 bricks, vk march 5.83, d3d12
     8.77**; minus voxel grass 841,994 / 5.80 / 8.55; minus skeletons too 844,486 / 5.69 / 8.37;
     minus sway and back to a 512 m region **772,906 / 4.69 / 6.92**.
     **THE REGION IS THE SPEND** — everything else this prompt turned on is at or below the 18%
     run-to-run noise floor `stress_pose`'s own comments record for this machine. That was not the
     expectation: trees that move and ground that is covered SOUND expensive, and goal 329's own
     table made the region look free.
     **Check PERFORMED against the 150 fps target**: vk's whole GPU frame is **6.32 ms (158 fps),
     inside**; d3d12's is 9.19 ms (109 fps), outside — **and d3d12 with every one of this prompt's
     features off AND the old 512 m region is 7.35 ms (136 fps), still outside.** Goal 244's
     measurement, which set the target, reads "`stress_pose`, vk". So no feature comes off the
     default on account of d3d12, because taking every one off does not bring d3d12 to target; the
     gap is the backend's and it is a goal rather than a knob (345).
     A note on reading frame numbers rather than GPU ones: **vsync is on by default and this panel is
     165 Hz**, so any frame whose GPU work exceeds 6.06 ms lands at 12.1 ms — which is why every
     frame median at these poses is 12.02-12.10. `--no-vsync` is not the answer either: the run then
     hits the harness's frame cap before the script finishes.
342. [x] The hilltop shot: `research/captures/an_hilltop_{vk,d3d12}.png`, the shipped default from
     high ground at (48, 0) looking down the valley, `contrast_percent` 27.26 / 27.5.
     **Check PERFORMED, and the backend comparison nearly reported the opposite**: vk against d3d12
     reads **34.0%** of pixels differing — three times Prompt 002 goal 217's 12.8% floor — and with
     `--no-wind` the same pair reads **1.375%**. The whole difference was the wind's phase, because
     the two backends reach a scripted capture at different frame counts and `--anim-step` ties the
     clock to frame count. The honest number is 1.375%, well inside the floor.
     **What is not in the frame, reported rather than dropped: no river.** `docs/terrain-pipeline.md`
     §6 already records sub-cell rivers as a limitation of this world at a 16 m field, so at a
     hilltop's distance there is nothing to see. Inherited, not caused.
343. [x] Four new scenarios: `grass_walk` (both ground-cover tier boundaries in one frame),
     `tree_sway` (one tree at a real wind with a fixed animation clock), `horizon` (the far
     region-edge check), `valley_far_v` (the fog sweep, run under
     `--ramp visibility:10000,20000,50000,100000`). `grass_walk` and `tree_sway` are registered in
     `ctest -L scenario`; **`horizon` and `valley_far_v` deliberately are not** — horizon exists to
     show a boundary that is still visible (goal 330's honest negative), so gating on it would gate
     on a known failure, and valley_far_v is meant to be ramped rather than run once.
344. [x] Regression cross-check against the three named old defects. **Check PERFORMED**, nine
     scenarios on vk: moire ratio **0.883 - 2.321** across the whole library (stress_pose 1.920
     against its 2.2 gate, valley_far 1.872 against 2.1, walk_cliff 1.369, spawn_stand 1.913/1.920,
     macro_ground 1.689, clip_stress 1.592, fly_transect 2.321/1.409/0.883, grass_walk 1.851) — **no
     new aliasing anywhere**, and valley_far's own gate now PASSES where it had been failing at 2.304
     under the mis-resolved pose goal 336 found. No shadow ringing and no sliver curtains in any
     capture; the sliver curtains were a mesh-path artefact and this path does not have it.
     **Two gates failed and both were CALIBRATION rather than regression**: `stress_pose`'s
     `gpu_ms_median < 4.8` measured 5.924 and `valley_far`'s `< 3.6` measured 5.781, against gates
     set before the 4 km region. Recalibrated on this shipped default with the measurement recorded
     in the scenario file, exactly as goal 273's own text describes.
     **And every golden in the library was re-taken, then VERIFIED rather than assumed.** Four more
     scenarios (`valley_far`, `macro_ground`, `macro_tree`, `fly_orbit`) needed `option --no-wind` for
     the same reason `spawn_stand` and `stress_pose` did, and `valley_far` had been failing standalone
     at **2.824% of pixels / mean 2.253/255 against gates of 1.500% / 1.200**. Final state: **all
     sixteen scenario runs PASS on both backends, and all ten golden comparisons read 0.0000% of
     pixels changed** at mean distances of 0.000-0.039/255. No gate was loosened to get there.
     **One more capture lost its golden rather than being loosened**: `fly_orbit`'s `quarter` fires at
     frame 120 mid-orbit and, re-taken wind-free, STILL promoted at **53.707% on vk / 2.719% on
     d3d12** -- worse than Prompt 003's 5.3-35.5% post-motion band, because the dominant term there is
     the rebuild storm and `--no-wind` cannot touch it. The scenario's own GOLDEN POLICY block already
     required this of moving captures; `capture end final` honoured it and `quarter` had been missed.
     It is `no-golden` now and its two reference PNGs are deleted.
345. [x] The deliberate-omissions list, each with a reason: **the far silhouette tier** (the region's
     2 km boundary IS visible at clear air and hiding it needs V = 2.67 km, which throws away goal
     327); **foveation on by default** (23.8% real, visible degradation real, and on a desktop the
     eyes roam); **geometric canopy motion** (a silhouette cannot move without warping traversal);
     **the raster overlay on by default** (the 10 ms synchronous rebuild, not the 0.21 ms of GPU);
     **moving voxel grass** (no published way to animate stored ray-marched voxels, and this pass did
     not invent one); **rivers you can see** (Prompt 006's channels are sub-cell at a 16 m field);
     **d3d12 at the fps target** (it was outside before this prompt and removing every feature does
     not bring it in); **a threaded grass-instance rebuild** (the fix for 339's hitch); and
     **reversed-Z** (goal 330 assessed it — nothing z-fights, so the benefit is unclaimable until
     something distant needs composing).

### AN-F. Found in passing, filed rather than fixed

346. [ ] **Voxel ground cover costs 4.9x the collision time AT A POSE STANDING IN IT, taking it from
     inside the 0.20 ms budget to ~2x over — and essentially nothing at a walking pose.** Measured at `macro_tree` with the harness's own `--ramp` so both arms
     share a build and a pose: shipped default **0.3465-0.4299 ms/tick at 151.9 nodes/query**;
     `--grass-radius 0` **0.0688-0.0791 ms/tick at 85.9 nodes/query**; `--skeleton-radius 0`
     **0.3485-0.3546 at 151.9 nodes/query, i.e. no effect at all** — so goal 338 is the whole cause
     and goal 336 is not implicated. `nodes/query` reads identically on vk and d3d12, which is what
     places the cause in the octree's contents rather than in timing noise.
     The mechanism is the one Prompt 003 recorded for deep water: `overlaps_solid` early-outs on the
     first SOLID voxel, and `GrassBlade` is `Phase::Foliage` deliberately, so a 24 m ring of
     occupied-but-not-solid voxels turns every query into a descent that finds nothing and runs to
     exhaustion.
     **The scope is narrower than that sounds, and checking it was the point.** Three walking/flying
     scenarios measure INSIDE budget on the shipped default — `clip_stress` 0.0562, `walk_cliff`
     0.0392, `fly_transect` 0.0233 ms/tick at 26.7/34.3/36.2 nodes/query — all below even
     `macro_tree`'s grass-OFF 85.9. The same A/B at `clip_stress` moves nodes/query **26.7 -> 25.7 and
     time 0.037 -> 0.033**: grass is **3.7%** of the visits there against **77%** at `macro_tree`. It
     needs a body standing still in dense cover. `macro_tree`'s grass-free baseline is itself 3.3x
     `clip_stress`'s, so that pose is the deeper one before grass enters.
     **Node visits rise 1.77x while time rises 4.9x** at `macro_tree`, so the added descents are
     individually more expensive too — measured, not yet explained.
     The likely fix is a node-summary bit answering "is anything in this subtree SOLID", so a
     foliage-only brick is skipped whole — the same shape of answer `caves_possible_in_band` already
     is for caves. **Nothing gates on collision cost today** (`app_run.cpp` logs it at Error level and
     no scenario asserts it), which is why this would otherwise have shipped as a number nobody read;
     a gate belongs with the fix.

## Sources

Every technical detail in groups C–F, I, and L traces to the extended research task completed this
session (John Lin's actual technique — real-time path tracing with 5-bounce GI over per-voxel
materials, confirmed from his own feature-description text; DiligentFX PostProcess modules'
standalone usage via `DiligentSamples/Tutorial27_PostProcessing`; baked voxel-AO's per-face-corner
subtlety from thenumb.at's Exile write-up and the original 0fps.net technique; fresnel water and
exponential/height fog formulas; CI caching, shallow-clone, software-Vulkan, and sanitizer-
combination guidance) — see that report for full citations rather than repeating them per-goal here.
Groups H and J trace to this session's own direct reading of the real repository (unzipped and read
file-by-file, not inferred from status reports) — `docs/progress.md` names the specific files read.
Group V traces to this session's own direct reading of `WorldLoader`/`ChunkVoxels`/`mesh_extractor`
(the three inefficiencies were found, not guessed) plus a `web-researcher` cross-check against
production voxel-engine prior art (Veloren, Sodium, Cuberite, Vercidium) and MSVC's own documented
`_ITERATOR_DEBUG_LEVEL`/RelWithDebInfo default-flag behavior — full citations in
`research/chunk-generation-optimization-log.md`. Groups W–Y trace to the pasted micro-voxel research
brief (Laine & Karras 2010; Kämpe, Sintorn & Assarsson 2013; Careil et al. 2020 HashDAG; Crassin et
al. 2009 GigaVoxels; Dolonius et al. 2017) and to this session's own direct reading of every module
it touched — the brief's stack (Rust/VoxelHex) did not match this repository's, so its techniques
were re-implemented, not adopted; `research/micro-voxel-pivot-log.md` has the decisions and numbers.
Groups Z–AC trace to this session's own measurements (the CPU reference tool with `--lod-center`,
the debug views, a shader-return-line bisection with pixel-row sampling, and a slow-frame
attributor over 900-frame autofly runs), a live web pass on Lin's renderer / voxel anti-aliasing /
LOD shadow acne (ESVO's `Raycast.inl`, Fessler's Derived Surface Shading, Gustafsson, Binks, Playdead
TRAA), and a read-only survey of every hardcoded material site — `research/lin-look-log.md`.
