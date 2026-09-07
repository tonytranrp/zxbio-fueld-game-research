# GPU Voxel Streaming, Resident Caches, Traversal Efficiency, Non-Perturbing Measurement, and Automated Render Regression Testing

**Provenance.** Written 2026-09-06 by the side session for Prompts 002 and 004. One read-only
web-research subagent ran with live Exa search/fetch against a brief naming this engine's actual
architecture (sparse-brick octree, 8³ bricks, 7.8 mm voxels, 512 m root, one fullscreen pixel
shader marching two `StructuredBuffer<uint>`s, whole-tree rebuild every 2 m of camera motion,
200–400 MB re-upload in 32 MB slices, 76 fps ground level). Every claim below carries a label and
a URL.

**Labels.** `[CONFIRMED]` = verbatim text or an exact number from the primary source (paper,
vendor doc, or the author's own writing). `[INFERENCE]` = the researcher's reading, or mine.
`[UNVERIFIED]` = widely repeated or plausible, no primary source reached.

**Two honest gaps in this document, stated up front:**

1. The extraction tool caps PDF text at ~10 k characters from the start of a document. Abstracts,
   introductions, related-work sections and figure captions were retrieved verbatim; **the deep
   method sections of the GigaVoxels I3D 2009 paper (§3–§6) and the ESVO beam-optimization and
   results sections were NOT retrieved verbatim.** Where that matters it is marked.
2. **A stretch of the report was lost in transit** — the remainder of §3 after the Nanite quote
   (including the Teardown / Dennis Gustafsson material) and §4.1–§4.4 (the bulk of the
   compute-shader-vs-pixel-shader comparison). The subagent could not be re-queried. Those two
   topics are therefore **the first things a follow-up research pass should re-run**; §4.5–§4.7 and
   the synthesis survived and are reproduced below, and the Teardown facts that appear elsewhere in
   the report are marked `[CONFIRMED as reported]` because they reached this document second-hand.

---

## 1. GigaVoxels (Crassin, Neyret, Lefebvre, Eisemann — I3D 2009)

### 1.1 The paper's own claims

`[CONFIRMED]` Abstract: *"We demonstrate our approach in several scenarios, like the exploration of
a 3D scan (8192³ resolution), of hypertextured meshes (16384³ virtual resolution), or of a fractal
(theoretically infinite resolution). All examples are rendered on current generation hardware at
20-90 fps and respect the limited GPU memory budget."*
— https://maverick.inria.fr/Publications/2009/CNLE09/CNLE09.pdf

`[CONFIRMED]` **The single most directly relevant sentence in the literature to this engine's
current architecture**, verbatim from §1: *"Even if we were able to iteratively fill the GPU memory
in a brute-force manner, the transfer of 512MB each frame (which is the standard memory size on
current GPUs) already prevents real time performance."* — same URL.

`[INFERENCE]` This engine re-uploads 200–400 MB per rebuild in 32 MB/frame slices. The GigaVoxels
authors rejected exactly this class of solution as non-real-time in 2009, on hardware whose PCIe
bandwidth was ~4–8 GB/s. A PCIe 4.0 x16 laptop link is ~25 GB/s, so 400 MB is ~16 ms of pure
transfer at theoretical peak — one to two whole frames of a 150 fps budget spent on the bus alone,
before any of the CPU rebuild cost.

`[CONFIRMED]` The predecessor tech report (INRIA RR-6567, June 2008, *Interactive GigaVoxels*):
*"Our method is based on a dynamic generalized octree with MIP-mapped 3D texture bricks in its
leaves. Data is stored only for visible regions at the current viewpoint, at the appropriate
resolution... A key originality of our algorithm is that it directly relies on the ray-marcher to
detect missing data. The march along every ray in every pixel may be interrupted while data is
generated or loaded."* Datasets rendered *"at an interactive frame-rate of 10 to 20 fps."*
— https://inria.hal.science/inria-00291670v2/file/RR-6567.pdf

### 1.2 Node pool and brick pool

`[CONFIRMED]` Crassin's own SIGGRAPH 2009 slides: *"Sparse Voxel MipMap Pyramid ... Composed
structure Generalized Octree • Empty space compaction ... Bricks of voxels • Linked by octree
nodes • Store opacity, color, normal ... Brick pool [CUDA 3D] ... Node pool ... Octree of Voxel
Bricks • One child pointer • Compact structure • Cache efficient."*
— https://gigavoxels.inria.fr/Publications/2009/CNLSE09/GigaVoxels_Siggraph09_Slides.pdf

`[CONFIRMED — third-party primary]` Laine & Karras's own reading of it: *"The first stage casts rays
against a regular octree using kd-restart algorithm to avoid the need for a stack. The leaves of
this octree are bricks, i.e. 3D grids, that contain the actual voxel data. When a brick is found,
its contents are sampled along the ray. Bricks typically contain 16³ or 32³ voxels, yielding a lot
of wasted memory except for truly volumetric or fuzzy data. On the other hand, mipmapped 3D texture
lookups supported by hardware make the brick sampling very efficient, and the result is
automatically antialiased."*
— https://users.aalto.fi/~laines9/publications/laine2010i3d_paper.pdf

`[CONFIRMED — secondary, course slides matching the paper's figures]` Node texel layout:
bricks *"are stored in a large shared 3D – Texture (Brick pool)"*, *"Voxel-grid of size M³ (usually
M=32)"*, *"3D-Mip-Mapped"*; the node texel *"Contains (64 bits): 3D Pointer (X,Y,Z) to the next
level in the tree (N³ child nodes); Constant Color or Brick Pointer; Flag indicating whether it is
a leaf node; Flag indicating the node type (Constant Color or Brick pointer)."*
— https://www.cs.cornell.edu/courses/cs6630/2012sp/slides/Joerimann-Robinson-GigaVoxels.pdf

`[INFERENCE]` "3D pointer to N³ child nodes" is the key structural point: children are stored as a
contiguous **node tile**, so one pointer reaches all 8. This is what makes the node pool
cache-friendly, and ESVO and SVDAG use the same trick.

### 1.3 Border / apron voxels

`[CONFIRMED — authors' own tutorial]` Borders exist and are configurable; the exact size table
could not be extracted. — http://gigavoxels.imag.fr/GigaVoxels_Tutorials.pdf

`[UNVERIFIED]` Synthesis: bricks carry a border margin of at least one voxel per axis, duplicating
neighbouring data, so hardware trilinear (and inter-mip quadrilinear) filtering never samples
across a brick boundary. No verbatim sentence retrieved.

`[CONFIRMED — third-party primary, and the important quantitative fact]` Kämpe, Sintorn & Assarsson:
*"Crassin et al. [2011] compute ambient occlusion and indirect lighting by cone tracing in a sparse
voxel octree... The interpolation requires duplication of data to neighbouring voxels, which result
in a memory consumption of nearly 1024 MB for a 512³ sparse voxel octree with materials."*
— https://www.cse.chalmers.se/~uffe/HighResolutionSparseVoxelDAGs.pdf

`[UNVERIFIED wording, CONFIRMED existence]` The 2011 voxel-cone-tracing follow-up changed brick
geometry to reduce this: **3×3×3 bricks with voxel centres at node corners rather than node
centres**, so an interpolated value is always available inside the brick spanning a 2×2×2 node
tile, at less than half the memory of straightforward boundary duplication with identical sampling
precision. The paper reports *"Real-time indirect illumination (25-70 fps on a GTX480)"*.
— https://artis.inrialpes.fr/Publications/2011/CNSGE11b/GIVoxels-pg2011-authors.pdf

`[INFERENCE — directly actionable here]` This engine's bricks are **8³**. A one-voxel apron on every
face makes the stored brick 10³ = 1000 voxels for 512 useful ones — a **1.95× memory blow-up**. At
655 k bricks / 395 MB that is the difference between 395 MB and ~770 MB. The corner-centred 3³
trick, or explicit neighbour fetches instead of hardware filtering, is what the GigaVoxels authors
themselves moved to.

### 1.4 The ray-guided feedback mechanism — exactly what the shader writes

`[CONFIRMED]` Crassin's slides enumerate the per-pass request taxonomy: *"Data request"*, *"Data
requests"*, *"Data request (Constant value)"*, *"(Max opacity)"*, *"Pass 1 (Node not reached)"*,
*"(LoD OK)"*, *"Wrong LOD"*, *"No Data"*. And: *"Ray-based visibility & queries — Zero CPU
intervention — Per ray frustum and visibility culling — On-chip structure management —
Subdivision requests ○ LOD adaptation — Cache management ○ Remove CPU synchronizations."*
— https://gigavoxels.inria.fr/Publications/2009/CNLSE09/GigaVoxels_Siggraph09_Slides.pdf

`[CONFIRMED — the authors' own companion paper, and the clearest statement of the mechanism]` From
*Building with Bricks: CUDA-based Out-of-Core GigaVoxel Rendering* (Crassin, Neyret, Eisemann):

> *"One major property of the GigaVoxel algorithm is that the actual level of refinement is
> associated directly to the results of the rendering. In practice, this has been achieved by
> recovering information about the data that was used by each ray during the traversal of the
> structure. If information is declared missing, the corresponding ray reports this miss. The CPU
> recovers these ray results and processes them. Missing data is uploaded and the octree is
> restructured accordingly from the CPU side. Further, an LRU-caching scheme is applied on the CPU
> side. Basically, each data element obtains a time stamp which is reset upon usage. When new data
> is needed to be uploaded the CPU, it overwrites with preference those elements in GPU memory
> who's usage lies most in the past."*
>
> *"First, we will show how we can decide on the GPU-side which elements are the least used and
> should be replaced if necessary and how to transfer a compact list to the CPU in order to initiate
> the update phase. With this approach, we reduce the throughput of information from the GPU to the
> CPU which is crucial, as the exchange of data between CPU and GPU still represents an important
> bottleneck in the current hardware architectures."*
>
> *"A. GPU-side LRU Caching. Previously, each ray sent back usage information of the entire node
> hierarchy. To reduce bandwidth, only a partial transfer was performed that exploited temporal and
> spatial coherence. In our new extension, we rely on a different mechanism: We maintain a usage
> list that contains all elements in the order of there most recent usage. The oldest elements are
> therefore always those that are situated at the beginning of the list. To make this
> GPU-LRU-scheme possible, we proceed as follows. Each node and data entry has an associated usage
> stamp on the GPU side. During the ray traversal, each ray activates the usage stamps of the
> elements that are visited. During this marking step, we also set a flag that indicates whether or
> not a refinement or data upload is needed. Coarsening of the structure is implicitly handled. It
> is possible to employ a strategy that allows us to avoid any atomic operations in this step. Once,
> all rays finished the traversal, we update the usage list. We flag all the elements in the usage
> list, that have been used in the current frame, which can be tested by relying on the usage
> stamps. This is done by looping over all the usage stamps. Then we perform two stream reductions,
> to separate all elements in the usage list that were used in the current frame from the others."*

— https://www.icare3d.org/research/publications/CNE09/IntelConf_Final.pdf

`[CONFIRMED]` Matching slide text: *"SVMP caches — Two caches on the GPU — Bricks — But also tree —
No maximal tree size ... Usage sorted nodes addresses / Oldest → Newest ... GPU LRU (Least Recently
Used) — Track elements usage — Maintain list with least used in front ... Stream compaction / Used
nodes mask / Stream compaction / Concatenate / New elements / New data ... Minimum amount of data is
loaded — Fully compatible with secondary rays and exotic rays paths — Reflections, refractions,
shadows, curved rays, …"*

**The full documented pipeline:**
1. `[CONFIRMED]` During traversal each ray **writes a usage stamp** on every node tile and brick it
   visits and **sets a flag** if that element needs refinement or upload. *"It is possible to employ
   a strategy that allows us to avoid any atomic operations in this step."*
2. `[CONFIRMED]` After all rays finish, the GPU loops over usage stamps and performs **two stream
   reductions** to split the usage list into used/not-used this frame, then concatenates, so the
   least-recently-used elements land at the front.
3. `[CONFIRMED]` A **compact list** — not per-ray data — is transferred to the CPU, which produces
   and uploads the requested bricks and node tiles into the freed slots.

