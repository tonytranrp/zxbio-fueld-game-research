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
[M. Material palette expansion](#m-material-palette-expansion) · [N. Consolidation](#n-consolidation)

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

41. Decide, in writing, whether Stage 4 is worth its cost right now: SSAO needs a genuinely new
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
53. Re-run the full `benchmarks/` suite against the post-refactor render path and compare to the
    saved baselines in `benchmarks/baselines/` (the project's own established convention). **Check**:
    a new dated baseline file, compared via Benchmark's own `tools/compare.py`, not eyeballed.

## H. Code quality

54. Full read-through of `world/meshing/src/mesh_extractor.cpp` specifically (the file that's had the
    most real bugs found in it historically — the boundary-vertex gap, the `NeighborCache`
    throughput fix) for anything else worth hardening now that AO (goal 12) is adding a second real
    piece of logic to the same padded-sampling pass — two independent concerns sharing one pass is
    worth a deliberate look, not assumed fine because each individually has tests. **Check**: a
    written note of what was reviewed and what (if anything) changed.
55. Audit every `throw std::runtime_error` / defensive check added across this project's history
    (there are several, per direct reading — missing SRB variables, failed buffer/shader creation)
    for consistency: do they all get caught somewhere sensible (currently `main()`'s single
    `catch (const std::exception&)`), or would a more specific error path help diagnose a real
    failure faster. **Check**: a written assessment, changed only if a concrete improvement is found,
    not refactored for its own sake.
56. Review `chunk_streaming.cpp`/`chunk_streamer.cpp` for the same "does this scale sensibly" question
    Group T's stutter work already asked of the upload path — now that Stage 1–4 add real per-vertex
    work (AO, color jitter, wind animation) to every chunk's mesh generation, re-confirm the
    generation-side job-time budget assumptions the streaming system's timing (unload delay, in-
    flight limits) were originally tuned against. **Check**: a real before/after generation-time
    number with Stage 1–3's additions included, not assumed unchanged from the original tuning.
57. Check whether `world/chunk`'s `CoordMap`/`CoordSet` boost-backed aliases are used consistently
    everywhere a chunk/streaming coordinate gets stored, or whether any newer code (trees, the
    upcoming G-buffer/pass bookkeeping) introduced a fresh `std::unordered_map`/`std::map` instance
    that should go through the same hardened alias instead. **Check**: a grep for raw
    `std::unordered_map`/`std::map` outside the alias definition itself and outside genuinely
    unrelated uses, each one justified or migrated.
58. Confirm `engine/events`' `entt::dispatcher` usage (chunk lifecycle events, the debug overlay's
    event-vs-poll consistency check) is the pattern reused for any new cross-system notification
    Stage 1–4 or the gameplay goals introduce, rather than a fresh ad hoc callback/polling mechanism
    reappearing. **Check**: any new cross-system notification added by this document's other goals
    goes through `engine::events::Dispatcher`, or a written reason why it doesn't.
59. Review test coverage for gaps specifically in the NEW code this document adds (AO, water,
    fog, foliage variety, any G-buffer work) against the standard the rest of the codebase already
    sets (boundary cases, not just the convenient common case, per `docs/progress.md`'s own
    assessment of the existing suite). **Check**: each new subsystem has at least one test exercising
    a real boundary case, not only a happy-path smoke test.
60. Confirm `.clang-format` (if one exists — check directly rather than assuming) is applied
    consistently across all newly-added files from this document's work; add one now if it doesn't
    exist yet, given the codebase's otherwise-consistent style is worth protecting as more people/
    sessions touch it. **Check**: a formatting pass runs clean (no diff) across the whole tree.
61. Revisit whether `VOXEL_CLANG_TIDY`'s existing exclusions (test directories, per the root
    `CMakeLists.txt`'s own comment) still make sense given the new test surface from goal 59, and
    whether the `/EHsc` restatement workaround documented there is still needed on the current
    toolchain. **Check**: a real clang-tidy run against the current tree, findings triaged (fixed or
    explicitly suppressed with reasoning), not left unrun since it was last wired up.

## I. CI hardening

Grounded in this pass's own research into GitHub Actions gotchas for GPU-adjacent CMake+CPM
projects — the workflow file exists but has never actually run; this group is what makes that first
real run land clean instead of thrashing through avoidable failures one at a time.

