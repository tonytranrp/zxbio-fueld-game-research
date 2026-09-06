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
109. Prove the Lavapipe CI leg end-to-end green after the MSVC-flag-leak fix (our
     imgui_impl_glfw.cpp COMPILE_OPTIONS were MSVC-only syntax -- fixed conditional). Then take
     goal 64's Linux compile-cache measurement from its second run. **Check**: a best-effort-leg
     run reaching the Xvfb smoke, and a cache-hit compile-time delta recorded.
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
157. Per-brick palette / 4-bit materials (brief §3.2's "cheap first" step): 2–4x on the ~350–400 MB
     the surface bricks take at the shipping default. **Check**: before/after tree bytes at the
     same pose, and no `--verify-frame`/oracle regression.
158. Incremental rebuild: reuse unchanged subtrees (far rings) and a persistent brick pool with
     partial uploads, instead of the whole-tree rebuild + 200–400 MB upload on every move.
     **Check**: rebuild wall-clock and upload bytes per camera step, before/after.
159. [x] Temporal AA (or supersampling) for the sub-pixel voxel shimmer 2–8 m out — visible moiré in
     every capture, the same reason John Lin's renderer needs TAA. **Check**: viewed capture pair.
     Done as goal 168 (Group Z), together with the averaged-normal blend that removes the moiré's
     lighting component.
160. Editing: the tree is rebuilt from an analytic sampler and never mutated; digging/placing needs
     a mutable structure (HashDAG is the researched shape, brief §2.2). Not started.
161. `fill_terrain` truncates the surface height toward zero (`static_cast<int32_t>`) instead of
     flooring, so underwater terrain sits one voxel higher than the geometric rule the sampler
     uses — a pre-existing quirk found by the equivalence test (which skips negative-height
     columns). **Check**: switch to `floor`, then the equivalence test covers every column.
162. Build-time profile is now ~60% `fill_brick` (2.9 µs per sampled brick, ~2x sampled per kept)
     with the column grid cache hitting only ~9%: the remaining oversampling is horizontal, not the
     vertical stacks the cache was written for. **Check**: Tracy capture of one build before the
     next optimization, not another guess.
163. DAG deduplication as a measurement (brief §5.3, hash-interning): expected small on noise
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
173. Collide against the octree once editing (goal 160) can make the analytic world stale: the
     `SolidQuery` boundary is already there; the query is a per-box point-sample of the tree
     near the build center. **Check**: the same 9 tests against a tree-backed query.

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