`[CONFIRMED — secondary, on the *original* I3D encoding]` The 2009 request encoding used **multiple
render targets**: *"Node list is stored in multiple render targets (MRTs) — RGBA32 = 4 x 32 bit —
One node pointer uses 32 bits — One channel per node pointer — Can store up to 12 node id's per
pixel using 3 MRTs."* Compression: *"Spatial node coherence — Neighboring rays traverse similar
nodes — Group in 2x2 grid"*; *"Temporal coherence: Used nodes are similar between subsequent frames
— FIFO (48 items) — 48-element window is shifted after each subsequent frame"*; *"Compaction step by
using Histogram pyramids"*; *"Fit as much as possible in one RGBA32 texture (4 Nodes per pixel) —
Postpone to next frame if the limit is exceeded — Usually 2-3 nodes per pixel are selected."*
— https://www.cs.cornell.edu/courses/cs6630/2012sp/slides/Joerimann-Robinson-GigaVoxels.pdf

`[CONFIRMED — same slides]` The per-frame loop: *"CPU: while (true) { Render image (using the GPU);
Get list of accessed/needed nodes from the GPU; Reset timestamp of accessed nodes; Expand or
collapses nodes; Update GPU memory with needed nodes (LRU) }"* and *"GPU: Fragment shader First
pass: Trace ray; **if LOD not available Pick next higher available level in Mip-map**; Shade pixel;
Keep a list of accessed nodes / Mip-map levels in result textures. Second pass: Compress
accessed/needed data."*

**Crucial behavioural detail.** `[CONFIRMED]` *"if LOD not available → Pick next higher available
level in Mip-map."* The renderer **never stalls and never shows a hole** — it degrades to the
coarsest resident level and files a request. That is the whole design, and it is what makes
"rebuild from scratch" unnecessary.

`[CONFIRMED]` Why the interruptible-ray approach from RR-6567 was abandoned, per the Cornell
summary: *"Interrupting and updating is too slow: Requires lots of CPU interaction (CPU-GPU
bandwidth is limited) → Try to keep all needed data available in the GPU's memory → Render one
frame in one step."*

### 1.5 How a ray picks its LOD (cone / footprint criterion)

`[CONFIRMED]` Crassin's slides: *"For a given pixel: Approximate cone integration ○ Using
pre-integrated data ○ With only one ray! — Voxels can be modeled as spheres — Sphere size chosen to
match the cone ○ Linear interpolation between mipmap levels — Samples distance d — Based on
voxels/spheres size"*, with diagram labels *"Image Plane / Cubical voxel footprint / One pixel
footprint / Pixel Color+Alpha."* Also: *"Volume MipMapping mechanism — Problem: LOD uses discrete
downsampled levels — Popping + Aliasing — ... — Quadrilinear filtering — Geometry is texture — No
need of multi sampling (eg. MSAA)"*. The same mechanism is reused for soft shadows (*"MipMap level
chosen to approximate light source cone"*) and DoF (*"MipMap level based on circle-of-confusion
size"*).

`[UNVERIFIED]` **No closed-form formula was retrieved.** `[INFERENCE]` The formula that follows from
the slides: pick the mip level whose voxel edge ≈ the cone diameter at distance `t`, i.e.
`voxel_edge(level) ≈ 2·t·tan(half_pixel_angle)`, step by `d ≈ voxel_edge(level)`, and interpolate
between the two bracketing levels to avoid popping. **Note that this engine's `tree_builder.hpp`
already implements the same criterion in its build rule** (`target(d) = max(finest, d·finest/
lod_radius)`), and the marcher applies a Laine–Karras early-out on top; what is missing is the
*interpolation between levels*, which is what the slides say prevents popping and aliasing.

`[CONFIRMED — an honest self-critique by the authors that is rarely quoted]` *"But there is a little
problem… Approximate cone integration ○ Using pre-integrated data — **But the integration function
is not the good one!** — Emi/Abs model used along rays ○ But pre-integration is a simple sum —
Result: Occluding objects are merged/blended — Virtually not noticeable for little ray-steps."* And
on lighting: *"**Lighting problem — How to pre-filter lighting? — Pre-filter Normals ○ How to store
them? ○ How to interpolate them?** ... Compute gradients on the fly?"* Their considered fix was
anisotropic pre-integration (*"2D mipmapping — 1 axis kept unfiltered — Interpolate between axis at
runtime"*), rejected on *"Storage / Sampling cost!"* grounds.

`[INFERENCE]` **This is the moiré, named by the original author.** A pre-filtered normal/coverage
attribute averaged over a level is not the correct filtered shading response, and the mismatch
appears as aliasing on high-frequency cube fields. It is this engine's layout-v2 attribute word
(int8×3 normal + uint8 coverage) in a different guise, and Crassin's open question — *how do you
store and interpolate a pre-filtered normal* — is still open.

### 1.6 Empty-space skipping and early termination

`[CONFIRMED]` RR-6567 abstract: *"Ray-marching allows to quickly stops when reaching opaque regions.
Also, we efficiently skip areas of constant density."*

`[CONFIRMED]` The node texel carries *"Constant Color or Brick Pointer"* plus a type flag, so a
uniform region (including empty) is a **terminal node with no brick at all** and the traversal step
is *"Skip Node."* The ray loop is *"Tree Descent → Brick Marching → Skip Node → Per-ray LOD
evaluation → Ray traversal"*, with *"Emission/Absorption model for each ray — Accumulate Color
intensity + Alpha — Front-to-back ○ Stop when opaque."* Traversal: *"Big CUDA kernel — One thread
per ray — **KD-restart algorithm** — Ray-driven LOD."* (Stackless, chosen deliberately for GPU.)

### 1.7 The mip pyramid as anti-aliasing

`[CONFIRMED]` The structure is a *"Sparse Voxel MipMap Pyramid"*; each brick is *"3D-Mip-Mapped"*,
giving quadrilinear filtering. Stated payoff: *"Geometry is texture — No need of multi sampling (eg.
MSAA)"*, and the abstract's *"our rendering provides is inherently anti-aliased."* The pyramid also
buys cheap blur effects: *"Full pre-integrated versions of objects — Idea: Implements blurry effects
very efficiently — Without multi-sampling — Soft shadows — Depth of field — Glossy reflections…"*

### 1.8 Reported performance

`[CONFIRMED]` 20–90 fps on 2009 hardware within a bounded memory budget (abstract). Predecessor
10–20 fps (RR-6567).
`[CONFIRMED — secondary]` *"Explicit volume (trabecular bone) — 8192³ Voxels — 20 – 40 Fps
(Mip-mapping enabled) — 60 Fps (Mip-mapping disabled) — System: Core2 bi-core E6600 at 2.4 GHz &
NVIDIA 8800 GTS 512MB"*; *"Hypertextured bunny — 1024³ Voxels — 20fps"*.
— Cornell slides, URL above.
`[UNVERIFIED]` "Node pool 4 MB (64³ entries), brick pool 430 MB" — could not be extracted from the
PDF. The *shape* (a small node pool and a large brick pool, both fixed size) is `[CONFIRMED]` by the
slides.

### 1.9 Stated limits and what the follow-up literature says is wrong with it

**(a) The authors' own 2024 follow-up names the failure mode.** `[CONFIRMED]` *GigaVoxels DP:
Starvation-Less Render and Production for Large and Detailed Volumetric Worlds Walkthrough*
(Richermoz & Neyret, **HPG 2024**), abstract:

> *"Still, GigaVoxels showed that by using a ray-guided cache to produce and store only visible
> voxels bricks on demand, it is possible to walk through very large and detailed worlds with
> real-time performance in bounded GPU memory. **However, on-demand production of data during
> rendering is still challenging in terms of synchronization and starvation of GPU cores.** We
> propose a new GPU-driven algorithm using dynamic parallelism (DP) to minimize these, and a
> 'GPU-cores timeline' profiling tool to analyze them. **We validate our model with timings
> (2× gain)** and we illustrate it on various scenes."*

— https://hal.science/hal-04654692 (HPG 2024, pp. 1–11, DOI https://doi.org/10.1145/3675389)

`[UNVERIFIED]` The mechanism (alternating render/produce passes leave cores idle; CUDA Dynamic
Parallelism launches production from within the render) is synthesis — the HAL PDF is encrypted.
But the abstract alone is decisive: **a 2× gain, by the original author, entirely about scheduling
the on-demand production rather than about the data structure.**

**(b) Laine & Karras's objection.** `[CONFIRMED]` *"Bricks typically contain 16³ or 32³ voxels,
yielding a lot of wasted memory except for truly volumetric or fuzzy data."* And: *"Our approach is
different from Crassin et al. in several ways. We aim for storing representations of large-scale
scenes in GPU memory with enough detail for high-quality rendering, which necessitates a small
memory footprint. Also, our primary interest is in representing surfaces instead of volumes...
Because of these considerations, we use of a single hierarchical structure instead of separate
schemes for coarse and fine data."*

