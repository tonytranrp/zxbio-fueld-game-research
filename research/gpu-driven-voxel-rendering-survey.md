# Research Report: GPU-Driven Voxel/Chunk Terrain Rendering in Vulkan

Subagent C from `PHASE_1_BRIEF.md` §9, completed 2026-09-02. Web research (Exa), covering
GPU-driven chunk rendering, additional real sources beyond the three named in the brief,
chunk-boundary/LOD stitching, and cross-frame compute-culling synchronization.

## Task 1: vblanco20-1/vulkan-guide and Project Ascendant — Deep Dive

Read the published guide text at vkguide.dev across five chapters: Draw Indirect, Compute-based
Culling, GPU Driven Rendering Overview, and both Project Ascendant chapters (voxel/mesh rendering,
and framegraph/synchronization). Also pulled the `Project-Ascendant` repo's own README directly
from GitHub.

### The base tutorial's GPU-driven pipeline (general, not voxel-specific)

**Indirect buffer layout.** The indirect buffer holds a packed array of
`VkDrawIndexedIndirectCommand` (indexCount, instanceCount, firstIndex, vertexOffset, firstInstance)
— the standard Vulkan struct. Critically, the guide notes the buffer doesn't have to be *just* that
struct: "you don't need to have the data be a packed array of command structs... as long as you set
the offset and stride correctly. In the engine we store extra data in the buffer." This is the hook
Project Ascendant later uses (see below). The buffer is created with
`VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | STORAGE_BUFFER_BIT` so a compute shader can write it
directly on the GPU.

**Compute culling core (`indirect_cull.comp`).** Each thread reads one `objectID`/`batchID` pair
from an `instanceBuffer`, tests visibility, and if visible does
`atomicAdd(drawBuffer.Draws[batchIndex].instanceCount, 1)` to reserve a slot, then writes the
surviving `objectID` into a separate `finalInstanceBuffer` at `firstInstance + countIndex`. The
vertex shader later indexes into `finalInstanceBuffer` via `gl_InstanceIndex` to fetch per-object
data. This is explicitly a workaround for *not* using `VkDrawIndexedIndirectCommandCount` (the
tutorial targets Nintendo Switch, which lacks it) — visibility feeds back purely through the
atomic-incremented `instanceCount` field embedded in each draw's own command struct.

**Frustum + occlusion culling.** `IsVisible()` does sphere-vs-frustum tests in view space, then (in
the fuller pipeline) occlusion culling against a depth pyramid built by repeated compute-shader mip
reduction of the previous frame's depth buffer (1-frame latency, explicitly acknowledged). The
guide attributes the underlying frustum-cull math to "Arseny [Kapoulkine]'s open source Niagara
stream." Published, attributed numbers from this base engine: 125,000 objects culled across
main+shadow views at 290 FPS / 40M+ triangles on an RTX 2080, CPU cull cost <0.5ms on a Ryzen 1700;
250,000 draws at 60fps on Switch, 500fps on PC. These are general-scene numbers, not voxel-chunk
numbers.

### Project Ascendant — the voxel/chunk-specific content

Ascendant is vblanco20-1's own follow-on prototype ("open world voxel RPG," MIT-licensed,
github.com/vblanco20-1/Project-Ascendant), Vulkan 1.3-only, runs on Steam Deck). Its voxel renderer
is a direct extension of the tutorial's GPU-driven pipeline:

- **Chunk size tradeoff, stated explicitly and compared to real games.** Ascendant uses 8×8×8-voxel
  chunks. The author explicitly contrasts this with Minecraft's 16×256×16 (now moving toward
  16×16×16 subchunks) and Vintage Story's 32×32×32, and states the general tradeoff plainly: "the
  bigger your chunks, the fewer draws you have, but also your draws become bigger so they cull
  worse. There isn't really an optimal chunk size."
- **Memory strategy: a pre-allocated "gigabuffer."** A single 400MB buffer allocated at startup,
  sub-allocated via VMA's Virtual Allocation feature, so chunk data can use 32-bit offsets into one
  buffer rather than 64-bit buffer-device-address pointers — a deliberate choice against BDA for
  this subsystem.
