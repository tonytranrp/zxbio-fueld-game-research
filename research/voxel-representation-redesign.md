# Voxel Representation Redesign
Not a `goals.md` group on its own — this is the design record for a real architectural pivot, the
same weight `PHASE_1_BRIEF.md` once carried for the rendering research. `docs/goals.md` gets the
resulting execution items (§9, continuing from goal 112 — 105–111 are still open and untouched).
`docs/progress.md`'s architecture section gets a one-line pointer here once this lands.
## 0. What's actually being asked, disambiguated
Four separate, real complaints, and they don't all have the same fix — worth separating before
reaching for one:
1. **"It's just normal meshes, smooth, not micro voxel like John Lin's."** Image 1 (this engine,
   current) is a continuous, rounded Naive-Surface-Nets landscape. Image 2 (the reference) reads as
   discrete, cube-scale elements — the red berries are individually recognizable as small voxel
   clusters, not a smooth blob. This is a **meshing-algorithm** complaint.
2. **"Huge lag from generating chunks when moving/sprinting."** This is the **streaming-during-
   gameplay** complaint — confirmed directly in the pasted log: frame time collapses to 1–2 fps with
   hundreds of chunks queued (`641 in flight`, `999.0 ms worst`) exactly when movement speed outpaces
   generation throughput.
3. **"Static instead of infinite, ~8km, already generated on load."** A **world-topology** change —
   bounded and pregenerated instead of an unbounded streaming window.
4. **"Modular block system with real properties, not one file with no properties."** A **data-model**
   complaint about `world/chunk/material.hpp`, confirmed directly by reading it — it's an 8-value
   enum and nothing else.
Plus one implicit request threaded through all of them: **"use RAG/RAGSO/DAGVSO for lowest memory
footprint."** These aren't quite real names, but they're pointing at something real — **sparse voxel
octrees (SVO) and sparse voxel DAGs (SVDAG)**, researched in depth below (§4). Confirmed real,
current academic work, not a garbled non sequitur.
## 1. The load-bearing insight: three separate axes, not one
Treating "make it look like micro-voxels" as "shrink the voxel size" would multiply memory by the
*cube* of the size reduction and make §3's world-size goal materially harder for no real reason. Three
genuinely independent questions, each answered on its own terms:
- **Does it *look* like it's made of voxels?** — a meshing-algorithm question (§2). Answer: swap
  Naive Surface Nets for blocky/greedy meshing. Doesn't require touching voxel resolution at all.
- **How fine is the voxel grid?** — a resolution question, answered *differently for terrain than for
  small objects* (§2.3). Terrain keeps its current scale; small decorative elements (flowers, the
  berries in image 2) get modeled at a finer *local* resolution, the same way `tree_decoration.cpp`
  already places a bounded number of small objects per chunk — just with voxel-cube geometry instead
  of primitive shapes.
- **How much memory does storing it cost?** — a compression question (§4), and the one SVO/SVDAG
  actually answers. Real, cited numbers below, not a guess.