`[INFERENCE]` This applies squarely here: 8³ bricks storing a *surface* waste most of their volume.
It is the argument for a DAG / bitmask leaf (SVDAG's 4³ = 64-bit leaf) over a brick pool when the
content is surface-like terrain rather than participating media.

**(c) Kämpe et al.'s objection.** `[CONFIRMED]` the ~1024 MB apron figure quoted in §1.3.

**(d) Feedback-loop latency.** `[INFERENCE]` The loop is frame N marks → compaction → readback →
CPU produce → upload → visible at frame N+k. The accepted answer to that latency, per §1.4, is
**render with what is resident (a coarser level) and never wait.**

---

## 2. Efficient Sparse Voxel Octrees (Laine & Karras, I3D 2010 + NVR-2010-001)

### 2.1 Headline numbers

`[CONFIRMED]` Figure 1 caption, verbatim:

> *"Sibenik cathedral ray-traced using voxels. Voxel data was created with high-resolution surface
> displacement, and ambient occlusion was calculated as a pre-process step. All geometry and shading
> data is stored on a per-voxel basis, i.e. there are no instantiated objects, textures, or
> materials. **The resolution is approximately 5mm throughout the entire building**, including outer
> walls that are not visible from the inside. **The total size of the data in GPU memory is 2.7 GB.**
> Our ray caster is able to cast **60.9 million primary rays per second** for this data, and **122.0
> million rays per second** for the non-displaced version of the same scene. For comparison, the
> fastest triangle-based GPU ray caster to date ([Aila and Laine 2009]) achieves **107.1 million
> rays per second** for the non-displaced variant on the same hardware."*

— https://users.aalto.fi/~laines9/publications/laine2010i3d_paper.pdf

`[UNVERIFIED]` Hardware: "the same hardware" as Aila & Laine 2009, which `[CONFIRMED]` used
*"an NVIDIA GTX285"*. ESVO's own measurement-setup sentence was not extracted. Treat as GTX 285,
single source.

`[CONFIRMED — via the ESVO source distribution's README]` Real octree file sizes:
`conference_15.oct 1.06 GB`, `conference_15_ao.oct 1.30 GB`, `default_13.oct 888 MB`,
`default_13_ao.oct 1.01 GB`, `hairball_11.oct 1.41 GB`, `hairball_11_ao.oct 1.60 GB`; one level
lower: `conference_14.oct 426 MB`, `default_12.oct 333 MB`, `hairball_10.oct 474 MB`.
— https://github.com/poelzi/efficient-sparse-voxel-octrees/blob/master/README

`[CONFIRMED — third-party]` SVDAG's independent characterization: *"The highest octree resolutions,
possible to fit into 4GB memory, ranged from 1K³ to 32K³ for the tested scenes. Approximatly 40% of
the memory consumption was due to geometry encoding and the rest due to material, i.e. color and
normal."*

### 2.2 Child descriptor layout — exact bit fields

`[CONFIRMED]` Verbatim from §3.1 and Figure 2:

> *"We encode the topology of the octree using 64-bit child descriptors, each corresponding to a
> single non-leaf voxel. Leaf voxels do not require a descriptor of their own, as they are described
> by their parents. As illustrated in Figure 2, the child descriptors are divided into two 32-bit
> parts. The first part describes the set of child voxels, while the second part is related to
> contours (Section 3.2). Each voxel is subdivided spatially into 8 child slots of equal size. The
> child descriptor contains two bitmasks, each storing one bit per child slot. valid mask tells
> whether each of the child slots actually contains a voxel, while leaf mask further specifies
> whether each of these voxels is a leaf."*

Figure 2 field widths, verbatim: **`child pointer 15 | far 1 | valid mask 8 | leaf mask 8`** and
**`contour pointer 24 | contour mask 8`**, captioned *"64-bit child descriptor stored for each
non-leaf voxel."*

`[CONFIRMED]` **Block organization — the most actionable single quote in this document:**

> *"On the highest level, our octree data is divided into blocks. Blocks are contiguous areas of
> memory that store the octree topology along with voxel geometry and shading attributes for
> localized portions of the octree. **All memory references within a block are relative, making it
> easy to reorganize blocks in memory. This facilitates dynamic memory management necessary for
> out-of-core rendering.** Each block consists of an array of child descriptors, an info section,
> contour data, and a variable number of attachments... The info section encompasses a directory of
> the available attachments as well as a pointer to the first child descriptor. We access child
> descriptors and contour data during ray casts. Once a ray hits surface geometry, we execute a
> shader that looks up the attachments contained by the particular block and decodes the shading
> attributes."*

`[CONFIRMED]` The `far` bit: when the 15-bit relative pointer cannot reach, the flag redirects
through a separate 32-bit far pointer stored in the block (Figure 3).

`[INFERENCE — highly actionable]` **Relative-within-block pointers are the single design decision
that makes ESVO streamable and this engine's structure not.** `world/svo`'s node array uses flat
global word offsets, so a partial upload is impossible: any relocation invalidates everything.
Blocks with relative references can be evicted, relocated and re-uploaded independently.

### 2.3 Contours

`[CONFIRMED]` Listed among the tech report's contributions: *"additional voxel contour information,
normal compression format for storing high-precision object-space normals, post-process filtering
technique for smoothing out blockiness of shading, and beam optimization for accelerating ray
casts."* — https://users.aalto.fi/~laines9/publications/laine2010tr1_paper.pdf

`[CONFIRMED — third-party critique]` *"The pruning of children allows them to save memory in scenes
with many flat surfaces, but in certain tricky scenes, where the scene does not resemble flat
surfaces, the result is instead problematic stitching of contours without memory savings."*
(Kämpe et al.) SSVDAG repeats it: *"Storing the proxy instead of subtrees achieves considerable
compression only in scenes with many planar faces, and introduces stitching problems as in other
discontinuous piecewise-planar approximations."* — https://jcgt.org/published/0006/02/01/paper.pdf

### 2.4 The beam optimization — what could and could not be confirmed

`[CONFIRMED]` It exists and is a named contribution: *"beam optimization for accelerating ray
casts."*

`[UNVERIFIED]` **The section describing it and its measured speedup were not retrieved.** Two
mutually inconsistent search answers appeared (one "4×4 or 8×8 pixel blocks, corner rays, no single
speedup figure"; another "2×2 grid, ~20–30%"), neither backed by primary text. **Do not treat any
specific resolution or speedup number for ESVO's beam optimization as established.** The reliable
statement is the concept only: a low-resolution pre-pass casts a sparse set of rays, and the
resulting conservative depth seeds a per-tile starting `t` so full-resolution rays skip the
traversal from the eye to near the surface.

**Follow-up**: fetch `nvr-2010-001.pdf` locally and read §6.

### 2.5 Why per-ray octree traversal is bandwidth/latency bound — the definitive measurement

`[CONFIRMED]` Aila & Laine, *Understanding the Efficiency of Ray Traversal on GPUs* (HPG 2009),
abstract:

> *"We study this question by comparing the measurements against a simulator that tells the upper
> bound of performance for a given kernel. **We observe that previously known methods are a factor
> of 1.5–2.5X off from theoretical optimum, and most of the gap is not explained by memory
> bandwidth, but rather by previously unidentified inefficiencies in hardware work distribution.**
> We then propose a simple solution that significantly narrows the gap between simulation and
> measurement. This results in the fastest GPU ray tracer to date."*

— https://users.aalto.fi/~ailat1/publications/aila2009hpg_paper.pdf

`[CONFIRMED]` On divergence: *"The execution of trace() consists of an unpredictable sequence of node
traversal and primitive intersection operations. This unpredictability can cause some penalties on
CPUs too, but **on wide SIMD/SIMT machines it is a major cause of inefficiency. For example, if a
warp executes node traversal, all threads (in that warp) that want to perform primitive
intersection are idle, and vice versa.**"*

`[CONFIRMED]` On packet/hierarchical tracing: *"The awkward fact is that when using hierarchical
tracing methods, only the most coherent part of the operations near the root gets accelerated, often
leaving seriously incoherent per-ray workloads to be dealt with. This appears to be one reason why
very few, if any, meaningful performance gains have been reported from hierarchical tracing on
GPUs."*

`[CONFIRMED]` Setup: *"All measurements were done using an NVIDIA GTX285. Register count of the
kernels ranged from 21 to 25 (this variation did not affect performance). We used a thread block
size of 192... Rays were assigned to warps following the Morton order (aka Z-curve). All data was
stored as array-of-structures, and nodes were fetched through a 1D texture."*

**Table 2, Conference scene (282 K tris, 164 K nodes), Mrays/s (simulated → measured, % of
simulated)** `[CONFIRMED]`:

| Kernel | Primary | AO | Diffuse |
|---|---|---|---|
| packet | 149.2 → **63.6** (43 %) | 100.7 → 39.4 (39 %) | 36.7 → 16.6 (45 %) |
| while-while | 166.7 → **88.0** (53 %) | 160.7 → 86.3 (54 %) | 81.4 → 44.5 (55 %) |
| if-if | 129.3 → 90.1 (70 %) | 131.6 → 88.8 (67 %) | 70.5 → 45.3 (64 %) |
| speculative if-if | 132.9 → 94.3 (71 %) | 139.2 → 92.7 (67 %) | 76.5 → 46.0 (60 %) |
| **persistent packet** | 149.2 → **122.1 (82 %)** | 100.7 → 86.1 (86 %) | 36.7 → 32.3 (88 %) |
| **persistent while-while** | 166.7 → **135.6 (81 %)** | — | — |

`[INFERENCE — the headline]` **Persistent threads took the packet kernel from 63.6 → 122.1 Mrays/s
(1.92×) and while-while from 88.0 → 135.6 Mrays/s (1.54×) on identical traversal code.** The entire
gain came from replacing the hardware work distributor with an application-managed persistent work
queue. **A fullscreen pixel shader offers no mechanism to do this; a compute dispatch with a global
atomic work counter does.**

---

## 3. Modern successors and shipped systems (2013–2026)

### 3.1 Sparse Voxel DAG (Kämpe, Sintorn, Assarsson — SIGGRAPH 2013)

`[CONFIRMED]` Figure 1 caption: *"The EPICCITADEL scene voxelized to a 128K³ (131 072³) resolution
and stored as a Sparse Voxel DAG. **Total voxel count is 19 billion, which requires 945MB of GPU
memory. A sparse voxel octree would require 5.1GB without counting pointers.** Primary shading is
from triangle rasterization, while ambient occlusion and shadows are raytraced in the sparse voxel
DAG at **170 MRays/sec and 240 MRays/sec** respectively, on an **NVIDIA GTX680**."*

`[CONFIRMED]` Abstract: *"We show that a binary voxel grid can be represented orders of magnitude
more efficiently than using a sparse voxel octree (SVO) by generalising the tree to a directed
acyclic graph (DAG)... **In all tested scenes, even the highly irregular ones, the number of nodes
is reduced by one to three orders of magnitude.**... **Meanwhile, our sparse voxel DAG requires no
decompression and can be traversed very efficiently.**"*
— https://www.cse.chalmers.se/~uffe/HighResolutionSparseVoxelDAGs.pdf

`[CONFIRMED]` Structure: identical-subtree deduplication with shared child pointers. Leaf encoding,
per HashDAG's description of it: *"following Kämpe et al. [KSA13], DAG leafs are 4³ = 64 subvolumes
encoded in bits of a 64-bit integer."*
`[CONFIRMED]` **Streaming/cache strategy: none.** *"We do not attempt to compress material or
reflectance properties of voxels in this paper, and thus, we focus mainly on using the structure for
visibility queries."* The design assumption is that the whole structure *fits*.

### 3.2 Symmetry-aware SVDAG (Jaspe Villanueva, Marton, Gobbetti — I3D 2016 / JCGT 2017)

`[CONFIRMED]` Figure 1 caption: *"The PowerPlant scene voxelized to a 256K³ resolution and stored as
a Symmetry-aware Sparse Voxel DAG (SSVDAG). **The total non-empty voxel count is nearly 100 billion,
stored in less than 575 MB at 0.048 bits/voxel. A sparse voxel octree would require 31.1 GB without
counting pointers, over 55 times more, while a Sparse Voxel DAG would require 1.0 GB, nearly
double.**"*

`[CONFIRMED]` Method: *"merging subtrees that are identical through a similarity transform and by
exploiting the skewed distribution of references to shared nodes to store child pointers using a
variabile bit-rate encoding... by selecting plane reflections along the main grid directions as
symmetry transforms."* Traversal: *"a GPU tracing method based on a multi-resolution Digital
Differential Analyzer (DDA), implemented with a full stack."*
`[CONFIRMED]` Cost: *"with a tracing overhead of less than 15%."* Result: *"real-time GPU in-core
visualization with shading and shadows of the full Boeing 777 at sub-millimeter precision."*
— https://jcgt.org/published/0006/02/01/paper.pdf

### 3.3 HashDAG — editable compressed voxel DAG (Careil, Billeter, Eisemann — EG 2020)

`[CONFIRMED]` Abstract: *"Sparse voxel DAGs (Directed Acyclic Graphs) overcome this hurdle and offer
high-resolution representations for real-time rendering **but only handle static data.** We introduce
a novel data structure to enable interactive modifications of such compressed voxel geometry
**without requiring de- and recompression.**"*

`[CONFIRMED]` Contributions: *"• A method for modifying sparse voxel DAGs in their compressed form,
i.e., without decompression/recompression; • A novel data structure to enable interactivity and
ensure little impact on compression/rendering/traversal performance; • A natural recording of
changes to enable undo/redo operations and associated garbage collection to free memory, when
needed."*

`[CONFIRMED]` Method: *"our method keeps the original structure intact but attaches additional
information to indicate modifications. These additions are light-weight as they take place directly
in the compressed domain, while maintaining an efficient traversal (and, thus, rendering)
performance."* Edit traversal: *"our method traverses the DAG structure in a depth-first manner. If a
node is not affected by the change, we stop the traversal, otherwise, we descend into the children.
During this traversal, it is possible to descend into nodes that did not exist in the original DAG
structure and will be added and compressed on the fly."*

`[CONFIRMED]` Test resolutions: Epic Citadel (128k)³, San Miguel (64k)³; *"copying an entire building
(order of 80M voxels)"*.
— https://graphics.tudelft.nl/Publications-new/2020/CBE20/ModifyingCompressedVoxels-main.pdf

`[UNVERIFIED / single-source]` From secondary documents only: edits run on the CPU at *interactive*
rather than real-time rates, e.g. ~52.6 ms to place spheres in Citadel; hash table default 2¹⁶
buckets for levels ≥ 10 and 1024 buckets nearer the root; page size 512 words; MurmurHash3 for
interior nodes and its bit-mixing step for leaves; memory overhead dominated by
allocated-but-partially-filled pages. Reference implementation: https://github.com/phyronnaz/hashdag

`[INFERENCE]` HashDAG's page + hash-table design is the closest published thing to what this engine
needs: a **content-addressed, page-granular, GPU-resident structure that can be mutated
incrementally instead of rebuilt.** Its append-only-with-garbage-collection discipline is the direct
alternative to "rebuild from scratch on 2 m of camera motion."

### 3.4 Aokana — GPU-driven SVDAG streaming for open-world games (PACMCGIT 2025)

`[CONFIRMED]` Abstract: *"we introduce Aokana, a GPU-Driven Voxel Rendering Framework for Open World
Games. **Aokana is based on a Sparse Voxel Directed Acyclic Graph (SVDAG). It incorporates a
Level-of-Details (LOD) mechanism and a streaming system, enabling seamless map loading as players
traverse the open-world game environment.** We also designed a corresponding high-performance
GPU-driven voxel rendering pipeline to support real-time rendering of the voxel scenes that contain
tens of billions of voxels... **Aokana can reduce memory usage by up to ninefold and achieves
rendering speeds up to 4.8 times faster than those of previous state-of-the-art approaches.**"*

`[CONFIRMED]` From §1: *"Many earlier methods either required storing the complete scene data in
VRAM, leading to high overhead, or **utilized deep data structures connected by pointers, which are
not cache-friendly. The use of these deep data structures results in decreased VRAM access
performance when the voxel scene resolution is high, ultimately causing a loss of rendering
performance.** To address these issues, we developed a streaming loading system that does not
require loading the entire voxel scene data into VRAM. **We utilize multiple shallow SVDAGs to
maintain the entire scene, and by employing Hi-Z occlusion culling and a visibility buffer, we
minimize memory access frequency and overdraw.** For voxel scenes with a resolution above 32K, our
method achieves a rendering speed **2 to 4 times faster than HashDAG**, and **only about 5% of the
complete voxel scene data needs to be loaded into VRAM when navigating the scene.**"*

`[CONFIRMED]` Aokana's critique of Nanite for voxels: *"if a scene contains many discontinuous
voxels, Nanite may generate an excessive number of disconnected clusters that are hard to be merged.
These disconnected clusters also have poor geometric quality because the construction of LOD relies
on the mesh simplification algorithm."*
— https://arxiv.org/html/2505.02017v1 (DOI https://doi.org/10.1145/3728299)

`[UNVERIFIED / single-source]` Results section, via synthesis: **RTX 3060 Ti**, average frame time
**6 ms** at 64K resolution (ten billion voxels); San Miguel 64K VRAM ≈ **424 MB**.

`[INFERENCE — the most transferable lesson]` *"multiple shallow SVDAGs"* rather than one deep tree.
This engine's 512 m root at 7.8 mm voxels is a **16-level** tree (V = 16); every primary ray pays 16
dependent, cache-missing `StructuredBuffer<uint>` loads before it reaches a brick. Splitting into a
grid of shallow trees (e.g. 32 m regions × 12 levels, or 8 m × 10 levels) collapses the
dependent-load chain **and** makes each region independently streamable and independently
rebuildable — which is also the answer to the rebuild storm.

### 3.5 NAADF — nested axis-aligned distance fields (Ulschmid, Ott, Macho, Wimmer, Ohrhallinger — CGF 45(2), Eurographics 2026)

`[CONFIRMED]` Abstract — the freshest directly relevant result found:

> *"We propose a novel multilayered spatial structure augmented with in-cell axis-aligned distance
> fields (AADF) operating as caches. **Our nested cell structure already accelerates ray tracing
> 3-5x compared to the state-of-the-art dense spatial structures, such as variants of directed
> acyclic graphs (DAG). Using the AADFs (constructed while rendering), we can double the ray
> throughput again (total 10x).** As an application, exploiting nested AADFs (NAADFs) also allows us
> to double the speed of global illumination computations while significantly reducing artifacts
> from camera motion, such as flickering, blurring, ghosting, and aliasing, all of which are
> especially important in voxel worlds with sharp edges. **We achieve this by adapting temporal
> antialiasing (TAA) to retain the last 32 frames rather than a single history buffer** to create
> the final antialiased image, since the discretized voxel structure requires much less memory to
> store the quantized positions and normals of ray bounces. The sample accumulation for global
> illumination is optimized by compressing and separating lit/unlit samples, and we apply **8x8
> window spatial resampling based on a reservoir-based spatiotemporal importance resampling
> (ReSTIR)** method. **Our proposed NAADFs support editing with quick updates to the acceleration in
> the background**, overlays of non-aligned dynamic geometry, and can be easily extended to support
> transform-aware compression or to represent huge real-world scans."*

— https://www.cg.tuwien.ac.at/research/publications/2026/ulschmid-2026-naadf/
(DOI https://doi.org/10.1111/cgf.70413; GitHub link on that page)

`[INFERENCE]` Two things map directly onto this engine's problems. (1) *"in-cell axis-aligned
distance fields... constructed while rendering, operating as caches"* — a **lazily built,
self-populating empty-space accelerator**: the marcher writes back what it learned so the next frame
is cheaper. That is the resident-cache idea applied to *traversal* rather than to geometry. (2) The
moiré here is exactly the artifact class they name (*"flickering, blurring, ghosting, and
aliasing... especially important in voxel worlds with sharp edges"*), and their answer is a
**32-frame history** rather than this engine's 8-frame (`taa_blend = 0.125`), affordable precisely
because voxel hit positions and normals quantize cheaply.

### 3.6 Encoding Occupancy in Memory Location (CGF 2025)

`[UNVERIFIED / single-source]` https://doi.org/10.1111/cgf.70292 — the child mask is folded into the
node's *address* rather than stored explicitly, giving **13–18 % smaller geometry**, **10–12 % faster
ray tracing for static models**, and **20–25 % faster for dynamic/editable scenes**, despite extra
address translation. Paper text not extracted. A promising 2025 pointer, not an established fact.

### 3.7 Nanite (Karis, Stubbe, Wihlidal — SIGGRAPH 2021)

`[CONFIRMED]` Karis's own assessment of why Nanite is *not* voxels:

> *"Voxels and implicit surfaces have a lot of potential advantages and are the most discussed
> direction to solve this problem. This is a 2M poly bust / Resampled to 13M narrow band SDF voxels /
> We've already accounted for sparsity in that no empty voxel is stored and yet with 6 times the
> amount of data / It looks blobby / The reason for this is voxelization is a form of uniform
> resampling and / Uniform resampling means loss... **In essence, the chief issue with voxels is a
> data size problem / Maximum sparsity is needed to keep data size small but we can't sacrifice ray
> casting performance in the process / And the data structure needs to be super adaptive to get
> sharp edges but not waste samples where its smo[oth]"*

`[CONFIRMED — quoted in the surviving synthesis]` Nanite's stated streaming principle: *"GPU scene
representation persists across frames — Sparsely updated where things change."*

> **GAP.** The remainder of §3 — including the Teardown / Dennis Gustafsson material and any other
> shipped systems and 2023–2026 papers — was lost in transit. The Teardown facts that survived
> elsewhere in the report are collected in §7 below and marked `[CONFIRMED as reported]`. **Re-run
> this topic first in any follow-up research pass**; the 2026 Teardown talks are named as the
> single highest-value unresolved source (§8.5).

---

## 4. Compute vs pixel shader, divergence, and coarse pre-passes

> **GAP.** §4.1–§4.4 were lost in transit. The surviving material is below. The one number from the
> lost section that the synthesis preserved is quoted in §7 item 4: NVIDIA's guidance *"Consider
> converting your full screen pass to a compute shader if there's a large difference in latency
> between warps"*, and **thread-group-ID swizzling measured at 47 % on a fullscreen pass with L2 hit
> rate 63 % → 86 %.** Re-run this topic to recover the primary URLs for those two claims before
> citing them as CONFIRMED.

### 4.5 Divergence and the silhouette

`[INFERENCE]` The effective cost of a warp is set by its slowest lane, so for a marcher the cost of
a 32-lane group is set by its silhouette pixels — the ones whose rays graze geometry and take the
most steps. Persistent threads, work compaction and SER all attack precisely this.

### 4.6 Shader Execution Reordering — the modern hardware answer to divergence

`[CONFIRMED]` Khronos, announcing `VK_EXT_ray_tracing_invocation_reorder` (18 Nov 2025): *"**a few
lines of code enabled SER to improve performance in this Vulkan® glTF™ path tracer by 47%**, with
similar improvements in widely used apps."* On the mechanism: *"In extreme cases, a 32-invocation
subgroup might invoke 32 different shaders one after the other before it can resume with the rest of
its code. This can also happen in path tracers when some invocations in a subgroup finish their ray
tracing work early... **All the invocations in a subgroup must wait for the slowest ones to terminate
before new work can be scheduled onto the subgroup.**"*
— https://www.khronos.org/blog/boosting-ray-tracing-performance-with-shader-execution-reordering-introducing-vk-ext-ray-tracing-invocation-reorder

`[CONFIRMED]` NVIDIA, *Path Tracing Optimization in Indiana Jones* (May 2025) — a shipped-game
measurement:

| | SER OFF | SER ON |
|---|---|---|
| GPU Time (`TraceMain`) | **4.08 ms** | **3.63 ms** |
| Active Threads / Warp | **38 %** | **70 %** |

And after additionally reducing live-state spills from **222 → 84 bytes per thread**: *"**enabling
Shader Execution Reordering on a GeForce RTX 5080 GPU reduced TraceMain GPU time by 24 percent, from
4.07 ms to 3.08 ms, while active threads per warp improved from 38 percent to 68 percent.**"*
Measured at 4K output with DLSS-RR Performance (path tracing at 1080p).

`[CONFIRMED]` The causes of low warp occupancy, verbatim: *"**Dynamic loops with divergent loop
counts within a warp; Dynamic branches with divergent executed paths within a warp; Different hit
shaders getting invoked within a warp, in RayGen shaders.**"*

`[CONFIRMED]` Availability: *"For Vulkan, SER is exposed by the VK_NV_ray_tracing_invocation_reorder
extension. For DX12, SER is now available in DXR 1.2 or through NvAPI."* Now also multi-vendor as
`VK_EXT_ray_tracing_invocation_reorder` and in Shader Model 6.9.
— https://developer.nvidia.com/blog/path-tracing-optimization-in-indiana-jones-shader-execution-reordering-and-live-state-reductions/

`[INFERENCE — important limitation]` SER requires the **ray tracing pipeline** (`hitObject`,
`reorderThreadEXT`). It is **not** available to a fullscreen pixel shader, and not to a plain compute
shader either. Using it would mean expressing the voxel march as DXR/VK-RT with custom intersection,
or emulating the idea manually in compute (sorting/compacting rays into coherent batches between
traversal phases). The 38 % → 70 % active-threads figure is nevertheless the best available
quantification of **how much headroom divergence is costing a marching workload of this shape.**

### 4.7 Is a low-resolution "start-t" pre-pass standard practice?

`[INFERENCE — supported by four independent shipped/published systems]` Yes, under different names,
and it always measures the same thing: **a conservative lower bound on the distance at which the
expensive per-pixel loop can begin, or a proof that it can be skipped entirely.**

- `[CONFIRMED]` ESVO calls it **beam optimization** and lists it as a contribution
  (https://users.aalto.fi/~laines9/publications/laine2010tr1_paper.pdf). Details `[UNVERIFIED]` (§2.4).
- `[CONFIRMED as reported]` Teardown copies a **linear depth buffer** several times per G-buffer pass
  and does a manual depth test in the fragment shader *"allowing the fragment to be discarded (and
  thus skip the expensive ray tracing part) if the rasterized box is behind other geometry"*, with
  objects drawn front-to-back. — https://juandiegomontoya.github.io/teardown_breakdown.html
- `[CONFIRMED]` Aokana uses **Hi-Z occlusion culling and a visibility buffer** to *"minimize memory
  access frequency and overdraw"*. — https://arxiv.org/html/2505.02017v1
- `[CONFIRMED]` GigaVoxels renders **proxy geometry** to generate rays, and *"Early-Z and Z-Cull
  prevents pixels with terminated rays from being overdrawn"*. — Cornell slides, URL above.

---

## 5. GPU performance measurement that does not perturb the frame

### (a) Tracy Profiler

**CPU zone overhead — the author's own numbers.** `[CONFIRMED]` In issue #192 a user measured
29 ns/span in a tight synthetic loop; Tracy's author (wolfpld) responded:

> *"The measurements, as described in the manual, do not represent this value, but rather a real
> wall-clock impact on program execution time... Due to the way out-of-order CPUs execute code, the
> 'queue delay' self-measurement may not reflect the true performance impact, which in this case was
> actually smaller."*
> *"Here are some measurements of the 'queue delay' performed on various CPUs and compilers:*
> *i7-8700K, MSVC: **9 ns**; i5-8259U, gcc: **20 ns**; Ryzen 3900X, MSVC: **10 ns**; Ryzen 3900X,
> gcc: **20 ns**; Ryzen 5950X, MSVC: **9 ns***
> *As you can see, the CPU model has negligible impact... **The actual cause here is the compiler
> (and OS) used to prepare the program.**"*

Reporter's own measurement: *"Logged 10000000 spans in 293.115409ms (29ns per span)"* on
*"an AMD Ryzen 9 3950X with gcc 9.3.0"*, and *"from watching it in perf top, **it looks like the
rdtsc instruction is taking up ~70% of the program's total time.**"* Author on AMD TSC granularity:
*"So yes, **the resolution is 10 nsecs** as you have measured."*
— https://github.com/wolfpld/tracy/issues/192

`[UNVERIFIED / single-source]` The manual's §1.7 headline figure is reported as **≈2.25 ns per zone**
(covering begin and end). The manual text exceeds the extraction cap. `[INFERENCE]` The
reconciliation is what the author says: 2.25 ns is marginal wall-clock cost in a real program (the
OOO core hides most of it); 9–29 ns is queue-delay self-measurement in an empty loop, dominated by
`rdtsc`/`rdtscp` latency. **Budget ~10 ns per zone as a worst case: at 150 fps and 1000 zones/frame
that is 1.5 % of a 6.67 ms frame.**

`[CONFIRMED — source-attributed, auto-generated from the repo]` Timer: *"On x86/x64: Direct `rdtscp`
instruction for hardware timer reading... The profiler calibrates the timer at startup."* Memory:
*"Tracy uses a separate memory allocator (rpmalloc) to isolate itself from the application's heap."*
Queueing: *"For most profiling events, a lock-free Multiple Producers Single Consumer queue is used
— Application threads add events without waiting — A dedicated worker thread dequeues events —
**No locks or synchronization required on the critical path.**"* Compression: *"Outgoing data is
buffered in 256KB chunks — Each chunk is compressed [LZ4] before transmission — **Compression happens
in the worker thread, not impacting the application threads.**"*
— https://deepwiki.com/wolfpld/tracy/5.4-performance-considerations (cites `manual/techdoc.tex` 420–434)

`[CONFIRMED]` Cost drivers to avoid: *"Avoid frequent text message events → Text processing and
copying has higher cost; **Be selective with callstack captures → Capturing callstacks has
significant overhead**; Limit lock tracking to important locks → Each lock event creates queue
entries; Use frame marks appropriately."*

`[CONFIRMED]` `TRACY_ON_DEMAND`: *"Only collects data when connected to server."*
`[UNVERIFIED / single-source]` Its cost: it *"introduces additional time costs per profiling event
due to the extra bookkeeping required to maintain a coherent application state when connections are
established or dropped."*

**GPU zones.** `[CONFIRMED — source-attributed]` *"Vulkan: Query pools and timestamps; Direct3D 12:
Query heaps and timestamps; Direct3D 11: Query objects."* Time base: *"Because CPU and GPU operate on
different timelines, Tracy synchronizes these timelines by: 1. Capturing corresponding timestamps on
both CPU and GPU 2. Establishing a correlation between these timestamps 3. **Periodically
recalibrating to account for drift.**"* Vulkan specifics: *"Multiple timing domains...; Calibration
between CPU and GPU timestamps; Handling of query pool management."* Protocol events:
`GpuNewContext`, `GpuContextName`, `GpuZoneBegin`, `GpuZoneEnd`, `GpuTime`, `GpuCalibration`. Usage:
*"Periodically call the Collect function to gather profiling data."*
— https://deepwiki.com/wolfpld/tracy/2.3-gpu-profiling (cites `public/tracy/TracyVulkan.hpp`
97–188, 483–623; `TracyD3D12.hpp` 57–187)

`[CONFIRMED]` `TRACY_NO_CALLSTACK` disables callstack collection; `TRACY_CALLSTACK` sets depth
(*"higher = more overhead"*).

`[INFERENCE]` Safe to leave enabled with discipline: scoped zones only, no per-zone text messages, no
callstacks, a handful of GPU zones per frame, plus `TRACY_ON_DEMAND`. GPU zones are the part to watch
— each is a `vkCmdWriteTimestamp`, which has a documented barrier-like side effect (next section).

### (b) Vulkan timestamp queries and `VK_KHR_performance_query`

**Timestamps — the documented caveats, verbatim** `[CONFIRMED]`:

> *"It's important to know that **timestamp queries differ greatly from how timing can be done on the
> CPU**... So while technically you can specify any pipeline stage at which the timestamp should be
> written, **a lot of stage combinations and orderings won't give meaningful result. This also means
> that you can't compare timestamps taken on different queues.**"*
> *"And so for this example, we take the same approach as some popular CPU/GPU profilers by **only
> using the top and bottom stages of the pipeline. This combination is known to give proper
> approximate timing results on most GPUs.**"*
> *"It's also important to note that **not all implementations are able to latch timers at all
> pipeline stages (e.g. if they don't have hardware that maps to a given stage) and may return timers
> at a later pipeline stage instead.**"*
> *"**Calling this function also defines an execution dependency similar to a barrier on all commands
> that were submitted before it.**"*

Support checks: `VkPhysicalDeviceLimits::timestampPeriod > 0`; then `timestampComputeAndGraphics` —
*"If this is VK_TRUE, all graphics and compute pipelines support timestamp queries... If not, we need
to check if the queue we want to use supports timestamps"* via
`queueFamilyProperties.timestampValidBits`. Pool must be reset with `vkCmdResetQueryPool` first.
Readback via `vkCmdCopyQueryPoolResults` into a `VkBuffer`, or `vkGetQueryPoolResults` after
completion.
— https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html

`[CONFIRMED]` Khronos' profiling guide adds the practical rule: *"**Retrieve results (usually in the
next frame to avoid blocking)**"*, with
`elapsed_ms = (results[1] - results[0]) * limits.timestampPeriod / 1e6f`.
— https://github.khronos.org/Vulkan-Site/guide/latest/profiling.html

**On this engine's documented "first timestamp faults on Vulkan" workaround:** `[UNVERIFIED — and
this is explicit]` **No primary Vulkan documentation was found stating any special caveat about the
first timestamp in a command buffer.** The documented facts that could produce the observed symptom
are: (1) the barrier-like execution dependency `vkCmdWriteTimestamp` imposes; (2) implementations
latching at a later stage than requested; (3) the pool must be reset first. `CLAUDE.md` records a
real crash inside the NVIDIA driver in exactly this situation; that appears to be **undocumented
driver behaviour**, and skipping the first two frames is a reasonable workaround with no published
justification to point at. Possibly-related open reports:
https://github.com/khronosGroup/Vulkan-samples/issues/1370 and
https://github.com/gpuweb/gpuweb/issues/3952.

**`VK_KHR_performance_query` on NVIDIA:** `[CONFIRMED]` The extension exists (announced Nov 2019,
https://www.khronos.org/blog/vulkan-releases-extension-to-expose-cross-vendor-performance-metrics)
and is specified at
https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_performance_query.html with counter
enumeration via `vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR`.

`[UNVERIFIED — an unresolved contradiction]` The Vulkan Hardware Database
(https://vulkan.gpuinfo.org/listdevicescoverage.php?extension=VK_KHR_performance_query) returned an
alphabetical page truncated before the NVIDIA entries; visible entries are **AMD (RADV), Intel and
Mesa drivers only**, and every corroborating implementation hit is a Mesa commit
(https://www.phoronix.com/news/RADV-VK_KHR_performance_query,
https://gitlab.freedesktop.org/mesa/mesa/-/commit/2001a80d4a81f2e8194b29cca301dd1b27be9acb). A
search synthesis asserted NVIDIA support with no primary evidence. **Check `vulkaninfo` on the
machine directly — it is a two-second check and neither answer is trustworthy.**

`[CONFIRMED]` What *is* certain: NVIDIA hardware performance counters are permission-restricted
regardless of API — https://developer.nvidia.com/nvidia-development-tools-solutions-err_nvgpuctrperm-permission-issue-performance-counters.
`[UNVERIFIED]` Remedies: enable GPU performance counter access via the NVIDIA App / Control Panel on
Windows, or `nvidia-capabilities` (`profiler-device`/`profiler-context`) on Linux; admin/root may be
required depending on driver version.

`[INFERENCE]` Practical conclusion: **timestamp queries are the only counter mechanism reliably
available unprivileged on this machine, and they are safe to leave permanently enabled** provided
you (a) use TOP_OF_PIPE/BOTTOM_OF_PIPE only, (b) read back a frame or two later without `WAIT_BIT`,
and (c) keep the count low, because each write is a partial barrier.

### (c) D3D12 query heaps and pipeline statistics

`[CONFIRMED]` Timestamps: `D3D12_QUERY_TYPE_TIMESTAMP` in a `D3D12_QUERY_HEAP_TYPE_TIMESTAMP` heap;
frequency from `ID3D12CommandQueue::GetTimestampFrequency` (Hz); resolve with
`ID3D12GraphicsCommandList::ResolveQueryData` into a buffer; result is a UINT64 tick count divided by
the frequency.
— https://learn.microsoft.com/en-us/windows/win32/direct3d12/queries,
https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing,
https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resolvequerydata

`[CONFIRMED]` `D3D12_QUERY_TYPE_PIPELINE_STATISTICS` writes a
`D3D12_QUERY_DATA_PIPELINE_STATISTICS` with exactly these counters: **`IAVertices`, `IAPrimitives`,
`VSInvocations`, `GSInvocations`, `GSPrimitives`, `CInvocations`, `CPrimitives`, `PSInvocations`,
`HSInvocations`, `DSInvocations`, `CSInvocations`.**
— https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_query_data_pipeline_statistics

`[INFERENCE]` **`PSInvocations` is directly useful here and cheap:** for a fullscreen marcher it says
exactly how many pixels actually ran the traversal, which is the ground truth for whether a coarse
depth pre-pass or a stencil mask is doing anything. There is **no** counter here for warp occupancy,
cache hit rate, or memory throughput — those need vendor tooling. Pipeline-statistics queries use
`BeginQuery`/`EndQuery` (unlike timestamps, which are `EndQuery`-only), so they bracket a region and
are somewhat more intrusive than a timestamp pair; still safe for a handful of regions.

### (d) NVIDIA Nsight Graphics / Nsight Perf SDK

`[CONFIRMED]` NVIDIA's own product page:

> *"The NVIDIA® Nsight™ Perf SDK is a graphics profiling toolbox for DirectX, Vulkan, and OpenGL
> **enabling you to collect GPU performance metrics directly from your application.** ... **Just a
> few lines of code are needed to set up GPU performance metrics collection.**"*
> *"**Realtime Perf Triage** — ... **The new GPU Periodic Sampler collects device-level metrics at
> high sampling rates with low overhead.**"*
> *"**Profile In-Application** — Integrate GPU performance metric collection into your application...
> **Activate profiling from your own custom programmatic triggers.**"*
> *"**Upgrade Your CI/CD** — Generate detailed profiler reports on every developer and artist change.
> **Add dedicated perf regression criteria by inspecting GPU metric values.**"*
> *"**Realtime Performance HUD** — ... **Explore panels with metrics on SM, L2 cache, ROP, VRAM and
> various other subunits**..."*
> *"**Timeline Viewer** — Examine a snapshot of your application's performance with the Nsight Perf
> SDK **one-shot sampling mode. This allows you to examine hardware activity with minimal
> overhead.** ... You can visualize **unit throughputs, warp occupancy, draw calls**, and more..."*
> *"**HTML Profiler Report Generator** — Simply insert a few calls at Graphics API Device
> Initialization, Present/SwapBuffers, a Keypress handler, or an automated trigger... **The report
> generator automatically collects 100s of GPU metrics of interest**... Quickly determine the
> workload type, pipeline activity and utilization, **shader latency reasons**, and 3D data flow."*

— https://developer.nvidia.com/nsight-perf-sdk

`[CONFIRMED]` This answers the question directly: **SM occupancy, warp stall (shader latency)
reasons, L2 and VRAM throughput can be collected programmatically from inside the application, with
programmatic triggers, and NVIDIA explicitly markets it for CI/CD perf-regression gating.**

`[CONFIRMED]` Nsight Graphics GPU Trace is the interactive counterpart; the Indiana Jones case study
shows what it yields: *"Predicated-On Active Threads per Warp: 38%"*, a **Ray Tracing Live State tab**
identifying which GLSL variables cause live-state spills, and VidL2/L1TEX throughput columns.

`[CONFIRMED]` **No published overhead percentage exists** for the Periodic Sampler; NVIDIA's own
words are the only characterization: *"low overhead"* / *"minimal overhead"*.

`[UNVERIFIED]` Shipping-build suitability: it requires GPU performance counter access (the
`ERR_NVGPUCTRPERM` gate), a per-machine driver setting on Windows and possibly `CAP_PERFMON`/root on
Linux — so it is a developer-machine and CI-machine tool, **not** a retail-build feature.
`[INFERENCE]` For this project that is fine: the laptop and the CI runner are the only machines that
matter.

### (e) PIX on Windows — programmatic capture

`[CONFIRMED]` Include `pix3.h` from the **WinPixEventRuntime** and load the capturer *before* D3D12
device creation: `PIXLoadLatestWinPixGpuCapturerLibrary()` loads `WinPixGpuCapturer.dll` for GPU
captures; `PIXLoadLatestWinPixTimingCapturerLibrary()` loads `WinPixTimingCapturer.dll` for timing
captures and **requires administrator privileges**. Control APIs: `PIXBeginCapture` /
`PIXEndCapture`, and **`PIXGpuCaptureNextFrames`** to enqueue a capture triggered by the next
`Present`. Overhead design: *"PIX uses minimal overhead by formatting event strings during capture
file analysis rather than at runtime."*
— https://devblogs.microsoft.com/pix/programmatic-capture/,
https://devblogs.microsoft.com/pix/pix-2108-18/,
https://devblogs.microsoft.com/pix/programmatic-timing-captures-now-available/,
https://devblogs.microsoft.com/pix/winpixeventruntime/,
https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/tools/pix3/functions/pixbegincapture

`[INFERENCE]` PIX markers (`PIXBeginEvent`/`PIXEndEvent`) are safe to leave in permanently — the
string formatting is deferred to analysis time. The *capturer DLLs* are not: loading them changes
device creation and belongs behind a flag. The Vulkan equivalent is `VK_EXT_debug_utils` labels —
`[CONFIRMED]` Khronos: *"Labels allow you to group draw calls and dispatch commands into logical
regions... These labels appear in tools like RenderDoc, NVIDIA Nsight, and AMD RGP"*
(https://github.khronos.org/Vulkan-Site/guide/latest/profiling.html).

### Summary table

| Mechanism | Documented overhead | Safe to leave enabled? |
|---|---|---|
| Tracy CPU zones | `[CONFIRMED]` 9–20 ns queue delay by compiler/OS (author); `[UNVERIFIED]` ~2.25 ns marginal per manual §1.7 | `[INFERENCE]` Yes — scoped zones only, no text/callstacks, plus `TRACY_ON_DEMAND` |
| Tracy GPU zones | `[CONFIRMED]` VK query pool / D3D12 query heap timestamps + periodic recalibration | `[INFERENCE]` Yes, few of them: each is a partial barrier |
| VK timestamp queries | `[CONFIRMED]` *"defines an execution dependency similar to a barrier on all commands that were submitted before it"* | `[INFERENCE]` Yes — TOP/BOTTOM only, read back next frame |
| `VK_KHR_performance_query` | `[CONFIRMED]` vendor-enumerated counters; needs counter permission on NVIDIA | `[UNVERIFIED]` NVIDIA availability unresolved — check `vulkaninfo` |
| D3D12 timestamps | `[CONFIRMED]` `GetTimestampFrequency` + `ResolveQueryData` | `[INFERENCE]` Yes |
| D3D12 pipeline statistics | `[CONFIRMED]` 11 named counters, no cache/occupancy data; Begin/End bracketed | `[INFERENCE]` Yes for a few regions |
| Nsight Perf SDK | `[CONFIRMED]` NVIDIA: *"low overhead"* / *"minimal overhead"*, no number published | `[UNVERIFIED]` Dev/CI only — needs counter permission |
| Nsight Graphics GPU Trace | `[INFERENCE]` interactive capture, perturbs by design | No — offline tool |
| PIX markers | `[CONFIRMED]` strings formatted at analysis time | `[INFERENCE]` Yes |
| PIX capturer DLL | `[CONFIRMED]` must load before device creation; timing capture needs admin | No — flag-gated |
| RenderDoc in-app API | `[CONFIRMED]` designed for passive detection, zero cost when absent | `[CONFIRMED]` Yes — see §6.7 |

---

## 6. Automated, headless, scriptable visual and performance regression testing

### 6.1 Unreal Engine — the most instructive write-up, because it documents the failures

`[CONFIRMED]` Nick Darnell (Epic), *Rendered Image Comparison* (Mar 2017), who built UE4's
screenshot comparison. The sources of nondeterminism, verbatim:

> *"rendered image comparison can be tricky because of differences caused by the following, — GPU —
> Driver — Hardware Abstraction Layer — Feature Level — Hardware Specific Features — Floating Point
> Precision — Resolution — Anti-Aliasing — …and well any source of non-determinism. All these things
> make it difficult to compare rendered output. **You could simplify matters by only testing on one
> kind of machine but that's a pretty unrealistic testing environment.**"*

The metric evolution, with his actual thresholds, verbatim:

> *"I started by converting the comparison method from Resemble.js to C++... It supports per-channel
> and brightness tolerances. **It also did neighbor similarity to attempt to account for
> anti-aliasing.**"*
> *"**Global Error** — The first mistake I made was comparing the pixels across the whole image and
> generate a percent difference... **The black pixels only represent a 1.85% difference in the image.
> The minimum required global error I defaulted to was 2%** before I considered it a problem.
> Lowering the required error to 1% would have worked, but **I wanted to maintain a large enough
> margin to avoid false positives coming from usual non-deterministic differences.**"*
> *"**Block Error** — To solve this problem I ended breaking up the images into **100 blocks (a
> spatial hash)**. I then accumulated error per block as well as global, which ends up producing
> blocks with **30%-40% error** in the sample above, which was plenty to overcome my new **maximum
> allowed block error of 10%**."*
> *"**Cluster Error** — ... having a small radius, say **3px radius**, and then for every error pixel
> that can touch another error pixel within the radius, they merge into a cluster... if you find a
> error cluster smaller than the global limit, but not insignificant (**maybe 0.05% total pixels**)."*

`[CONFIRMED]` **The cross-backend answer — UE does not compare across backends, it buckets goldens by
backend:**

> *"So I store the images like this,
> `CornellBox_Lit\Windows_D3D11_SM5\2806e638aac6982b11cbba723f004bb2.png` [+ `.json`] — Under the
> test folder, they're put into a folder made up of **PLATFORM_RHI_SHADERMODEL. This broadly separates
> the images based on at least the most significant contributors to differences. The files themselves
> are based on a unique identifier for the hardware**, so there is an assumption right now we need to
> have stable results for a given piece of hardware."*
> *"...one of the features I ended up adding... is the concept of **alternatives. In the event two
> shots are both right, the system permits additional shots to be added as ground truth**, and when
> comparison time comes, the system will choose the shot that is closest in terms of metadata matching
> to compare against."*
> *"...having a json file per shot containing the shot metadata... **it has and will have more
> information about features and rendering options currently enabled, in addition to things like
> driver version**, which in the diffing tool we can highlight changes to machines as possibly being
> the cause of differences."*

`[CONFIRMED]` **On perceptual metrics — a direct answer to "should I use SSIM?":**

> *"I looked at some other comparison approaches starting with perceptual comparison algorithms like
> **Structural Similarity and Perceptual Hashing, even added a prototype SSIM approach to UE4. The
> problem with these approaches is that they may hide the existence of real errors just because a
> human couldn't see them in the examples.**"*

`[CONFIRMED]` UE can compare **individual G-buffers**, not just final colour: *"if you actually
perform tests on the individual buffers before they are factored into the final pixel color, you may
detect errors sooner because... the difference may be obvious if you looked at say the Ambient
Occlusion buffer in isolation, [but] it may not show up clearly when comparing final pixel color."*
— https://www.nickdarnell.com/rendered-image-comparison/

`[UNVERIFIED]` Epic's current Gauntlet docs
(https://dev.epicgames.com/documentation/unreal-engine/gauntlet-automation-framework-in-unreal-engine)
returned HTTP 403.

`[INFERENCE — the most directly applicable finding for this engine]` The existing `--verify-frame`
local-contrast metric (34.7 % terrain, 0.9 % sky-only, 6 % threshold) is a *sanity* check, not a
regression check. Darnell's architecture is the upgrade path and it is cheap: **per-pose golden PNGs
bucketed by `platform_backend`, a JSON sidecar with driver version and flag state, a global-error
threshold plus a per-block threshold (his numbers: 2 % global, 10 % max block over 100 blocks), and
support for multiple accepted goldens per pose.** Note also the G-buffer point: this engine already
has 12 `--debug-view` terms, which are exactly the isolated buffers Darnell says catch errors sooner.

### 6.2 Unity Graphics Test Framework — the thresholds it exposes

`[CONFIRMED]` `ImageComparisonSettings`, verbatim:

- **`AverageCorrectnessThreshold`** — *"The maximum permitted average error value across the entire
  image. If the average per-pixel difference across the image is above this value, the images are
  considered not to be equal."*
- **`RMSEThreshold`** — *"The maximum permitted root mean squared error value across the entire
  image."*
- **`IncorrectPixelsThreshold`** — *"The maximum ratio of pixels allowed to be incorrect across the
  image. A pixel is incorrect if it exceeds the specified per-pixel thresholds."*
- **`PerPixelCorrectnessThreshold`** — *"The permitted perceptual difference between individual
  pixels of the images. **The deltaE for each pixel of the image is compared** and any differences
  below this threshold are ignored."*
- **`PerPixelGammaThreshold`** — *"The permitted difference between the RGB components (in gamma) of
  individual pixels."*
- **`PerPixelAlphaThreshold`**, **`TargetWidth`/`TargetHeight`** (*"If a reference image already
  exists for this test and has a different size the test will fail"*), **`TargetMSAASamples`**,
  **`UseBackBuffer`**, **`UseHDR`**, **`ActiveImageTests`**, **`ActivePixelTests`**.

— https://docs.unity3d.com/Packages/com.unity.testframework.graphics@9.0/api/UnityEngine.TestTools.Graphics.ImageComparisonSettings.html
(entry point `ImageAssert`; source at
https://github.com/Unity-Technologies/com.unity.testframework.graphics/blob/master/Runtime/ImageAssert.cs)

`[INFERENCE]` Unity's design is UE's conclusion expressed as composable thresholds: **a perceptual
per-pixel gate (CIE ΔE) to decide "is this pixel wrong", then a count-of-wrong-pixels ratio gate to
decide "is this image wrong".** That two-level structure is the most robust simple scheme found, and
is the one to copy.

### 6.3 Godot

`[CONFIRMED]` `Calinou/godot-rendering-tests` (a Godot maintainer's project, MIT, last pushed
2026-01-21): *"This is a Godot project that stores and runs a collection of 2D and 3D rendering
tests. **It is used to check for regressions in Godot's rendering backends, and can also be used to
test performance on various GPUs.**"* Comparison recipe: *"1. Run the project once to have images be
saved to `results/`. 2. Copy the `results/` folder to the same folder, but with a different suffix
(e.g. `results_master/`). 3. Install **dssim**, which is a tool compares images and returns a
similarity score (lower is more similar). 4. Run:
`(for img in results/*/*.png; do dssim $img ${img//results/results_master}; done) | sort` — Images
that differ the most from the images in `results_master/` are displayed at the bottom of the list."*
— https://github.com/Calinou/godot-rendering-tests (dssim: https://github.com/kornelski/dssim)

`[INFERENCE]` Note what this is and isn't: **no thresholds, no pass/fail — a ranked-diff triage list
a human reads.** For a one-person engine that may be the right cost/benefit point, and it is
trivially scriptable. Also found but not inspected: `godotengine/regression-test-project`.

### 6.4 Chromium / Skia Gold — the industrial answer to nondeterminism

`[CONFIRMED]` Chromium's GPU pixel testing docs, on why they chose multiple goldens over fuzzy
diffing:

> *"**Gold supports multiple approved images per test.** It is not uncommon for tests to produce
> images that are visually indistinguishable, but differ in a handful of pixels by a small RGB value.
> **Fuzzy image diffing can solve this problem, but introduces its own set of issues such as possibly
> causing a test to erroneously pass. Since most tests that exhibit this behavior only actually
> produce 2 or 3 possible valid images, being able to say that any of those images are acceptable is
> simpler and less error-prone.**"*

Mechanism: *"1. The test produces an image and passes it to `goldctl`, along with some information
about the hardware and software configuration that the image was produced on, the test name, etc.
2. `goldctl` checks whether the hash of the produced image is in the list of approved hashes. [If it
is] `goldctl` exits with a non-failing return code... [If not] `goldctl` uploads the image and
metadata to the storage bucket and exits with a failing return code... A user approves the new image
in the GUI, and the server adds the image's hash to the baselines."* Plus: *"**Triage time can be
much lower**... Once an image is triaged in Gold, it becomes immediately available for future test
runs."*
— https://chromium.googlesource.com/chromium/src.git/+/master/docs/gpu/gpu_pixel_testing_with_gold.md

`[INFERENCE]` Two independent large projects (Epic and Chromium) converged on **"multiple approved
exact images, keyed by hardware/config metadata"** rather than **"one image plus a fuzzy metric."**
That is a strong signal, and Chromium rejects fuzzy diffing explicitly on false-negative grounds.

### 6.5 Metrics that tolerate GPU nondeterminism

`[CONFIRMED]` **FLIP** (Andersson, Nilsson, Akenine-Möller, Oskarsson, Åström, Fairchild — HPG 2020),
abstract: *"We present ꟻLIP, which is a difference evaluator **with a particular focus on the
differences between rendered images and corresponding ground truths.** Our algorithm produces a map
that approximates the difference perceived by humans when alternating between two images... We also
present results of a user study which indicate that our method performs substantially better, on
average, than the other algorithms."*

`[CONFIRMED]` The authors' own erratum on the scalar to threshold on — important: *"**Hindsight: in
the paper we advocated for the weighted median, computed from the weighted histogram, but this is not
ideal. If a single number is to be used, then we recommend using the mean ꟻLIP value instead.**"*
And the scope limit: *"**This version of ꟻLIP handles only low dynamic range images.**"* Award:
*"Wolfgang Straßer Award for 3rd best paper, High Performance Graphics 2020."*
— https://research.nvidia.com/publication/flip

`[CONFIRMED]` Tooling: BSD-3-Clause; single-header C++/CUDA (`src/cpp/FLIP.h` since v1.3);
`pip install flip-evaluator` then `flip -r referenceImage.png -t testImage.png`; PyTorch loss
module; version 1.7. — https://github.com/NVlabs/flip

`[INFERENCE]` FLIP is the best-motivated perceptual metric for exactly this use case (rendered vs
reference), has a maintained single-header C++ implementation that can sit next to the existing PNG
dump path, and its authors say which scalar to threshold. But note Darnell's warning `[CONFIRMED]`:
a perceptual metric can *hide* real errors. **The right combination is FLIP (or ΔE) as the per-pixel
"is this wrong" gate plus a block/cluster-localized count as the "is this image wrong" gate** —
Unity's two-level structure with a better per-pixel term.

### 6.6 Cross-backend comparison specifically (Vulkan vs D3D12)

`[CONFIRMED]` UE's answer, in effect: **don't.** Goldens live in `PLATFORM_RHI_SHADERMODEL`
directories keyed further by a hardware hash.

`[CONFIRMED]` Khronos' KTX-Software CTS uses a two-tier policy: exact output matching on a designated
**"primary platform"**, and a **per-test `outputTolerance`** on non-primary platforms where golden
regeneration is disabled. — https://github.com/KhronosGroup/KTX-Software-CTS/blob/main/README.md

`[CONFIRMED — note the source's nature: a tutorial chapter]` The Vulkan Documentation Project's
*CI/CD Render Validation* chapter states the problem crisply: *"Traditional unit tests can't see your
screen. **Pixel-by-pixel comparisons are notoriously fragile—a single-pixel shift or a tiny color
variation in a different driver can trigger a false positive.**"* ... *"**Resolution Drift**: You
change the UI scale, and suddenly every pixel is slightly different. **Driver Jitter**: You update
your GPU drivers, and the anti-aliasing implementation changes its sub-pixel sampling pattern.
**Headless Mismatch**: You run on a software renderer in CI (like SwiftShader), which might use
different floating-point rounding..."* ... *"Suddenly, you have thousands of 'failing' tests that
look identical to the human eye. **This is called the Oracle Problem—it's easy to see that an image
is wrong, but it's very hard to write a mathematical rule that defines 'correctness' for every
pixel.**"* ... *"**Level 1: The 'Don't Panic' Sanity Check.** We'll use high-tolerance pixel
comparison and LPIPS to catch catastrophic failures (black screens, white screens, or static) in
milliseconds."* Headless recipe: point `VK_ICD_FILENAMES` at SwiftShader (*"it's perfect for CI
because it's deterministic—it should produce the exact same results on every machine"*) or lavapipe,
skip `VkSurfaceKHR` entirely, render offscreen to `VK_FORMAT_R8G8B8A8_UNORM` with `eLinear` tiling
and `eColorAttachment | eTransferSrc`, host-visible memory, `mapMemory`/`memcpy` readback.
— https://docs.vulkan.org/tutorial/latest/ML_Inference/Desktop_Applications/04_ci_render_validation.html

`[INFERENCE — the recommendation for this engine's cross-backend question]` Do not attempt a tight
cross-backend image comparison. Instead:
1. **Per-backend goldens** (`vk/` and `d3d12/` directories), each compared exactly-or-tightly against
   its own history — this catches *regressions*, which is what is actually wanted.
2. **One cross-backend consistency test with a deliberately loose perceptual threshold** (FLIP mean,
   or ΔE-per-pixel + incorrect-pixel ratio) to catch a *backend divergence* like the D3D12 X3500
   vector-write class of bug this engine already hit. Loose enough that TAA jitter and FP rounding
   pass; a failure means "investigate", not "block".
3. Because TAA is the dominant nondeterminism source, **run visual regression poses with `--no-taa`
   and a fixed frame count**, so the only remaining nondeterminism is FP rounding and driver AA. The
   flag already exists.

### 6.7 RenderDoc for programmatic capture

`[CONFIRMED]` RenderDoc's in-application API (v1.6.0 reference):

> *"**The recommended way to access the RenderDoc API is to passively check if the module is loaded,
> and use the API if it is. This lets you continue to use RenderDoc entirely as normal, launching your
> program through the UI, but you can access additional functionality to e.g. trigger captures at
> custom times. When your program is launched independently it will see that the RenderDoc module is
> not present and safely fall back.**"*
> *"To do this you'll use your platforms dynamic library functions to see if the library is open
> already - e.g. `GetModuleHandle` on Windows... so you don't need to know the path to where
> RenderDoc is running from."*

Canonical snippet: `GetModuleHandleA("renderdoc.dll")` → `GetProcAddress(mod, "RENDERDOC_GetAPI")` →
`RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api)` →
`rdoc_api->StartFrameCapture(NULL, NULL)` / `EndFrameCapture(NULL, NULL)`. Plus `GetAPIVersion`,
`SetCaptureOptionU32/F32` (*"each option only takes effect from after it is set - so it is advised to
set these options as early as possible, ideally before any graphics API has been initialised"*).
— https://renderdoc.org/docs/in_application_api.html

`[CONFIRMED]` The replay side has a Python API for scripted analysis:
https://renderdoc.org/docs/python_api/renderdoc/replay.html,
https://renderdoc.org/docs/python_api/renderdoc/capturing.html, and a remote capture-and-replay
example at https://renderdoc.org/docs/python_api/examples/renderdoc/remote_capture.html.

`[INFERENCE — resolves a deferred item in this repo's own notes]` `CLAUDE.md` lists "in-app RenderDoc
trigger (no vendored `renderdoc_app.h`)" as deferred, and goals 73/105 as blocked on RenderDoc for
the sliver-curtain defect. **The documented integration requires no vendoring in the sense that
matters**: the header ships in every RenderDoc build's root directory, nothing links against the DLL,
and the passive `GetModuleHandle` check makes the code a no-op when RenderDoc is not attached.
`StartFrameCapture`/`EndFrameCapture` around the existing `--dump-every` trigger would let
`--autofly` capture the exact frame the sliver appears on, unattended.

### 6.8 Performance regression, not just visual

`[CONFIRMED]` NVIDIA states the CI framing directly: *"**Upgrade Your CI/CD** — Generate detailed
profiler reports on every developer and artist change. **Add dedicated perf regression criteria by
inspecting GPU metric values.**"*

`[CONFIRMED]` For CPU-side statistical judgement, the Google Benchmark methodology this repo already
uses (`tools/compare.py`, Mann-Whitney U) is the standard; nothing found supersedes it.

`[INFERENCE]` For a frame-budget assertion the non-perturbing primitive is the one already present:
**a GPU timestamp pair per pass, read back a frame late, logged with the pose ID; assert on a
percentile (p95/p99) across a fixed camera path, not on a mean and not on fps.** This machine's
165 Hz FIFO_RELAXED panel caps fps readings at 155–159, which makes fps useless as a regression
signal — which is exactly why the timestamp-derived pass time is the right metric.

---

## 7. Synthesis: what the literature says about this engine's architecture

`[INFERENCE throughout]` Ranked by how strongly the sources support it.

1. **Whole-tree rebuild plus full re-upload is the thing every one of these systems exists to
   avoid.** GigaVoxels rejected per-frame bulk transfer in 2009 (*"the transfer of 512MB each frame
   ... already prevents real time performance"*, CONFIRMED). Nanite's stated principle is *"GPU scene
   representation persists across frames — Sparsely updated where things change"* (CONFIRMED).
   HashDAG's entire contribution is mutating a compressed structure *"without requiring de- and
   recompression"* (CONFIRMED). Aokana streams *"only about 5%"* of the scene (CONFIRMED). **Nothing
   in the literature supports rebuild-and-reupload.**

2. **Adopt relative-within-block addressing before anything else.** ESVO: *"All memory references
   within a block are relative, making it easy to reorganize blocks in memory. **This facilitates
   dynamic memory management necessary for out-of-core rendering.**"* (CONFIRMED). This engine's flat
   global word offsets are the single thing making partial upload impossible; fixing it is a
   prerequisite for every other item.

3. **Split the 16-level 512 m tree into many shallow trees.** Aokana: *"deep data structures connected
   by pointers ... are not cache-friendly ... results in decreased VRAM access performance when the
   voxel scene resolution is high"*; their fix is *"multiple shallow SVDAGs"*, with 2–4× over HashDAG
   above 32K (CONFIRMED). Teardown's shipped answer is the same shape: per-object volumes rasterized
   as boxes, culled and depth-sorted by the rasterizer (CONFIRMED as reported).

4. **Move the marcher to compute, and use the two things only compute gives you.** NVIDIA:
   *"Consider converting your full screen pass to a compute shader if there's a large difference in
   latency between warps"* (CONFIRMED in the lost §4 — re-verify the URL). Thread-group-ID swizzling:
   **47 % on a fullscreen pass, L2 63 % → 86 %** (same caveat), and this workload meets all three of
   NVIDIA's stated preconditions. Persistent threads: **63.6 → 122.1 and 88.0 → 135.6 Mrays/s on
   unchanged traversal code** (CONFIRMED, Aila & Laine Table 2). **Neither is available from a pixel
   shader.**

5. **Add a coarse start-`t` pre-pass.** ESVO's beam optimization (concept CONFIRMED, numbers
   UNVERIFIED), Teardown's manual linear-depth early-out with front-to-back ordering (CONFIRMED as
   reported), Nanite/Aokana Hi-Z (CONFIRMED). Cheap, backend-agnostic, and `PSInvocations` or a
   timestamp pair will say immediately whether it worked.

6. **Make the shader mark usage and file requests; run LRU on the GPU with stream compaction; never
   stall a ray.** GigaVoxels' final design, in the authors' words: usage stamps written during
   traversal, *"a strategy that allows us to avoid any atomic operations in this step"*, *"two stream
   reductions"*, a compact list to the CPU, and *"if LOD not available → Pick next higher available
   level in Mip-map"* (all CONFIRMED). Budget for the failure mode the same authors documented in
   2024 — *"synchronization and starvation of GPU cores"*, worth **2×** when fixed (CONFIRMED).

7. **Attack the moiré with the structure, not just TAA.** Crassin's own admission that pre-integrated
   cone sampling uses the wrong integration function, and his still-open question *"How to pre-filter
   lighting? Pre-filter Normals — How to store them? How to interpolate them?"* (CONFIRMED) — that is
   this engine's attribute-word problem in a different guise. NAADF 2026's answer is a **32-frame TAA
   history** rather than one, justified by voxel-quantized positions and normals being cheap to store
   (CONFIRMED).

8. **Shrink the payload.** Teardown: one byte per voxel plus a palette (CONFIRMED as reported) versus
   this engine's `uint`. Aprons: nearly 1024 MB for a 512³ SVO with materials due to neighbour
   duplication (CONFIRMED, Kämpe et al.); corner-centred 3³ bricks halve it (UNVERIFIED wording).
   SVDAG/SSVDAG show what deduplication buys if the terrain is at all self-similar: **19 G voxels in
   945 MB**, and **100 G voxels in <575 MB at 0.048 bits/voxel with <15 % tracing overhead** (both
   CONFIRMED).

---

## 8. Open gaps a follow-up research pass should close

1. **The lost sections of this report** — §3's remainder (Teardown / Gustafsson, other shipped
   systems and 2023–2026 papers) and §4.1–§4.4 (compute vs pixel shader in full, with the URLs behind
   the 47 % swizzling and L2 63 %→86 % numbers). **Highest priority**, because item 4 of the
   synthesis rests on them.
2. `[UNVERIFIED]` **ESVO's beam optimization: resolution and measured speedup.** Two contradictory
   search answers, no extractable primary text. Get `nvr-2010-001.pdf` locally and read §6.
3. `[UNVERIFIED]` **GigaVoxels I3D 2009 §3–§6 verbatim** — exact node-tile and brick dimensions,
   apron size, the LOD formula, and the results table.
4. `[UNVERIFIED]` **Whether NVIDIA's Vulkan driver exposes `VK_KHR_performance_query`.** Check
   `vulkaninfo` on the 4070 directly — two seconds, and neither answer found is trustworthy.
5. `[UNVERIFIED]` **No primary source for a "first timestamp in a command buffer" caveat.** The
   observed NVIDIA driver fault appears undocumented.
6. `[UNVERIFIED]` **The 2026 Teardown talks** — *"Raytracing Voxels in Teardown and Beyond"*
   (https://www.youtube.com/watch?v=IM1Dr98f3xU) and *"Teardown Developers Reveal New Voxel RT
   Technology"* (https://www.youtube.com/watch?v=Hr_Olt52Xl0). No transcript retrievable; Gustafsson
   confirms *"new tech ... for a new game in a completely redesigned engine"* (CONFIRMED).
   **The single highest-value unresolved source**: the one shipped, profitable voxel-ray-marching
   game engine, being rebuilt for hardware RT, whose author talks publicly about it. YouTube pages
   return boilerplate to fetchers — recover text via search-indexed mirrors, video descriptions, or
   Reddit crossposts.
7. `[UNVERIFIED]` **Aokana's and HashDAG's own results tables** (frame times, VRAM, hardware) —
   reached only through search synthesis.
8. `[UNVERIFIED]` **UE Gauntlet's current documented screenshot-comparison thresholds** —
   dev.epicgames.com returned 403.
9. **Not researched:** NAADF's open-source implementation (GitHub link on
   https://www.cg.tuwien.ac.at/research/publications/2026/ulschmid-2026-naadf/) and
   `mathijs727/GPU-SVDAG-Editing` (https://github.com/mathijs727/GPU-SVDAG-Editing), a GPU-side
   successor to HashDAG. Both look directly usable as references.

---

## §9. Appendix: §8.1's lost stretch, recovered (Prompt 004, 2026-09-07)

Recovered by one read-only research subagent, as Prompt 004 rule 11 requires. CONFIRMED = the cited
source states it; INFERENCE = derived. **Two of these findings contradict this document's own §2.5
and §7, and the contradiction is the point — see §9.3.**

### §9.1 Fullscreen pixel shader vs compute, for a heavy raymarch

- **CONFIRMED** (Drobot, *GCN Execution Patterns in Full Screen Passes*,
  https://michaldrobot.com/2014/04/01/gcn-execution-patterns-in-full-screen-passes/): measured on the
  same shader writing a UAV — 2-triangle quad PS **87% cache hit / 100% perf**; **1 fullscreen
  triangle PS 95% / 108%**; **compute 95% / 108%**. The single-triangle PS and compute **tie**. The
  8% was over the two-triangle quad.
  **Consequence for this engine: we already draw `Draw(3)`, so the cache-locality argument for
  moving to compute is worth nothing here.**
- **CONFIRMED** (D3D spec, SM 6.6/6.7 wave-ops docs): pixel shaders run 2x2 quads with helper lanes.
  **INFERENCE**: a *fullscreen triangle* has ~100% coverage, so there are ~zero helper lanes. The
  "quads waste lanes" argument is real for small triangles and `discard`-heavy shaders and **does not
  apply to us**. No source was found claiming otherwise.
- **CONFIRMED and this is the one that does apply** — Sebastian Aaltonen, on a shipped title,
  describing a shadow cone-trace (a cheap early-out path plus a slow path, i.e. our shape exactly):
  *"ROP exports apparently retire in submission order... Moved shadow cone trace PS->CS = 50% perf
  gain (both Nvidia and AMD)"*, and *"Splitscreen shadow ray-trace (PS->CS)... 1.89ms -> 0.54ms.
  Performance = 3.4x."* (mirror: https://www.unrollnow.com/status/1011211972904472576 — single
  source, transcription unverified against the original posts).
  Corroborated by NVIDIA's own *Advanced API Performance: Shaders* (2023-09-01,
  https://developer.nvidia.com/blog/advanced-api-performance-shaders/): *"Consider converting your
  full screen pass to a compute shader if there's a large difference in latency between warps."*
  **An SVO marcher over sparse terrain has exactly that: sky pixels exit in a few steps, grazing
  ground pixels march hundreds.**

### §9.2 Thread-group-ID swizzling — the 47% / 63%-to-86% figures are exact

**CONFIRMED**, primary source found: Louis Bavoil, NVIDIA, 2020-07-16,
https://developer.nvidia.com/blog/optimizing-compute-shaders-for-l2-locality-using-thread-group-id-swizzling/
— *"horizontal thread-group-ID tiling with N=16 produced a 47% gain on a full-screen, denoising,
compute shader pass in Battlefield V DXR at 1440p on a RTX 2080 with SetStablePowerState(TRUE)...
the L2 read hit rate has greatly increased (from 63% to 86%)"*. First presented at GDC 2019.
Details usually dropped in retelling: it is **not** Morton order (*"a simpler approach works better
in practice"*); the GDC HLSL snippet was **buggy** and the maintained version is at
https://github.com/LouisBavoil/ThreadGroupIDSwizzling; and it only pays when **all three**
preconditions hold — VRAM is the top-throughput unit, L2 hit rate is well under 80%, and adjacent
thread groups' footprints overlap. **It is also a compute-only lever** — the rasteriser owns launch
order for a `Draw(3)`.

### §9.3 Persistent threads: this document's §2.5 cites a result its own authors retracted

**This is the correction that matters.** §2.5 quotes Aila and Laine 2009's persistent-threads
speedups (63.6 -> 122.1 and 88.0 -> 135.6 Mrays/s) as motivation for an application-managed work
queue.

- **CONFIRMED**: those numbers are exact — Table 2, *Conference* scene, primary rays, **GTX 285**
  (https://users.aalto.fi/~ailat1/publications/aila2009hpg_paper.pdf). The paper's own explanation is
  *"previously unidentified inefficiencies in hardware work distribution"* — i.e. **a 2009 hardware
  defect**.
- **CONFIRMED**: the same authors withdrew the recommendation three years later. Aila, Laine and
  Karras, *Kepler and Fermi Addendum*, NVIDIA TR **NVR-2012-002**
  (https://users.aalto.fi/~ailat1/publications/aila2012hpg_poster.pdf), Table 2, row "Persistent
  threads": Tesla **"Doubles performance."** -> Fermi **"Not beneficial due to a better hardware work
  distributor."** -> Kepler **"+10%"**, and only as lane compaction at a 60%-utilisation threshold,
  not as a work queue. Fermi is 2010; Ada is four generations further on.
- **CONFIRMED, modern**: the hardware-scheduled successor measures *slower*. Anagnostou, 2024-09,
  https://interplayoflight.wordpress.com/2024/09/09/an-introduction-to-workgraphs-part-2-performance/
  — FidelityFX SSSR classify+raymarch on an **RTX 3080 at 1080p**: compute **0.65 ms** vs work graph
  **2.18 ms (3.4x slower)**; occupancy 25 warps (52%) vs 13 (27%).
- **CONFIRMED, portability**: Sorensen et al., OOPSLA 2021
  (https://johnwickerson.github.io/papers/gpu_progress_OOPSLA21.pdf) — across **eight GPUs from five
  vendors, none** supported the occupancy-bound forward-progress model persistent threads assume;
  **non-termination observed on Apple and ARM**. A persistent-thread kernel can deadlock on hardware
  we do not own.
- The surviving respectable form is **wavefront/stage decomposition**, not persistent threads —
  Laine, Karras and Aila, *Megakernels Considered Harmful* (HPG 2013), and a 2026 re-measurement
  reporting **~16%** (https://arxiv.org/html/2605.27323), attributed to cache locality rather than
  work distribution.

**INFERENCE, and it is a recommendation against this document's §7 item on persistent threads: do
not build the work queue.** Test a straight PS-to-CS port first (§9.1's mechanism), measure Bavoil's
three preconditions before swizzling, and treat persistent threads as retracted.

### §9.4 Teardown, recovered

Primary source: Gustafsson and Rundlett, *"Raytracing Voxels in Teardown and Beyond"*, 2026-04-10,
https://www.youtube.com/watch?v=IM1Dr98f3xU (venue not identified — do not cite one).

- **CONFIRMED, his own words**: *"It's written in OpenGL... OpenGL 3.3. So I didn't really have
  compute shaders. So everything you see was implemented using the normal rasterization pipeline."*
  Per object: a **3D texture, one byte per voxel**, indexing a **palette** carrying colour,
  roughness, metallicity and material type.
- **CONFIRMED**: rasterise each object's **oriented bounding box, back faces only** (12 triangles),
  reconstruct the ray in the fragment shader, and march with **Amanatides and Woo (1987)**.
- **CONFIRMED**: LOD is *"a number of mipmaps used as an octree acceleration structure... step in
  the lower mips first... and we can go back up and down the mip chain as we traverse."*
- **CONFIRMED, and directly applicable**: overdraw is the problem because *"we cannot use early Z...
  finding the depth of the fragment we're shading is part of the slow path"*, with ~1000-3000 boxes
  on screen. Fix: **sort objects into distance bins, draw bin 0, copy the depth buffer, and for each
  later bin compare the box's front face against that copy and kill the ray immediately.** A manual
  early-out standing in for the depth pre-pass we also cannot do.
- **CONFIRMED** (2018, https://blog.voxagon.se/2018/10/17/from-screen-space-to-voxel-space.html): a
  second, dual representation — one global axis-aligned occupancy texture for light transport, bit-
  packed **eight octants per byte** plus two mips, **292 MB instead of 2 GB**; *"all ambient
  occlusion, lighting, fog and reflections... in about 9 ms, including denoising... full HD... on a
  GTX 1080."*
- **INFERENCE**: Teardown does **no voxel streaming** — the shadow volume is one texture sized to fit
  VRAM, and independent frame captures note no pop-in. Its only LOD is the per-object mip chain.

**The counter-datapoint worth weighing: a well-regarded voxel raymarcher shipped entirely in fragment
shaders on OpenGL 3.3, and solved our exact divergence problem with depth binning and a mip chain,
not with compute.**