- **Concrete chunk-draw struct layout** (directly answers the buffer-layout question for the voxel
  case):
  ```
  struct ChunkDrawInfo { ivec3 position; int16_t type; int16_t drawcount; int32_t index; };
  struct DrawBlock { uint32_t packed; }; // flags:4, pos:12, type:16 — one solid surface voxel
  struct ChunkDrawIndirect {
      uint32_t indexCount, instanceCount, firstIndex; int32_t vertexOffset; uint32_t firstInstance;
      int32_t chunkx, chunky, chunkz; // extra fields packed right into the indirect command
  };
  ```
  Confirms in a real voxel-specific implementation exactly the pattern the base tutorial only
  described abstractly: the indirect-draw struct is extended with extra application data (chunk
  world position) beyond the 5 fields Vulkan requires.
- **Scale that forces GPU culling, with a real number.** "In the Ascendant screenshots shown, there
  can be up to around 400,000 chunks... culling on CPU for such a high number is a non-starter."
  This is the actual, attributed reason GPU-driven culling was adopted for chunk terrain
  specifically, not scene objects in general.
- **Visibility feedback differs from the base tutorial.** Ascendant runs a compute shader that
  "outputs into an indirect buffer + indirect count" — implying it *does* use
  `vkCmdDrawIndexedIndirectCount` (unlike the Switch-constrained base tutorial), and "Shadow passes
  and main view passes reuse the same indirect buffer." Could not retrieve the literal body of this
  voxel-specific cull compute shader — page content ends right at the `[shader("compute")]`
  declaration before showing the function body.
- **No depth-pyramid occlusion culling in the voxel path**, by deliberate choice: "The engine does
  not implement pyramid-based depth culling... I just didn't find need to add that in practice as
  it's more useful if there are cave networks but I don't have those and performance was already
  high." A concrete, attributed engineering decision, not a limitation of the technique.
- **Five separate geometry systems**, not one: near-field SurfaceNets-smoothed voxel meshes (based
  on the Minecraft "NoCubes" mod's technique), far-field voxel-raycast sprite draws (per the paper
  "A Ray-Box Intersection Algorithm and Efficient Dynamic Voxel Rendering" — one quad/point per
  voxel, ray-box intersection in the pixel shader), a 3-camera-facing-quad variant of the same,
  instanced vegetation, and arbitrary GLTF mesh rendering. All funnel through the same
  `BlockRenderer`/gigabuffer/indirect-draw architecture. Presented as the reason the engine moved
  to a deferred renderer — five draw systems, one unified GBuffer write, lighting applied once
  afterward.