62. Split `.github/workflows/ci.yml` into a **core, no-GPU job** (matrix: Windows/Linux ×
    MSVC/GCC/Clang, `-DVOXEL_BUILD_RENDERER=OFF` per the flag this project already has) and a
    separate **renderer job**, rather than one job trying to build and test everything. **Check**:
    the core job runs and passes without ever fetching DiligentEngine/GLFW/Tracy.
63. Add CPM/dependency caching via `actions/cache` keyed on `hashFiles('cmake/Dependencies.cmake')`
    (already the stated intent per `CLAUDE.md`'s dependency-additions note) with
    `-DCPM_SOURCE_CACHE=<cache-dir>` actually passed at configure time — confirm this is really wired
    into the workflow YAML, not just assumed because the intent was written down. **Check**: a second
    CI run on an unchanged `Dependencies.cmake` shows a cache hit (near-zero dependency re-fetch
    time), not a full re-clone.
64. Add ccache (or sccache) caching alongside the CPM cache — compile-artifact reuse is a different,
    complementary cache from the dependency-source cache. **Check**: a second run with only
    application-code changes (no dependency change) shows meaningfully faster compile times than a
    cold run.
65. **Do not shallow-clone DiligentEngine.** It's pinned to a specific commit SHA (`aca2285`), not a
    branch tip — a shallow clone of an arbitrary SHA is a real, documented GitHub/git failure mode,
    not a hypothetical. Confirm the CPM fetch for DiligentEngine specifically does a full clone (or
    clones the branch then checks out the SHA), not `GIT_SHALLOW ON`. **Check**: the actual CPM
    package declaration for DiligentEngine is read directly and confirmed, not assumed safe.
66. Add the renderer job's actual execution environment: **Mesa Lavapipe** (software Vulkan) on the
    Linux leg, **D3D12 WARP** on the Windows leg, since GitHub-hosted runners have no real GPU.
    **Check**: at least one renderer-job test (e.g. `voxel_app --frames 5`) runs to completion on
    both, or is explicitly marked skipped with a written reason if software rendering proves
    infeasible for a specific test.
67. Treat software-Vulkan CI results as best-effort, not a guarantee of real-hardware parity (a real,
    documented risk per the research — Lavapipe/SwiftShader have known rough edges) — don't gate
    merges on renderer-job flakiness the same strictly as the core job. **Check**: the workflow's
    branch-protection expectations (if any) reflect this distinction explicitly.
68. Add ASan+UBSan as one CI job (they combine safely) and **TSan as a separate job** (mutually
    exclusive with ASan/UBSan) — this project's own `CLAUDE.md` already documents TSan as locally
    unavailable on the dev machine by all three realistic paths, which makes a Linux CI TSan job the
    actual way this project ever gets TSan coverage, not an optional nice-to-have. **Check**: the
    TSan job actually runs (not just declared) and its result is reported, even if slower — budget
    for the researched 5–15× TSan slowdown rather than being surprised by CI runtime.
69. Add clang-tidy as its own CI job using `CMAKE_EXPORT_COMPILE_COMMANDS` + `VOXEL_CLANG_TIDY`
    (both already exist in `CMakeLists.txt`), excluding `_deps`/third-party sources explicitly.
    **Check**: the job runs against real project sources only — confirmed by checking the actual
    file list clang-tidy processes in a run, not assumed correct because the exclusion flag is set.
70. Keep the sanitizer and static-analysis jobs from blocking the fast core-job signal — run them in
    parallel, not as sequential gates (this project's own reference material already states this
    convention; confirm the actual workflow YAML follows it). **Check**: the workflow graph shows
    these as parallel jobs, not a chain.
71. Do the actual first real run on a real GitHub runner (not a local `act` simulation, which
    doesn't catch every runner-environment difference) and fix whatever breaks — expect at least one
    real surprise given this has never executed for real; that's the point of doing it now rather
    than assuming the YAML is correct because it looks right. **Check**: a real, green (or explicitly
    triaged, not silently ignored) run, linked/referenced in `docs/progress.md` once done.
72. Once green, add a CI status badge to whatever now serves as the project's top-level readme/intro
    (or `docs/progress.md` itself) — a cheap, standard signal that the pipeline this group hardened
    is actually being exercised going forward, not a one-time proof.

## J. Deferred-item cleanup

Named explicitly in `CLAUDE.md` as deferred, not forgotten — this group is where they get picked up.

73. In-app RenderDoc capture trigger: vendor `renderdoc_app.h` (the missing piece per `CLAUDE.md`)
    and wire the in-application capture-trigger API. **Check**: a hotkey during a live run produces a
    RenderDoc capture file, openable in the standalone RenderDoc UI.
74. `tools/mesh_dump`'s `.obj` export (named as deferred from the very first phase of this project) —
    implement it now that the mesh format has stabilized through Stage 1's AO-data addition, so it's
    built against the current vertex shape rather than needing a second pass later. **Check**: a
    dumped `.obj` opens correctly in a separate viewer (Blender/MeshLab), matching the in-engine
    render of the same chunk.
75. Revisit the RG16-normal-format A/B named in `CLAUDE.md` as "if slope banding ever bothers" — now
    that Stage 1's warmer lighting and hemisphere ambient (goal 14) make normal-quality issues more
    visually apparent than flat ambient did, actually check whether banding is now visible rather than
    leaving the question open indefinitely. **Check**: a viewed dump of a smooth slope under the new
    lighting, a specific yes/no on visible banding, and the A/B only actually run if the answer is
    yes.
76. Confirm whether `backward-cpp` (researched, pinned, but not wired in per `CLAUDE.md`'s crash-
    handler section) is worth removing from `Dependencies.cmake` entirely now that the custom
    handler covers its use case, versus keeping it pinned as a documented fallback — a dependency
    that's fetched but unused either earns its keep with a clear reason or should go. **Check**: an
    explicit decision, written down, either way.
77. Check whether `unordered_dense` (kept, per `CLAUDE.md`, "ONLY for the comparison harness" after
    losing the hash-map benchmark) is still needed now that the decision is made and documented in
    `docs/progress.md` — a losing candidate kept only for a comparison that already happened is a
    candidate for removal, not permanent residency. **Check**: same standard as goal 76.
78. Re-run the full benchmark suite (`benchmarks/`) now that Stage 1–4's shader/mesh changes exist,
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
80. Decide explicitly whether cave/overhang terrain (true 3D density rather than the current 2D-
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
83. Review whether the spectator camera's fly-mode speed/boost values still feel right once terrain
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
86. Document the actual current gameplay loop honestly in `docs/progress.md` once this group's items
    land — "fly or walk around generated, streaming, forested terrain with water" is genuinely what
    exists; resist the temptation to describe it as more feature-complete than it is.
87. Identify, concretely, what "a game" still needs beyond what exists after this document's other
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
97. Update `docs/progress.md`'s architecture section once the palette count changes, so it stays an
    accurate current-state summary rather than silently drifting stale (the exact failure mode that
    made the original six-brief sprawl hard to navigate).

## N. Consolidation

98. Full-suite regression run across everything this document touches — confirm the count only goes
    up from the last reported 70/70, with any net-new test count stated explicitly.
99. Full visual review: view dumps covering every material, the sky, water, fog at distance, a dense
    forest, and a steep slope, in one sitting, as the real "does this read as progress toward
    colorful/John-Lin-adjacent" check — not each piece in isolation as it was built, which is how the
    original ribbon bug survived two "verified" passes. **Check**: a written one-paragraph honest
    assessment, referencing the specific viewed images, not a generic "looks good."
100. Update `docs/progress.md`'s "Current state" section to reflect everything this document's groups
     actually landed — and explicitly mark anything from groups A–N that was decided *against*
     (goal 41's SSAO gate, goal 76/77's dependency removals, goal 80's cave decision) so a future
     session doesn't re-litigate a settled question from scratch.
101. Re-open this same `docs/goals.md` file and add whatever new goals this pass's own work
     surfaced — the standing expectation for a living backlog, not a one-shot list that goes stale
     the moment it's first executed.
102. Confirm every "Check" across groups A–N that was actually performed is genuinely reflected as
     done (`[x]`) — and, just as importantly, confirm nothing is marked done whose check wasn't
     actually performed, matching this document's own standing methodology note rather than treating
     it as aspirational.
103. Write the equivalent of a "what problems does the code have now" honest note (mirroring
     `docs/progress.md`'s equivalent section for the pre-this-document state) — the same kind of
     direct, specific assessment this document opened by giving the *existing* code, now applied to
     what this document's own work added.
104. One last full-scene view, from a genuinely fresh eye if possible — the single check that
     catches what an implementer, close to each individual change, is most likely to miss.

---

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