## 2. Rendering: blocky/greedy meshing replaces Naive Surface Nets
### 2.1 The swap, concretely
Per-voxel-face meshing instead of iso-surface extraction: for each solid voxel, emit a quad for each
face exposed to air (not solid), skip faces between two solid voxels entirely. This is real,
well-established, and has a name and real numbers — the Rust `block_mesh` crate documents two
concrete algorithms with measured throughput: **naive visible-face culling** (~40 million quads/sec,
single core, 2.5GHz i7) and **greedy meshing** (merges coplanar same-material faces into larger quads
— roughly a third the quad count of naive culling, at about 3× the generation cost). Greedy meshing is
"probably what Minecraft actually uses" per the same reference — the standard choice once naive
culling's quad count becomes the bottleneck rather than generation time.
**This directly fixes the aesthetic complaint**: image 1's rounded slopes become the terraced,
angular silhouette every real cube-based voxel game has — visibly built from discrete units, matching
what image 2's berries already look like at small scale.
### 2.2 What this touches, and what it doesn't
`world/meshing/mesh_extractor.cpp`'s actual algorithm changes (the dual/quad-connection Surface-Nets
logic is replaced by per-face emission), but the interface it presents (`MeshData` — vertices/
indices) and everything downstream of it — the compressed `GpuVertexCompressed` format, the PSO, the
bloom/water/sky/fog passes documented in `docs/render-pipeline.md` — are unaffected. This is a
meshing-layer swap, not a renderer rewrite. Existing tests (`test_surface_coverage.cpp`,
`test_normal_continuity.cpp`, `test_sliver_hunt.cpp`) get rewritten against the new algorithm's actual
correctness properties (face-normal-per-quad is trivial and exact for blocky meshing — there's no
averaging-across-differently-angled-triangles question a smooth mesher has), not preserved as-is.
**The AO scheme changes shape, doesn't disappear.** Baked voxel-neighbor AO (already in the 4th
vertex byte per `docs/progress.md`) is *more* natural here than it was for Surface Nets — this is
exactly the technique's original context (0fps.net's write-up is specifically about Minecraft-style
blocky worlds), so this is a simplification, not new work.
### 2.3 The micro-voxel look for small objects, specifically
Terrain's voxel size doesn't change — only the meshing algorithm does. Small decorative objects
(flowers, berries, small rocks) get a *second*, finer voxel grid local to each object (e.g. an 8×8×8
or 16×16×16 micro-grid spanning the object's own small bounding box, greedy-meshed the same way),
placed by the same deterministic scatter `tree_decoration.cpp` already implements. This is bounded,
cheap (a handful of small grids per chunk, not a global resolution increase), and it's the actual
mechanism that produces image 2's individually-recognizable berry cubes — they're small objects
modeled at object-local fine resolution, not evidence the whole world needs 10× smaller voxels.
## 3. World: static and bounded, sized from real numbers
### 3.1 A real comparable project, not a guess
A current (Jan 2025), independently-built voxel game — 32×32×32 chunks, **the exact chunk size this
project already uses** — deliberately chose a finite world "because finite worlds are just as, if not
more, interesting than infinite ones... instead of exploring outward, you explore inward." World size
1024×128×1024 voxels (32×4×32 chunks); pregeneration "takes only a few seconds... on a separate
thread." Its own author's caveat, equally worth having: naively rendering the *entire* 1024×128×1024
world at once (no culling/LOD) "would dip below 60fps" — even a modest finite world needs the
existing frustum culling this project already has, not a reason to skip bounding the world.
### 3.2 Sizing 8km honestly, with the arithmetic shown
At the current `kChunkSize = 32` and an assumed ~1 voxel ≈ 1 meter (the terrain amplitude and chunk
scale in the existing code are consistent with this, though it's worth confirming explicitly rather
than assumed): 8km = 8000m → **250 chunks per horizontal axis**, 250² = **62,500 columns**, × 6
vertical band layers (the existing `[y_min,y_max] = [-3,2]`) = **375,000 chunks total**. Compare
against the real reference project's 32×32 = 1,024 columns (6,144 chunks with their 4-layer height) —
**8km at this project's current parameters is roughly 60× more columns** than the comparable
real-world example that itself needed culling to hold 60fps.
**The honest recommendation**: don't commit to 8km blind. Build the static-world pipeline (§3.3–3.4)
parameterized by a `kWorldRadiusChunks` constant, prove it first at a size close to the real
reference point (e.g. **48×48 columns ≈ 1.5km per side**, roughly 14,000 chunks — a real, measured
generation-time and memory number at a size close to a project that's already been shown to work),
then scale the constant up and re-measure before committing to 8km. This is the same "measure before
trusting a number" discipline this project has applied to every real decision so far (the hash-map
benchmark overturning the reasoned lean, the ~80× `ChunkStore::find` fix, the winding-bug bisection).
If 1.5km's real numbers hold up cleanly, doubling toward 3km and then 8km is a parameter change and a
re-run, not a redesign.
### 3.3 Pregeneration replaces streaming-during-gameplay
`world::streaming::ChunkStreamer`'s desired-set/hysteresis machinery (built for a camera continuously
moving through an unbounded world) is the wrong tool for a fixed, known-in-advance chunk set — it
solves "what should be loaded right now" for a set that changes every frame; a static world's set
never changes after generation. Replace it with a **one-time parallel generation pass**: enqueue every
chunk coordinate in the fixed world bounds to the existing `ThreadPool` (moodycamel-backed,
`concurrency-and-parallelism.md` §4) at startup, let it saturate every worker thread, and only start
the interactive camera loop once generation (and meshing, and GPU upload) has actually completed —
this is what directly eliminates complaint 2's mid-gameplay lag spikes, since there's no more
generation happening *during* play at all.
### 3.4 What survives from the streaming system, and what doesn't
**Survives**: frustum culling (`render/diligent/src/frustum_cull.cpp` — needed regardless of world
topology, confirmed necessary even by the real reference project's own naive-rendering caveat); the
GPU buffer pool and upload-budget work from the hardening pass (a large one-time upload burst at
load time benefits from exactly the same pacing logic that was built for streaming bursts); the
`ChunkStore`/`CoordMap` storage layer itself (§4 builds on it, doesn't replace it).
**Retired**: `ChunkStreamer`'s desired-set diffing, the two-radii spatial hysteresis, and the delayed-
unload timer — all solve a "what should load/unload *right now*" problem a static world doesn't have.
Don't keep this code paths active-but-unused; remove it deliberately (§9) rather than leaving a dead,
untested subsystem for a future reader to wonder about.
## 4. Storage: what SVO/SVDAG actually are, real numbers, and a phased plan
### 4.1 The real terms, and the real numbers (not the garbled acronyms, the actual things)
Confirmed directly from a peer-reviewed source (Villanueva, Marton & Gobbetti, *Symmetry-aware Sparse
Voxel DAGs*, Journal of Computer Graphics Techniques, 2017) — real, dated, cited figures:
| Structure | Size at 64K³ res, ~6B voxels | Size at 256K³ res, ~100B voxels |
|---|---|---|
| Dense array | (infeasible — not stated, would be many TB) | (infeasible) |
| Sparse Voxel Octree (SVO) | 16.2 GB | 31.1 GB |
| Sparse Voxel DAG (SVDAG) | 167 MB (**~97× smaller than SVO**) | 1.0 GB (**~31× smaller**) |
| Symmetry-aware SVDAG | <86 MB, 0.123 bits/voxel | <575 MB, 0.048 bits/voxel |
**The load-bearing detail, easy to miss**: plain SVO alone is *not* the dramatic win — it's still
tens of gigabytes at high resolution. The DAG step (deduplicating identical/similar subtrees into a
shared graph) is where the order-of-magnitude compression actually comes from. Recommending "an SVO"
without the DAG step would be recommending the less-effective half of the technique.
**A real, separate caveat worth having before it's a surprise**: these numbers compress *geometry*
(occupancy). A 2018 Chalmers thesis on this exact topic notes that a naive per-voxel color (3 bytes)
"renders useless" the benefit of compressing geometry down to a fraction of a bit per voxel — colors/
materials need their *own* compression scheme layered on top, not a free ride on the DAG's geometry
compression. This project already has exactly that scheme — the palette compression from
`M1_2_BRIEF.md` — which is precisely the right complementary technique, not a coincidence.
### 4.2 The honest scope call: don't build SVDAG construction speculatively
Full SVDAG construction (subtree hashing/deduplication, GPU-friendly pointer encoding) is genuine
research-grade work — the citations above are a conference paper and a full thesis on refining it
further. Building it before it's *needed* would be exactly the kind of premature complexity this
project's own standing discipline (the SSAO no-go in `docs/render-pipeline.md`, the "measure before
optimizing" rule throughout) argues against.
**Phase 1 (do this now, real and simple):** apply the *existing* per-chunk homogeneous-fast-path
palette compression (already built, already proven — a fully-air or fully-stone chunk already costs
near-zero) across the *entire static world* rather than a streaming window. For a natural terrain
world, most chunks are either deep-underground-solid or high-sky-air — exactly the case the existing
scheme already collapses to near-nothing. Measure the real total footprint at the §3.2 trial size
before assuming more compression is needed at all.
**Phase 2 (a real, named stretch, not built yet):** if Phase 1's measured number is still too large at
the target world size, a **sparse chunk-grid** (skip allocating storage for any chunk whose material
is already known to be uniform from generation, rather than generating-then-collapsing) is the next
real lever, before reaching for full octree/DAG machinery.
**Phase 3 (the evidenced, cited, but genuinely advanced upgrade):** full SVO→SVDAG conversion, if
Phase 1 and 2's real numbers at the actual target world size still don't fit — at that point the
§4.1 table is exactly the citation to build from, and the Chalmers thesis is the concrete reference
for handling colors/materials alongside the compressed geometry.
## 5. Startup: multithreaded pregeneration with real loading feedback
"No signs of life while Diligent loads" is being mis-attributed — Diligent's own device/PSO
initialization (confirmed by direct reading of `main.cpp`'s `run()`) is not the slow part; it's a
window, a swap chain, and one PSO. The actual dominant cost, especially once §3's static world lands,
is **world generation** — and §3.3 already made that a one-time, parallel, front-loaded pass rather
than spread across gameplay. That pass needs to be *visible*, not silent.
**Concretely**: create the window and initialize Diligent first (fast, per the above — this also means
there's something to draw *into* immediately). Kick off the full-world generation pass (§3.3) on the
`ThreadPool` right after. While it runs, draw a minimal loading screen every frame — a progress bar
(chunks completed / total, a number the job system already has) via the same ImGui overlay
infrastructure already wired up for the debug overlay, not a new UI system. Only enter the interactive
camera loop once generation, meshing, and initial GPU upload have actually finished. This is the same
pattern the Godot forum's real, standard answer to this exact question describes: a full-screen
loading UI, hidden once loading completes, fed by chunk-completion progress from a background thread
— not a novel invention, a well-worn pattern applied here.
## 6. A real, modular, property-driven block system
### 6.1 What's there now, confirmed by direct reading
`world/chunk/include/world/chunk/material.hpp` is exactly what the complaint describes: an 8-value
`enum class MaterialID : uint8_t` and nothing else. Every actual *property* a material has — its
color (`pso_terrain.cpp`'s `kMaterialColors`), whether it's water for gameplay purposes (scattered
`== MaterialID::Water` comparisons per the progress log's swimming/ground-query work), whether trees
can grow on it (height/slope masking in `tree_decoration.cpp`) — lives externally, redundantly, and
without a single place that defines "what this material actually is."
### 6.2 The redesign: one table, real properties, everything else derives from it
```cpp
// world/chunk/include/world/chunk/block_type.hpp
struct BlockProperties {
    glm::vec3 base_color;
    bool is_solid;          // participates in occupancy/collision
    bool is_liquid;         // buoyancy applies (Group K's swimming), not ground-clamp
    bool supports_growth;   // trees/decoration may place on top
    float hardness;         // reserved -- mining/destruction, not yet built, but the property
                             // belongs here now rather than bolted on externally later
    // extend here, not by adding a new scattered comparison elsewhere in the codebase
};
inline constexpr std::array<BlockProperties, kMaterialCount> kBlockTable = {{
    /* Air    */ {{0,0,0},       false, false, false, 0.0f},
    /* Stone  */ {{0.55,0.55,0.58}, true,  false, false, 1.5f},
    /* Dirt   */ {{0.45,0.32,0.18}, true,  false, true,  0.5f},
    /* Water  */ {{0.13,0.35,0.72}, false, true,  false, 0.0f},
    // ...
}};
constexpr const BlockProperties& properties_of(MaterialID id) {
    return kBlockTable[static_cast<std::size_t>(id)];
}
```
A `constexpr` table, not a runtime registry with virtual dispatch — matches `templates-and-
metaprogramming.md`'s own guidance (§2, CRTP/static-polymorphism territory: the full set of materials
is known at compile time, small, and fixed; a registry with dynamic registration is solving a problem
this project doesn't have). Every scattered `== MaterialID::Water` comparison becomes
`properties_of(id).is_liquid`; the shader's color array is generated *from* `kBlockTable` (a single
source of truth) instead of a hand-duplicated parallel array that has to be kept in sync by comment
discipline alone (the current `pso_terrain.cpp`/`terrain.psh.hlsl` "update both together" comment is
exactly the discipline a single source of truth removes the need for).
### 6.3 Why not a virtual `Block` class hierarchy (the Minecraft-style pattern)
Named and rejected with a reason, not silently skipped: Minecraft's own `Block` class hierarchy is a
real, well-known pattern, but it's built for a *mod-extensible*, *runtime-registered* block set — a
problem this project doesn't have (materials are a small, fixed, compile-time-known set, and there's
no plugin/mod loading system per `modular-architecture.md` §3, not needed here either). A `constexpr`
table gets the same "one real source of truth per property" benefit at zero runtime cost and zero
indirection — worth revisiting only if this project ever actually grows a runtime-extensible content
pipeline, which is a real, separate, much bigger decision, not one to build toward speculatively now.
## 7. Reframing the memory number, honestly
700MB in a **Debug** build is not the voxel-storage number it's being read as. The debug overlay's own
already-logged numbers (from the pasted run) show chunk GPU memory in the **single-digit megabytes**
even with hundreds of chunks loaded (`5.0 MiB GPU`, `6.7 MiB GPU`) — the *voxel/mesh data itself* is
already small and already tracked. 700MB total process footprint in Debug is consistent with MSVC's
iterator-debug-level overhead, unoptimized allocation patterns, DiligentEngine's own debug-mode
instrumentation, and Tracy's client buffers — normal Debug-build weight, not evidence the storage
design is inefficient. The real target for §4's compression work is the **static world's total voxel
footprint at the §3.2 trial size, measured in a Release build** — a different, specific number,
worth measuring directly once §3 lands rather than inferred from an unrelated Debug-mode total.
---
## 8. Sequencing
Dependency-real, not arbitrary: §6 (block properties) first — it's small, self-contained, and §2's
new mesher wants `properties_of(id).is_solid` for face-exposure testing anyway, so building it first
means the mesher rewrite consumes a real API instead of a placeholder. §2 (meshing) second. §3
(static world) third — it needs §2's mesher already producing correct output to validate against.
§5 (loading screen) is really part of §3's own delivery, not separate. §4 (storage) is explicitly
phased and gated on §3's real measured numbers, per §4.2 — don't start Phase 2/3 speculatively.
---
## 9. New goals (continuing `docs/goals.md` from 112 — 105–111 stay open, untouched)
**Group P — Modular block properties (§6)**
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
**Group Q — Blocky/greedy meshing (§2)**
117. Rewrite `world/meshing/mesh_extractor.cpp`'s core algorithm from Naive Surface Nets to
     per-voxel-face emission (naive visible-face culling first — simpler, and a real, valid
     stopping point on its own per the research). **Check**: a hand-constructed single-solid-voxel
     case produces exactly 6 correctly-wound outward faces (the same kind of golden-value test
     Group A's original meshing work used).
118. Add greedy face-merging on top of 117 (per `block_mesh`'s documented approach: expand a face in
     each valid direction while neighbors share material and remain exposed). **Check**: a flat,
     uniform-material region of N×N voxels produces measurably fewer quads than the naive version —
     a real quad-count number, not assumed reduced.
119. Rebuild `test_surface_coverage.cpp`/`test_normal_continuity.cpp`/`test_sliver_hunt.cpp` against
     blocky meshing's actual correctness properties (exact face normals, no averaging question) —
     don't keep smooth-meshing-era tests that no longer test anything meaningful for this algorithm.
120. Re-verify the padded cross-chunk sampling / chunk-boundary logic (`docs/progress.md`'s own
     account of this being "the actual hard part" historically) still produces seamless boundaries
     under face-based meshing — the boundary-ownership rules differ from Surface Nets' dual-cell
     scheme and need re-deriving, not assumed to carry over. **Check**: the cross-chunk seam test
     from goal 119's rebuild, specifically exercised at a chunk boundary.
121. View a dumped frame of the new blocky terrain directly (the standing methodology from the last
     pass) — confirm it actually reads as "made of voxels" per the original complaint, not just that
     it compiles and renders something. **Check**: a viewed image, compared side by side with image 1
     from this conversation, with a one-sentence honest assessment.
122. Benchmark meshing throughput before/after (Google Benchmark, the existing `bench_mesh_extract`
     harness) — face-based meshing has a different cost profile than Surface Nets' padded-sampling
     approach, worth a real number rather than assumed faster or slower.
123. Re-tune baked AO (§2.2) for the blocky context specifically — confirm the existing 4th-vertex-
     byte scheme still reads correctly per-face rather than assumed unaffected by the meshing change.
**Group R — Micro-voxel decorative objects (§2.3)**
124. Design the local micro-grid format for small decorative objects (size, e.g. 8³/16³; how it's
     authored — procedural per-object-type generator, not hand-authored per-instance). **Check**: the
     design is written down, including how it composes with `tree_decoration.cpp`'s existing
     deterministic placement rather than replacing it.
125. Implement at least one micro-voxel decorative object type (a flower/berry cluster, per image 2)
     using 124's format, greedy-meshed via Group Q's mesher and appended into the chunk's mesh the
     same way trees already are. **Check**: view a dump; individually recognizable small cube
     clusters, matching image 2's read, not smooth geometry.
126. Confirm the memory cost of 125 is genuinely bounded (a handful of small grids per chunk, not a
     hidden global resolution increase) — measure it directly rather than assuming §1's "bounded and
     cheap" framing holds without checking.
**Group S — Static, bounded world (§3)**
127. Add `kWorldRadiusChunks` (or equivalent bounds) as a real, named config constant, set to the
     §3.2 trial size (48×48 columns) first — not 8km, not yet.
128. Replace `ChunkStreamer`'s per-tick desired-set/hysteresis logic with the one-time parallel
     generation pass from §3.3 — enqueue every in-bounds coordinate to the `ThreadPool` at startup.
     **Check**: every chunk in bounds is generated exactly once, verified by a count, not assumed.
129. Remove `world::streaming::ChunkStreamer`'s now-dead spatial/temporal hysteresis code path
     deliberately (§3.4) — don't leave it inert and untested. **Check**: `git diff` shows real
     deletion, not a disabled-but-present code path; the removed tests are removed, not skipped.
130. Wire the loading-screen progress feedback from §5 to the generation pass's real completion
     count. **Check**: view a screenshot of the loading screen mid-generation — a real, moving
     progress indicator, not a static "Loading..." string.
131. Measure real wall-clock generation time and real memory footprint (Release build specifically,
     per §7's reframing) at the 48×48 trial size. **Check**: both numbers recorded in
     `docs/progress.md`, with the methodology (build config, machine) stated.
132. Decide, from 131's real numbers, whether to scale `kWorldRadiusChunks` toward the original 8km
     ask, and by how much per step — re-measuring at each step per §3.2's explicit plan, not jumping
     straight to the final number. **Check**: each size step has its own recorded measurement.
133. Re-run `--autofly`-equivalent movement through the now-static, fully-generated world and confirm
     the original stutter complaint is actually gone — frame time should show no generation-driven
     spikes at all post-load, since nothing generates during play anymore. **Check**: a worst-frame
     number from a full traverse of the loaded world, compared against the pre-redesign log's
     collapse-to-1fps behavior.
134. Full regression run once Groups P–S land together. **Check**: real pass count, stated explicitly.
**Group T — Storage compression, phased (§4)**
135. Measure Phase 1 (§4.2) directly: the existing per-chunk palette compression's real total memory
     across the full static world at the §3.2 trial size — this may already be small enough that
     Phase 2/3 are unnecessary, and that's a real, good outcome to confirm rather than assume needs
     more work.
136. Only if 135's number doesn't fit a reasonable budget: implement Phase 2's sparse chunk-grid
     (skip storage entirely for chunks known-uniform at generation time). **Check**: a real before/
     after memory number, same standard as every other optimization claim in this project's history.
137. Only if 136 still doesn't fit: scope Phase 3 (real SVO→SVDAG construction) as its own dedicated
     design pass — cite §4.1's real numbers as the starting evidence, and treat it with the same
     research depth this project gave the original rendering-API pivot, not a quick bolt-on. **Check**:
     this becomes its own goals group, written before implementation, matching goal 108's own
     precedent for "big enough to deserve its own group."
**Group U — Consolidation**
138. Update `docs/progress.md`'s architecture section to reflect the post-redesign shape (blocky
     meshing, static world, the block-properties table) — the same discipline goal 97/100 already
     established for keeping that section accurate rather than stale.
139. Full visual review against both images from this conversation specifically — does the redesigned
     engine's own capture read closer to image 2's aesthetic than image 1 did. **Check**: a written,
     honest, specific comparison, not a generic "looks better."
140. Write up what changed and why in one place for a future session that wasn't part of this
     conversation — the same "conclusions, not just a list of things touched" standard the last
     consolidation pass set.
---
## Sources
Villanueva, Marton & Gobbetti, "Symmetry-aware Sparse Voxel DAGs (SSVDAGs) for compression-domain
tracing of high-resolution geometric scenes," *Journal of Computer Graphics Techniques* 6(2), 2017
(the SVO/SVDAG/SSVDAG memory figures in §4.1, and the CRS4 I3D 2016 paper it extends); Dan Dolonius,
"On sparse voxel DAGs and memory efficient compression of surface attributes," Chalmers licentiate
thesis, 2018 (the color/material-compression caveat in §4.1); the `block_mesh` Rust crate's own
documented benchmarks (naive vs. greedy meshing throughput, §2.1); a real, current (Jan 2025)
r/VoxelGameDev post describing a 32×32×32-chunk finite voxel world, its pregeneration-on-a-thread
approach, and its own naive-rendering performance ceiling (§3.1); a Godot Engine forum thread's
standard answer for a threaded world-generation loading screen (§5).
**In-repo**: this document extends `docs/progress.md` (the current architecture it's revising) and
`docs/render-pipeline.md` (the render pass structure Group Q's meshing swap must preserve);
`M1_2_BRIEF.md` §1 (the palette compression scheme §4's Phase 1 reuses directly);
`concurrency-and-parallelism.md` §4 (the `ThreadPool` §3.3's pregeneration pass is built on);
`templates-and-metaprogramming.md` §2 (the CRTP/compile-time-known-set reasoning behind §6.3's
constexpr-table-over-virtual-hierarchy decision); and `modular-architecture.md` §3 (why a plugin-style
runtime block registry is named and correctly not what this project needs).

---

## §10. What actually happened (retrospective, goal 140)

Written for a future session that wasn't part of implementing this — conclusions, not a diff list.
Everything below is real, verified evidence; the numbered goals in `docs/goals.md` (112–140) carry
the specific tests/commands/commits if you need to reproduce any of it.

**Groups P, Q, and S — the three groups answering the four original complaints — are done and
verified.** Group R (micro-voxel decorative objects) is designed but not built; Group T's phase
gate was checked and correctly stayed closed.

- **Group P (modular block properties)** was small and clean, as expected: `world/chunk/
  block_type.hpp`'s `kBlockTable` is now the one place a material's color/solidity/liquidity live.
  The real finding here was negative, and worth keeping: grep-verifying which scattered
  comparisons actually needed migrating found that swimming and tree placement, despite this
  document's own §6.1 assumption, were already purely height-based with no material access at
  all — nothing to migrate there. Don't trust a design doc's own claims about existing code over a
  grep; even a document written from direct reading can still be wrong about a detail.
- **Group Q (blocky/greedy meshing)** was the highest-risk, highest-value piece, and it worked on
  the first real build. The winding derivation (algebraic, not eyeballed) held. The real
  correctness lesson was that greedy merging invalidates an entire *style* of test (vertex-
  proximity probing), not just specific test cases — three test files needed rebuilding around
  footprint/bounding-region checks, and doing that rebuild surfaced a genuine, separate bug in one
  of the OLD tests itself (sampling a boundary's short-neighbor side instead of the solid source
  column), not in the new mesher. The lesson generalizes: when an algorithm's fundamental output
  SHAPE changes (dense-everywhere vertices → sparse-at-corners-only), audit every test's
  assumptions about that shape, don't just re-run the old suite and patch failures individually.
- **Group S (static, bounded world)** is where the actual reported pain (lag while moving,
  streaming during gameplay) got fixed, and it's backed by the most dramatic before/after evidence
  in the whole pass: exact chunk-count equality (1014/1014) and a 38ms worst-frame across an entire
  autofly traverse, against a pasted log showing collapse to 1-2fps. It's also where the least
  glamorous but most consequential bug of the whole redesign was found: a budget floor
  ("drain at least 25% of the backlog") that was correct and tested at the OLD system's few-
  hundred-chunk scale became nearly unbounded at the new ~15,000-chunk scale, and this was caught
  only by actually running `--frames 60` at radius 20 and noticing the ENTIRE load finished anyway.
  **The general lesson, worth carrying into any future scale-up**: a tuning constant's own
  justification comment can be correct and the constant can still fail silently at 30x the scale it
  was proven at — re-derive, don't just re-trust, when the input scale changes by an order of
  magnitude or more.
- **The "48×48" trial size was itself ambiguous** (side vs. radius — see goal 131's note) and
  resolved toward the larger, more conservative reading rather than being silently "corrected" to
  match the letter of the original phrasing, since real, good data already existed for the larger
  interpretation.
- **The literal 8km world-size ask is real, measured, and not reached**: two Release-build data
  points (not one extrapolated guess) show ~1ms/chunk near-linear generation cost, and that rate
  puts an 8km-equivalent world at several minutes of load time. The shipping default (radius 48,
  ~3.1km per side, 53.6s) is a deliberate stopping point with the reasoning attached, not a
  quiet failure to reach the original number. If a future session wants to push further, the
  starting question is "where does the ~1ms/chunk actually go" (profiled, not guessed) — the
  scaling data here rules out "just raise the radius" as sufficient on its own.
- **Group R was scoped and then deliberately deferred**, not forgotten: its design
  (`research/micro-voxel-object-design.md`) is real and specific enough to implement directly from,
  but implementation was judged optional polish against the four originally reported complaints,
  all already addressed by P/Q/S. A future session picking this up should start at goal 125.
- **Group T's gate stayed shut, correctly**: the real measured number (343 MiB for the entire
  static world's voxel storage) comfortably fits, so Phase 2 (sparse chunk-grid) and Phase 3
  (SVO/SVDAG) were never started. This is the redesign doc's own hoped-for outcome (§4.2), not a
  shortcut — don't build compression machinery a real measurement says isn't needed yet.

**If you're picking this up cold**: read `docs/progress.md`'s current-state section first (it's
the accurate, current summary), then this document for the *why* behind anything that looks like
an unusual choice, then `docs/goals.md` groups P–U for the specific evidence behind each claim.