- **Barrier and framegraph engineering at scale.** Once the engine had many compute+graphics passes
  (culling, GBuffer, deferred lighting, SSAO, etc.), per-call `vkCmdPipelineBarrier` calls became a
  real problem ("the GPU driver doesn't really like to handle barriers like this... it performs
  better if it does a single VkCmdPipelineBarrier that does multiple barriers at a time"). The fix
  was a `BarrierMerger` utility batching `VkImageMemoryBarrier2`/`VkBufferMemoryBarrier2` arrays
  into one call, and above that a `RenderGraph::Builder` (`AddComputePass`/`AddGraphicsPass`/
  `AddTrackedImage`/`AddTrackedBuffer`) that auto-derives barriers from declared per-pass
  reads/writes, rebuilt fresh every frame for easy feature toggling — explicitly modeled on (and
  citing) EA/DICE's public Frostbite Framegraph talk.

Sources: [Draw Indirect](https://www.vkguide.dev/docs/gpudriven/draw_indirect/),
[Compute based Culling](https://www.vkguide.dev/docs/gpudriven/compute_culling/),
[GPU Driven Rendering Overview](https://www.vkguide.dev/docs/gpudriven/gpu_driven_engines/),
[Project Ascendant — voxel/mesh rendering](https://www.vkguide.dev/docs/ascendant/ascendant_geometry/),
[Project Ascendant — framegraph/sync/lighting](https://www.vkguide.dev/docs/ascendant/ascendant_light/),
[Project Ascendant overview](https://www.vkguide.dev/docs/ascendant/ascendant_overview/),
[Project-Ascendant GitHub repo](https://github.com/vblanco20-1/Project-Ascendant).

---

## Task 2: Additional Real Sources for Chunk/Voxel-Terrain Rendering at Scale

Six sources found beyond the three already known (vulkan-guide, Khronos MDI sample, VulkanMod),
each doing something genuinely different:

**1. Aokana — "A GPU-Driven Voxel Rendering Framework for Open World Games"** (Fang, Wang, Wang;
Fudan University + Harvard; ACM PACMCGIT, 2025). Real academic paper. Built on a Sparse Voxel DAG
(not a brick/octree-of-meshes like Ascendant or a dense texture like the raymarchers below), with
an integrated LOD mechanism and streaming system for seamless map loading. **Attributed performance
claim** (from the abstract, single source, unverified independently): "reduce memory usage by up to
ninefold and achieves rendering speeds up to 4.8 times faster than those of previous state-of-the-art
approaches" as voxel scene resolution increases, targeting "tens of billions of voxels." Genuinely
different because it's designed to be dropped into existing engines and integrated with mesh-based
rendering, and its unit of GPU-driven work is a DAG-compressed sparse structure, not per-chunk mesh
draws. [arXiv:2505.02017](https://arxiv.org/html/2505.02017v1) /
[ACM DOI](https://dl.acm.org/doi/10.1145/3728299).

**2. Roblox's voxel terrain system — SIGGRAPH 2020 talk by Arseny Kapoulkine** (zeux.io, Roblox's
Chief Rendering Engineer). Real, shipped, massive-scale production system — 100M+ MAU, 5M+ CCU,
targeting 10+ km² scale terrain from an iPad 2 to desktop, D3D9/11/OpenGL/Metal/Vulkan backends.
Genuinely different: a sparse multi-resolution voxel grid where each chunk stores a full mip
pyramid (1³ to 32³, top mips skippable, streamed in/out under memory pressure) rather than one
fixed-resolution mesh; a CPU-side "dual method" mesher inspired by Dual Contouring and Naive
Surface Nets (not GPU compute); single draw call per chunk via a material atlas/texture array.
Concrete attributed data-layout numbers: 20 bytes/vertex packed format, empty/full mip = 1 byte,
compressed rows = 1 byte/row. Doubly relevant — see Task 3 for its LOD-seam technique.
[zeux.io/data/siggraph2020.pdf](https://zeux.io/data/siggraph2020.pdf).

**3. paulrobello/voxel-world** — open-source Rust + Vulkan (`vulkano` crate) voxel sandbox.
Genuinely different technique: **zero mesh generation at all** — "all rendering handled by Vulkan
compute shaders — there is no traditional vertex/fragment pipeline," blocks ray-marched directly
out of a 3D texture. Streams a 512×512×512-block resident window infinitely along X/Z via
origin-shift operations, per-chunk dirty tracking cited as "~32 KB vs 32 MB full-world upload"
(attributed bandwidth-reduction ratio, not FPS). Solo/hobby project, not shipped commercially —
flagged explicitly. No FPS numbers found.
[github.com/paulrobello/voxel-world, ARCHITECTURE.md](https://github.com/paulrobello/voxel-world/blob/main/docs/ARCHITECTURE.md).

**4. GigaVoxels — SIGGRAPH 2009** (Crassin, Neyret, Lefebvre, Eisemann, Sainz; INRIA/NVIDIA).
Foundational, heavily-cited academic system, genuinely different in kind: **pure GPU raycasting**,
not rasterized indirect-draw chunks at all. Sparse voxel octree of "brick pool"
(opacity/color/normal bricks) + "node pool," driven by a CUDA hierarchical volume ray-caster
(one thread per ray, KD-restart traversal, ray-driven per-ray LOD) with a GPU cache manager
streaming bricks on demand. No specific performance number captured (slides cut off at "Rendering
cost"). Relevant mainly to the eventual streaming milestone, not the current fixed-LOD
indirect-draw milestone.
[GigaVoxels SIGGRAPH09 slides, INRIA](https://artis.inrialpes.fr/Publications/2009/CNLSE09/GigaVoxels_Siggraph09_Slides.pdf).

**5. Teardown / Voxagon dev blog — Dennis Gustafsson, "From screen space to voxel space"** (2018,
pre-dating Teardown's 2020 release — the primary technical source, not press coverage). A third
genuinely different technique: full raytracing in a *dense* 3D voxel texture (not sparse octree,
not indirect-draw mesh), with a separate 1-bit-per-voxel octree-packed, mip-mapped "shadow texture"
for empty-space skipping, plus a "supercover" traversal algorithm for watertight results. Attributed
numbers: a 100×100×25m world at 5cm resolution = 2 billion voxels (2GB naive), reduced to 292MB via
the bit-packed octree + 2 mip levels. Dynamic objects CPU-rasterized into the world texture via
`glTexSubImage3D` each frame.
[blog.voxagon.se](https://blog.voxagon.se/2018/10/17/from-screen-space-to-voxel-space.html).

**6. Cubyz** (PixelGuys/Cubyz, open-source, Zig, actively maintained). Real, playable, shipping
open-source voxel game — valuable as *corroboration rather than a different technique*:
independently converges on essentially the same compute-cull → indirect-draw chunk architecture as
vulkan-guide/VulkanMod (`fillIndirectBuffer.comp`, frustum + occlusion culling into a GPU command
buffer, greedy meshing, a 32-level LOD system, mesh updates time-budgeted to 12ms/frame). **Verified
caveat**: supports both OpenGL 4.3 (Windows/Linux default) and Vulkan as selectable backends; the
specific chunk-culling code read references `glMultiDrawElementsIndirect` (OpenGL) — confirmed for
the OpenGL path only; whether the Vulkan backend implements the identical chunk-culling shader was
not verified.
[Cubyz chunk meshing & LOD](https://deepwiki.com/PixelGuys/Cubyz/4.2-chunk-meshing-and-lod),
[Cubyz graphics/Vulkan](https://deepwiki.com/PixelGuys/Cubyz/4.4-graphics-context-and-vulkan).

Also found `AurelienLeandri/VulkanCulling` (not clearly voxel-specific) and Sascha Willems'
`computecullandlod`/`indirectdraw` examples (general-scene, cited under Task 4 instead).

---

## Task 3: Chunk-Boundary/LOD Stitching for Smooth-Voxel Terrain

Four real sources, four genuinely different solutions to the same crack/seam problem:

**1. The Transvoxel Algorithm — Eric Lengyel, 2009/2010.** The canonical, most widely adopted
solution, invented for Lengyel's own C4 Engine, later formalized academically ("Transition Cells
for Dynamic Multiresolution Marching Cubes," peer-reviewed, 2010). Mechanism: special "transition
cells" are inserted between regular Marching-Cubes cells exactly along the boundary where
full-resolution voxel data meets half-resolution data. Rather than handling all combinatorial cases
of both resolutions at once (~1.2 million), it samples only 9 points of the high-resolution side,
yielding 512 cases that collapse into 73 equivalence classes via lookup tables. Because each
transition cell needs only *local* voxel data, retriangulation is fast enough for real-time,
dynamically-edited (destructible) terrain — directly relevant since Transvoxel targets the same use
case this engine will eventually need (editable smooth terrain with streaming LOD). Sources:
[transvoxel.org](https://transvoxel.org/), the [published Transition Cells paper](https://doi.org/10.1080/2151237x.2011.563682),
and the reference [lookup-table implementation](https://github.com/EricLengyel/Transvoxel).

**2. Nick Gildea's "Dual Contouring: Seams & LOD for Chunked Terrain"** (2014, independent
practitioner blog). Genuinely different mechanism: each chunk builds an explicit **shared seam
octree** — collects octree leaf nodes on its own max-boundary faces, plus the corresponding
boundary leaf nodes from face-, edge-, and corner-adjacent neighbors (7 neighbors in 3D, explicit
min/max-coordinate selection predicates per direction, working C++11 code given for
`FindSeamNodes`), then contours *that combined seam octree* separately so each chunk remains
responsible for its own local piece of every seam. Author traces the lineage to Miguel Cepero's
"Procedural World" blog and states it underpins EverQuest Next/Landmark (now defunct). Verified the
same-topology neighbor-node-sharing mechanism for eliminating cracks between chunks directly; the
*differing*-resolution-neighbor (LOD) extension is stated by the author but not independently
confirmed here (page continues beyond what was fetched).
[ngildea.blogspot.com](http://ngildea.blogspot.com/2014/09/dual-contouring-chunked-terrain.html).

**3. Roblox terrain, SIGGRAPH 2020 (same talk as Task 2 #2) — the "skirts" approach.** A third,
genuinely different, notably *cheaper* technique, from a team that explicitly considered and
rejected exact triangulation: "Ideally, we'd generate triangles to match... but this is expensive
and complicated." Instead: "skirts" — one extra layer of triangles per chunk edge, from one extra
layer of voxels, patched onto the mesh with a depth bias in the vertex shader "so skirts are only
visible in gaps." Not topologically exact, but cheap enough to generate unconditionally for every
chunk. Implementation detail: because each chunk has up to 3 relevant stitch directions (X, Z,
rarely Y), they pack a special index buffer layout `[stitchX][base][stitchZ][stitchY]` so the base
mesh plus any active stitches render in a **single draw call**.
[zeux.io/data/siggraph2020.pdf](https://zeux.io/data/siggraph2020.pdf).

**4. godot_voxel — real-world evidence Transvoxel is harder to get right than the algorithm alone
suggests.** `Zylann/godot_voxel` is a real, shipping, actively-maintained (4k stars) open-source
Godot module implementing `VoxelMesherTransvoxel` over a quantized SDF voxel store. A real, publicly
tracked issue — [#440, "Transvoxel gaps between lod levels in godot 4"](https://github.com/Zylann/godot_voxel/issues/440)
(opened Sept 2022, closed resolved) — documents visible seam gaps in production, root-caused to the
**shader-side vertex-offset seam-smoothing pass** (a required custom `ShaderMaterial` nudging seam
vertices) misbehaving specifically on axis-aligned planes, not the core Transvoxel triangulation
tables. Workaround: disable the vertex offset entirely ("better uglier seams than holes in mesh").
Valuable precisely because it's a real bug report showing even a mature Transvoxel implementation
has a second, fragile layer distinct from the core algorithm. Supporting doc:
[smooth_terrain.md](https://github.com/Zylann/godot_voxel/blob/master/doc/source/smooth_terrain.md).

Taken together: exact/lookup-table (Transvoxel), exact/explicit-shared-topology (Gildea's seam
octree), cheap-approximate (Roblox skirts), and a real-world failure mode of the exact approach
(godot_voxel #440) — useful framing for whichever tradeoff a future streaming milestone picks.

---

## Task 4: Compute-Shader Chunk Culling and the Frames-in-Flight Model

**The core barrier, stated identically by two independent official sources.** Between a compute
shader writing the indirect-draw buffer and the indirect draw command reading it, the required
dependency (`VK_KHR_synchronization2` terms):
```
srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
dstStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
```
submitted as a single `vkCmdPipelineBarrier2`. Stated in the official Vulkan Documentation
Project's "Indirect Dispatch" chapter ("Failure to include this barrier will result in the GPU
reading 'garbage' or stale data, leading to incorrect dispatches or even device crashes"), and the
closely analogous case is a canonical worked example on the official
[Synchronization Examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html)
page, which also recommends batching multiple resource barriers into one global memory barrier
rather than many small ones. vulkan-guide's own Ascendant chapter independently arrived at the same
conclusion in production (the `BarrierMerger` utility, Task 1), citing Frostbite's Framegraph talk.
Sources: [Indirect Dispatch](https://docs.vulkan.org/tutorial/latest/Advanced_Vulkan_Compute/07_GPU_Driven_Pipelines/02_indirect_dispatch.html),
[GPU-Side Command Generation](https://docs.vulkan.org/tutorial/latest/Advanced_Vulkan_Compute/07_GPU_Driven_Pipelines/03_gpu_side_command_generation.html),
[Multi-Draw Indirect](https://docs.vulkan.org/tutorial/latest/Advanced_Vulkan_Compute/07_GPU_Driven_Pipelines/04_multi_draw_indirect_mdi.html),
[Synchronization Examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html).

**Double/triple-buffering the indirect buffer itself across frames-in-flight — yes, a well-
established pattern, confirmed by an official Khronos sample.** The
[Khronos Vulkan-Samples "GPU Rendering and Multi-Draw Indirect" sample](https://docs.vulkan.org/samples/latest/samples/performance/multi_draw_indirect/README.html)
describes the most advanced of three draw-generation modes, "GPU using buffer device address": the
indirect-command array's starting address is passed to the culling compute shader via
`buffer_reference`/push constants rather than a bound descriptor set, specifically so "each
invocation of the culling compute shader can point to a different indirect command array without
needing to change descriptor sets," with the explicit payoff: **"This allows culling of the next
frame to occur prior to completion of rendering of the current frame with minimal overhead."** That
is exactly the double-buffered-indirect-buffer pattern, from an authoritative source, with an
explicit mechanism (BDA-addressed per-frame buffer selection) for why it's efficient.

This composes with the general N-buffering pattern: vulkan-guide's own
["Double buffering"](https://www.vkguide.dev/docs/chapter-4/double_buffering/) chapter establishes
the baseline idiom — a `FrameData` struct holding every per-frame GPU-touched resource, in a fixed
array of size `FRAME_OVERLAP` (typically 2), indexed by `frameNumber % FRAME_OVERLAP`. The
[LunarG "Frames in Flight Demystified" talk](https://www.lunarg.com/wp-content/uploads/2026/02/Charles-Giessen-LunarG.pdf)
(Vulkanised 2026) frames this as "Multiple Buffering": a stage cannot read from and write to the
same data buffer simultaneously across overlapping frames, so the standard fix is a distinct buffer
per in-flight frame slot. No single source found stating "always N-buffer your indirect-draw
buffer" as a rule distinct from "N-buffer every per-frame-written GPU resource" — but the Khronos
sample's explicit description, combined with the general FrameData pattern applying to every other
per-frame-written buffer in the same engines, is about as directly evidenced as this gets without
reading raw source line-by-line.

**Alternative to N-buffering: fence or timeline-semaphore gating of a single buffer.** Reusing one
buffer but blocking its next write until the GPU finishes the previous read — what a `VkFence` or a
**timeline semaphore** is for: "the CPU can wait for the GPU to reach value N, knowing that it's now
safe to reuse a buffer that was used by the command that signaled value N... This replaces the need
for VkFence in many scenarios." Trades memory (1 buffer vs. N) for a potential pipeline stall —
precisely what N-buffering exists to avoid — so N-buffering is the better default unless
memory-constrained.
[Timeline Semaphores: Unified Synchronization](https://docs.vulkan.org/tutorial/latest/Advanced_Vulkan_Compute/08_Asynchronous_Compute/03_timeline_semaphores.html).

**If the culling compute dispatch runs on a separate (async-compute) queue from the graphics queue
doing the draw**, an additional queue-ownership-transfer handshake applies on top of the stage/
access barrier above: a "release" barrier on the source (compute) queue and a matching "acquire"
barrier on the destination (graphics) queue (`srcQueueFamilyIndex`/`dstQueueFamilyIndex`),
coordinated via semaphores. Lets the GPU's hardware scheduler stall the graphics queue only at the
exact pipeline stage that needs the compute result, hiding compute cost behind geometry-heavy
graphics work — though the tutorial cautions the benefit is hardware-dependent (unified mobile GPUs
may just interleave) and should be profiled, not assumed. Unnecessary if compute and graphics stay
on the same queue, which is what vulkan-guide's own engine and the Khronos MDI sample both do — only
relevant if a future milestone specifically adopts async compute for culling.
[Concurrent Execution and Synchronization 2](https://docs.vulkan.org/tutorial/latest/Advanced_Vulkan_Compute/08_Asynchronous_Compute/02_concurrent_execution.html).

**A concrete, runnable reference implementation exists but its internals weren't verified.** Sascha
Willems' open-source Vulkan examples repository has both
[`indirectdraw.cpp`](https://github.com/SaschaWillems/Vulkan/blob/master/examples/indirectdraw/indirectdraw.cpp)
and
[`computecullandlod.cpp`](https://github.com/SaschaWillems/Vulkan/blob/master/examples/computecullandlod/computecullandlod.cpp)
(compute-shader culling + LOD selection writing an indirect buffer) — confirmed to exist and read
the blog announcement's description, but did not trace whether the example N-buffers its indirect
buffer across frames-in-flight.

---

## Summary of What's Still Genuinely Uncertain

- Ascendant's actual voxel-chunk cull compute shader body (buffer layout and design narrative
  confirmed; function body's page content cut off).
- Nick Gildea's blog: same-topology seam-octree mechanism confirmed in detail; the
  cross-resolution (differing-LOD) extension of that mechanism not independently confirmed.
- No specific FPS/performance number found for GigaVoxels (slides cut off) or paulrobello/
  voxel-world (no benchmarks published).
- Whether Cubyz's Vulkan backend (vs. its default OpenGL backend) implements the same
  chunk-culling compute shader — not verified.
- Whether Sascha Willems' `computecullandlod` example N-buffers its indirect buffer across
  frames-in-flight — not verified, flagged rather than guessed.
- vulkan-guide's claims about Assassin's Creed Unity, Frostbite/Dragon Age Inquisition, and
  Rainbow Six Siege using these techniques are stated by vkguide as historical context, not
  independently traced to a primary source from those studios — treat as single-source/unverified
  as reported by vkguide.
