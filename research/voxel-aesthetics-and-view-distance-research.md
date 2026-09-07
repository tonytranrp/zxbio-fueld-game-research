# Sub-Pixel Voxel Aesthetics: Pre-Filtering, Deliberate Grain, Reconstruction, and How Far the Eye Can Use

**Provenance.** Written 2026-09-06 by the side session for Prompts 005 and 007. One read-only
web-research subagent ran with live search/fetch against a brief naming this engine's two actual
complaints (concentric ring moiré at distance; big flat cubes / "lumps" up close) and its actual
architecture (sparse-brick octree, 7.8 mm voxels, one primary ray per pixel plus 8-frame TAA,
int8×3 average normal + uint8 coverage per node). Every claim carries a label and a URL.

**Labels.** `[C]` = CONFIRMED, verbatim from a primary source (paper, vendor doc, or the creator's
own words), quoted. `[I]` = INFERENCE, with the work shown. `[U]` = UNVERIFIED.

**One verification done by the side session itself, not by the subagent, and it changes the
priority order of Prompt 005.** See §1.3-A's boxed note: the finding that this engine already
computes and then discards the Toksvig roughness signal was checked directly against
`world/svo/detail/tree_builder_impl.hpp` on 2026-09-06 and is `[C]` against the repo.

---

## 0. Both artefacts already have names in the literature

This matters because it says which body of work to read.

### (a) The rings are "onion rings", and the cause is *correlated* sampling error — not too few samples

`[C]` Ruijters, *Common Artifacts in Volume Rendering* (arXiv 2109.13704), verbatim:

> *"Equidistant sampling: The stripes in the 'onion ring' pattern arise from the fact that the rays
> in neighboring pixels are sampled at similar distances, yielding comparable errors (in magnitude
> and sign). When the sample locations of the rays in adjacent pixels is varied (e.g., by using
> variable sample distances, or a random offset at the beginning of each ray), the stripe pattern is
> broken. **This leads to a more 'dithered' image, as the errors are still present but now randomly
> distributed over the pixels.**"*

— https://arxiv.org/pdf/2109.13704

`[I]` **That last sentence is the most important line in this document for this engine: the
textbook remedy for the ring artefact is to convert it into a dither. The two complaints are the
same problem, and the fix for the rings is a step toward the look that is wanted.**

### (b) The close-range "lumps" are "blockiness caused by discrete sampling of shading attributes"

`[C]` Laine & Karras, *Efficient Sparse Voxel Octrees* (I3D 2010) §4.3: *"To smooth out the
blockiness caused by discrete sampling of shading attributes, we apply an adaptive blur filter on
the rendered image as a post-processing step. Without filtering, the result would resemble the
effect of nearest-sampled texture lookups."*
— https://users.aalto.fi/~laines9/publications/laine2010i3d_paper.pdf

`[C]` Heitz & Neyret, HPG 2012, on Carmack-era voxel engines: *"Because their voxels are not
interpolated they look like Lego(TM) bricks at close view, and aliasing is high."*
— http://diglib.eg.org/bitstream/handle/10.2312/EGGH.HPG12.125-134/125-134.pdf

`[C]` And a direct hit on the exact symptom, from a Luanti/Minetest contributor (lhofhansl), GitHub
issue #14285: *"I notice some annoying moire patterns, especially with blocks fairly far away - like
300-400+"* … *"Note that these are due to mapnode boundaries that are rendered with a higher
frequency then the screen can render. **FXAA does not help with that, and SSAA is way to too
expensive**"* … *"This really needs a geometry interpolation."*
— https://github.com/luanti-org/luanti/issues/14285

`[I]` **MSAA/FXAA are reported ineffective against voxel-lattice moiré by someone who measured it.**
The aliasing frequency lives in the lattice, not in any material — which is the argument for
pre-filtering the *geometry hierarchy* rather than any per-voxel texture. `[C]` stated the same way
on Stack Exchange about Minecraft-style grids: moiré *"occur when a repetitive pattern of high
spatial frequency is sampled at low resolution"* and *"in your Minecraft example, the 'texture' is
effectively made across multiple blocks, so it can't be mitigated by mipmaps."*
— https://gamedev.stackexchange.com/questions/202305/name-explain-shimmering-patterns-seen-when-looking-at-a-grid-of-objects-from-a

---

## 1. Anti-aliasing and pre-filtering for ray-marched voxels

### 1.1 The correct operation is a cone, not a ray — stated outright

`[C]` Crassin, Neyret, Lefebvre, Eisemann, *GigaVoxels* (I3D 2009), verbatim:

> *"We also address aliasing, which appears because **the value of a pixel should not be defined by
> accumulations along a ray, but by the integration along a cone defined by the eye and the pixel's
> extent.** Computing this value with several rays (FSAA methods) increases computation time
> significantly. As for 2D texturing, a way to overcome this issue is to use mipmapping.
> Equivalently, a voxel hierarchy can be built by iteratively downsampling and averaging neighbors.
> Due to the filtering process, reading from a mipmap delivers the integrated value of a small
> volume. If the step size during the ray marching is chosen accordingly to the distance from the
> observer, it is possible to obtain a good approximation of the actual cone integral and accelerate
> computations. Mipmapping thus addresses aliasing to a large extent, smoothes the results and can
> increase rendering performance, but it increases memory consumption and makes tree updates more
> challenging."*

— https://maverick.inria.fr/Publications/2009/CNLE09/CNLE09.pdf

`[C]` The traversal-termination rule, same paper §5.1: *"The descent stops when we reach a node with
the appropriate level-of-detail (not necessarily a leaf). Such a node either represents a constant
region of space, or contains a brick whose resolution is fine enough so that **a voxel projects to
at most one pixel**."* And: *"One important observation is that our traversal does not need the
structure to indicate correct level-of-details … This is determined in the shader."*

`[C]` **How many mip levels must be blended**, §5.2: *"One might think that many mipmap levels might
be necessary to perform this internal filtering, but a brick only describes a small extent of
space. It can be shown that for a small near plane offset **three levels are enough. Three is also
the minimum** because, if the filter kernel at the entry point is only slightly below the next
mipmap level, it might exceed it when the cone leaves the brick. **For proper blending three levels
are a must.**"* Kept in *"a small queue of three elements implemented in shader registers without
using dynamic indexing operations."*

`[C]` Laine & Karras do it more cheaply: *"Line 11 checks whether the voxel is small enough to
justify termination of the traversal. This provides a way to pre-filter the geometry by dynamically
adapting voxel resolution to match the screen resolution, and is accomplished by comparing
exp2(scale) against a linear function of tc_max."*

`[C]` GigaVoxels perf: *"at 20-90 fps and respect the limited GPU memory budget."*

### 1.2 The footprint math, in closed form

`[C]` Ray Tracing Gems ch. 20 (Akenine-Möller, Nilsson, Andersson, Barré-Brisebois, Toth, Karras).
Eq. 30, the pixel spread angle: *"α = arctan( 2 tan(ψ/2) / H )"* where *"ψ is the vertical field of
view and H is the height of the image in pixels. Note that α is the angle to the center pixel."*
Cone width at the first hit: *"the cone width will be w0 = 2||d0|| tan(α/2) ≈ α||d0||"*, propagated
as *"w1 = w0 + γ1·t1"*. The LOD, Eq. 26: *"λ = Δ0 + log2( α||d0|| · 1/|n̂0 · d̂0| )"*, with the
projection term explained verbatim: *"the larger the angle, the more the ray can 'see' of the
triangle surface, and consequently, **the LOD should increase**"* and *"where |n̂0 · d̂0| models the
square root of the projected area."* And, bridging to §6: *"for foveated renderers with eye tracking
[12], one may wish to use a larger α in the periphery."*
— https://media.contentapi.ea.com/content/dam/ea/seed/presentations/2019-ray-tracing-gems-chapter-20-akenine-moller-et-al.pdf

`[I]` **This engine already has α** (`g_ShadeParams.w`, the raw pixel angle). What it does **not**
have is the **grazing term `1/|n̂·d̂|`** — and rings live at grazing angles, exactly where an
isotropic radius under-estimates the footprint. Replacing the LOD rule with
`level = log2(α·t / |n̂·d̂| / voxel_size)` is the single most targeted change available.

`[C]` A distance-only alternative, shipped and measured — Fang, Wang & Wang, *Aokana* (PACMCGIT
8(1), May 2025), Eq. 1: *"LODError = (ChunkSize × StreamingFactor) − ||ChunkCenterPos −
CameraPos||"* … *"The StreamingFactor is a predefined parameter; the larger its value, the finer the
details at greater distances, which correspondingly requires more VRAM."* With a measured quality
curve: *"when the StreamingFactor is 2.0, **the SSIM is near 0.9**, and it maintains relatively high
rendering quality while loading fewer chunks."* Their LOD build rule: *"If the number of non-empty
voxels among these eight is greater than or equal to a predefined density, we create a new voxel and
use the average color of these voxels as the color of the aggregated voxel. In our implementation,
we set density = 2."* — https://arxiv.org/pdf/2505.02017

### 1.3 What to store per node — four options, increasing cost

#### Option A — average-normal LENGTH as the variance. This engine already computes it and throws it away.

`[C]` Crassin et al., *Interactive Indirect Illumination Using Voxel Cone Tracing* (I3D 2011) §4.3:
*"we choose to store only isotropic Gaussian lobes characterized by an average vector D and a
standard deviation σ. Following [Tok05], **to ease the interpolation, the variance is encoded via
the norm |D| such that σ² = (1−|D|)/|D|**."* And §7, the shading side: *"the NDF can be computed from
the length of the averaged normal vector |N| that is stored in the voxels, via the approach proposed
by [Tok05] (σ²n = (1−|N|)/|N|)."*
— https://research.nvidia.com/sites/default/files/pubs/2011-09_Interactive-Indirect-Illumination/GIVoxels-pg2011-authors.pdf

`[C]` The Toksvig remap in its game form, plus the CLEAN variant, from Stephen Hill:
Toksvig factor `f_t = |N_a| / (|N_a| + s(1 − |N_a|))`, with `σ²_toksvig = (1 − |N_a|)/|N_a|` and
CLEAN's `σ²_clean = M_z − (M_x² + M_y²)`. His assessment, verbatim: *"baked Toksvig/CLEAN Mapping is
still a hell of a lot better than doing nothing, which is precisely what most of us are doing at the
moment"*, and the limitation: *"it's certainly not as good on account of the lack of anisotropy,
which can mean over-broadening of the specular highlight in some cases."*
— https://blog.selfshadow.com/2011/07/22/specular-showdown/

> ### `[C]` **VERIFIED AGAINST THIS REPO, 2026-09-06 — the signal is computed and discarded in one line.**
>
> `world/svo/detail/tree_builder_impl.hpp` accumulates exactly the right quantity. Its own comment:
> *"What a built node reports upward (Group Z): **its area-weighted exposed-face normal sum (world
> units squared, so coarse and fine children mix by real surface area)** and the fraction of its
> volume that is occupied."* The struct:
> ```cpp
> struct NodeSummary { glm::vec3 normal_sum{0.0f}; float coverage = 0.0f; };
> ```
> accumulated up the tree as `mine.normal_sum += cs.normal_sum;` and seeded at brick leaves from
> `glm::vec3{brick.exposed_face_sum()} * (voxelEdge * voxelEdge)`. Then, at the pack site:
> ```cpp
> [[nodiscard]] static std::uint32_t pack_summary(const NodeSummary& s) noexcept {
>     const float length = glm::length(s.normal_sum);
>     const glm::vec3 n = length > 1.0e-12f ? s.normal_sum / length : glm::vec3{0.0f};
>     return make_node_attributes(n, s.coverage);
> }
> ```
> **`length` is computed and then dropped on the floor.** The area-weighted normal sum — the
> correct quantity, in the correct units, already mixing coarse and fine children by real surface
> area — is normalized to a unit vector before quantizing, which is precisely the operation that
> destroys the anti-aliasing signal.
>
> **One subtlety the research does not state and that matters for the implementation.** Toksvig's
> `|N_a|` is the length of the average of *unit* normals, in [0,1]. `normal_sum` here is in absolute
> area units (world units²), so its raw length is **not** `|N_a|` — you need
> `|N_a| = |Σ nᵢAᵢ| / Σ Aᵢ`, i.e. the vector sum's length divided by the **total exposed face
> area**. That denominator is **not currently accumulated**: the builder has `childFaceArea` and
> `brick.exposed_face_sum()` available at the seeding sites, so it is derivable, but `NodeSummary`
> needs a third field (`float area_sum`) and one more `+=` in the accumulation loop.
>
> **Storage.** The attribute word is full: 32 bits = 3×8 snorm normal + 8 coverage. Options are
> (i) a second attribute word — at 246,046 internal + 657,034 brick-leaf nodes at the default pose
> that is ~3.6 MB, which should be measured rather than assumed; or (ii) split the existing coverage
> byte 4/4 between coverage and mean-length. `[I]` (ii) is likely sufficient: coverage feeds an
> early-out threshold (`kSecondaryCoverage = 0.35`) that does not need 8 bits, and a 4-bit
> mean-length gives 16 roughness buckets, which is more than a `σ²` remap needs to look smooth.
> **Measure both.**

#### Option B — the full second-moment representation (LEAN / LEADR): 5 scalars, linearly filterable

`[C]` Olano & Baker, *LEAN mapping* (I3D 2010). Seed the top mip with
`B = (b̃n.x, b̃n.y)` (3) and `M = (b̃n.x², b̃n.y², b̃n.x·b̃n.y)` (4) — where `b̃` denotes division by
`b⃗.z` — then reconstruct after any linear filter:
```
Σ = [ M.x − B.x·B.x   M.z − B.x·B.y ]
    [ M.z − B.x·B.y   M.y − B.y·B.y ]                (5)
```
and shade with the off-centre Beckmann
`(1 / (2π√|Σ|)) · exp( −½ (h̃n − b̃n)ᵀ Σ⁻¹ (h̃n − b̃n) )` (1).
Verbatim on why: *"**Neither the covariance matrix, nor upper-triangular decomposition combine
linearly, but the second moments do**, and can be used to reconstruct the elements of Equation
(2)."* And: *"This combination of bump covariances in a common space are the key to transitioning
large scale bump behavior into microfacet shading behavior."*
— https://redirect.cs.umbc.edu/~olano/papers/lean/lean.pdf

`[C]` **The base-roughness convolution — the part that matters most**, verbatim: *"Han et al. [2007]
show that an existing BRDF can be combined with a normal distribution by convolution… Fortunately,
the Fourier transform of a Gaussian is another Gaussian with the inverse variance."* The derivation,
quoted exactly:
```
e^(−½(h̃n−b̃n)ᵀΣ⁻¹(h̃n−b̃n)) ⊗ e^(−(s/2)(h̃n−b̃n)ᵀ(h̃n−b̃n))
  = e^(−½(h̃n−b̃n)ᵀ(Σ + (1/s)I)(h̃n−b̃n))
```
*"We can compute the results of the convolution by just adding 1/s to the x² and y² terms of Σ"*,
giving `M = (b̃n.x² + 1/s, b̃n.y² + 1/s, b̃n.x·b̃n.y)` (6). And the Blinn-Phong↔Beckmann bridge:
*"cos(θ)^s ≈ e^(−(s/2)tan²θ). The Beckmann distribution should be normalized by multiplying by
s/(2π)."*

`[C]` **LEADR** (Dupuy, Heitz, Iehl, Poulin, Neyret, Ostromoukhov, SIGGRAPH Asia 2013) — the same
representation applied to *displacement*, plus analytic masking-shadowing. Stored quantities:
*"similarly to LEAN mapping, we store the terms E[x̃n], E[ỹn], E[x̃n²], E[ỹn²], and E[x̃n·ỹn], i.e.,
**five scalar values**, in two mipmapped texture maps."* Covariance (Eq. 9):
`σ²x = E[x²ñ] − E²[xñ]`, `σ²y = E[y²ñ] − E²[yñ]`, `cxy = E[xñyñ] − E[xñ]E[yñ]`. Slope PDF (Eq. 8):
`P22(ñ) = exp(−½(ñ−E[ñ])ᵀΣ⁻¹(ñ−E[ñ])) / (2π√|Σ|)`. NDF (Eq. 10): `D(ωn) = P22(ñ) / (ωn·ωg)⁴`, with
*"The factor 1/(ωn·ωg)⁴ is due to the Jacobian |∂ñ/∂ωn| = 1/(ωn·ωg)³ and the inverse projection
1/(ωn·ωg) that normalizes the PDF."* Mean normal from the same data (Eq. 11):
`ω_n̄ = (−E[x̃n], −E[ỹn], 1)ᵀ / √(1 + E²[x̃n] + E²[ỹn])`.
— https://perso.liris.cnrs.fr/victor.ostromoukhov/publications/pdf/SAsia2013-LEADR.pdf

`[C]` **Measured cost** — LEADR §7, GTX 480, 1024×1024 full-screen quad, one directional light,
specular, ms: **constant 0.055 | Blinn-Phong 0.146 | LEAN 0.266 | LEADR 0.267**. Verbatim: *"Here,
LEADR mapping is as fast as LEAN mapping despite requiring more arithmetic operations. This is most
probably due to the bottleneck texture fetches during rendering from the LEADR map. **Compared to a
naive Blinn-Phong shader, the performance differences are also quite negligible.**"* Environment
lighting is where it gets expensive: specular 0.402 / 0.433 / 1.704 / 4.284 ms for 1 / 3×3 / 5×5 /
7×7 samples; diffuse 0.148 / 1.067 / 2.750 / 5.027. Full T-rex asset: *"performances ranging between
40 and 60 milliseconds at an image resolution of 1024 × 1024"*, 2.8 GB of displacement textures.

`[I]` **Translation: the per-pixel arithmetic of a covariance-based filtered NDF is ~0.12 ms/Mpixel
class — free. The cost is storage and bandwidth, not ALU.**

#### Option C — the voxel-native version: Heitz & Neyret's per-voxel Gaussian descriptors

**This is the paper written for exactly this data structure, and it is the one to read first.**

`[C]` Abstract: *"We store macro- and micro-descriptors of the surface shape and associated
attributes in each voxel. We represent the surface macroscopically with a signed distance field and
we encode subvoxel microdetails with **Gaussian descriptors of the surface and attributes within the
voxel**. Our voxels form a continuous field interpolated through space and scales, through which we
cast conic rays."*

`[C]` §5.2, the exact per-voxel payload:
> *"• Macroscopic distance field h̄ and the variance of its microscopic oscillation amplitudes σ²h
> • Macroscopic normal n̄ and the roughness σ²n of the microgeometry. The associated NDF is a
> Gaussian lobe with mean n̄ and slope variance σ²n (σ²nx and σ²ny in the anisotropic case)
> • Microscopic distributions of each attribute with ā and as."*

`[C]` **The interpolation gotcha**: *"Note that we store and interpolate **variances σ², which is the
quantity that interpolates linearly** (and is thus suited for hardware interpolation)."*

`[C]` Memory: *"We use two channels for the distance field parameters (h̄ and σ²h), four (isotropic)
or five (anisotropic) channels for the macro-normal (n̄) and the micro-NDF (σ²n) and two channels per
surface attribute (ā and as)… While 32-bit precision is preferable for the distance field, 8- or
16-bit channels are reasonable for the other components. Thus, our representation handles
multi-scale geometry, view-dependent filtered RGB colors and shading for **an average 15-20 bytes
per voxel** (possibly less at the deepest level where σ²p = 0). This is about two or three times as
much as in [CNLE09] with RGBA values and normals."*

`[C]` **How the NDF meets the BRDF — one addition**: *"We rely on their Gaussian slope statistics
N(n̄, σ²n) representation [CT81, ON94]. The initial microfacet statistics of the BRDF σ²nρ is
progressively enriched with the filtering of meso-surface normals σ²n. **Convolving two random
Gaussian variables comes down to adding the variances.** At runtime, we compute the shading with the
convolved BRDF with variance σ²n + σ²nρ."*

`[C]` The sub-pixel **coverage** machinery — relevant because this engine already stores a coverage
byte: they revive Carpenter's A-buffer. *"With his A-buffer algorithm [Car84], Carpenter proposed a
representation of the subpixel occupancy through a compact bitmask (possibly stored in the bits of a
single int)… Our fragments will correspond to traversed voxels, introducing a view-independent 3D
vector mask to represent subvoxel occupancy. When marching along a ray, these will generate 2D
bitmasks combined as for A-buffer."* The masks are precomputed and indexed by two parameters:
*"The state of each bit (ωx,ωy) of the mask and thus the distribution 1A(ω,[zd,zd+1]) is then
entirely described with the two parameters (θ,v). We pre-compute each mask and store it as an
integer value in a 2D texture parametrized by (θ,v). At the runtime, for each cone element d, we
compute θ and v and fetch the texture in nearest mode to get the mask."*

`[C]` The view-dependent attribute closed form (Eq. 17):
`ā(v,l) = ā + as( 2^(Λ(v)+Λ(l)+1) / (Λ(v)+Λ(l)+2) − 1 )`, from Smith's visibility
`V(v,h) = g(h)^Λ(v)`.

`[C]` **Measured performance**, NVIDIA GTX 560, CUDA, 512×512: *"The typical performances are 40-60
fps without shadows and 10-25 fps with shadows"* … *"**While zooming in, the cost per covered pixel
is nearly constant around 0.1-0.3 µs/pixel.** This cost mainly depends on the presence of
silhouettes: views with no silhouettes are the fastest, views with large grazing areas are the most
expensive since several cone elements per ray are computed."* Table: far 57 fps / mid 37 / close 25 /
closer 19 fps; 0.06–0.57 µs/pixel. Memory: *"a resolution of 512³ and requires 300 MB storage on the
GPU"*, with procedural amplification to *"a virtual resolution of 8192³."* Accuracy: *"The maximum
error of our method is less than 1% on a Perlin noise height map… On a real-world texture which is
not really Gaussian, the error is about 5% at grazing angles."*
— http://diglib.eg.org/bitstream/handle/10.2312/EGGH.HPG12.125-134/125-134.pdf

`[I]` **The headline to take from this paper is its abstract's claim that the timings per pixel are
scale-independent.** That is exactly the property this engine lacks: constant cost whether one voxel
or 128,000 voxels land in a pixel (see §5's table).

#### Option D — directional/anisotropic voxels, if light leaks

`[C]` Crassin 2011 §9: *"Instead of a single channel of non-directional values, voxels will store 6
channels of directional values, one per major direction. A directional value is computed by doing a
step of volumetric integration in depth, and then averaging the 4 directional values to get the
resulting value for one given direction… At render time, the voxel value is retrieved by linearly
interpolating the values from the three closest directions with respect to the view direction."*
Cost: *"storing directional values for all the properties only increases the memory consumption by
**1.5x**."* The reason: the *"two red-green wall problem"* — *"when a set of 2x2x2 voxels is half
filled with opaque voxels and half filled with fully transparent ones, the resulting averaged voxel
will be half-transparent."*

### 1.4 The zero-storage route: screen-space derivative NDF filtering — four lines of HLSL

Kaplanyan, Hill, Patney & Lefohn, *Filtering Distributions of Normals for Shading Antialiasing*,
HPG 2016, pp. 151–162 — https://diglib.eg.org/items/8a23c0fd-2da5-4ade-8c69-6426e074da49.
**`[U]` The PDF could not be opened** (kaplanyan.com's certificate has expired; diglib returns 403).
**The math below is quoted verbatim from Tokuyoshi & Kaplanyan's own 2019 restatement, which is a
co-authored primary restatement, not a third-party summary.**

`[C]` Tokuyoshi & Kaplanyan, *Improved Geometric Specular Antialiasing*, I3D 2019 §2:

> *"The filter kernel is given as a covariance matrix Σ calculated for each pixel as follows:
> Σ = σ²·[Δh∥u ; Δh∥v]ᵀ[Δh∥u ; Δh∥v], where **σ = 0.5 is the standard deviation of the pixel filter
> kernel in image space measured in pixels**, and Δh∥u and Δh∥v are the derivatives of the
> halfvector in slope space with respect to image space axial pixel offsets. The filtering process
> is a convolution of the estimated kernel with the NDF of the material. Since the Beckmann NDF is a
> 2D Gaussian distribution in slope space, this filtering becomes a convolution of two Gaussian
> distributions, which has a simple closed-form solution. Hence, the resulting filtered NDF is also
> an anisotropic Beckmann NDF that uses the following 2×2 matrix as its roughness parameter:
> **A = diag(α²x, α²y) + 2Σ** … It was also shown that this roughness matrix can be used to
> approximate the filtering of the GGX NDF."*

— https://yusuketokuyoshi.com/papers/2019/ImprovedGeometricSpecularAA.pdf

`[C]` The deferred/isotropic simplification, Eq. 1 & 2: `ᾱ² = α² + min(2λmax, κ)` where *"**κ = 0.18
is the clamping threshold** used in the Kaplanyan et al. [2016]'s original axis-aligned filtering to
suppress the estimation error of derivatives"*, and
`λmax ≤ λmin + λmax = tr(Σ) = σ²(‖Δn̄⊥u‖² + ‖Δn̄⊥v‖²)`. Derivative shortcut:
*"‖Δn̄⊥u‖ = 2 sin θu = ‖n − nu‖."*

`[C]` **The whole implementation, Listing 2, verbatim:**
```hlsl
float3 dndu = ddx(normal), dndv = ddy(normal);
float variance = SIGMA2 * (dot(dndu, dndu) + dot(dndv, dndv));
float kernelRoughness2 = min(2.0 * variance, KAPPA);
float filteredRoughness2 = saturate(roughness2 + kernelRoughness2);
```

`[C]` **Measured cost, Table 2 — forward shading at 8K, AMD Radeon RX Vega 56, ms:**

| Scene | w/o SAA | Non-axis-aligned (theirs) | Normal-based isotropic, Avg (Eq. 5) |
|---|---|---|---|
| Sponza (262 k tris) | 1.80 | 2.31 | **1.88** |
| Bistro (814 k tris) | 2.06 | 2.57 | **2.17** |
| San Miguel (5.3 M tris) | 3.65 | 4.12 | **3.74** |

`[I]` **At 8K the normal-based isotropic variant costs 0.08–0.11 ms over no anti-aliasing at all.
This is effectively free and there is no reason not to have it.**

`[C]` **Their temporal-stability warning, which matters because this engine uses TAA:** *"In this
experiment, our non-axis-aligned filtering is the highest quality in terms of the RMSE metric, while
**some pixels can still flicker in animation**… For such dynamic scenes, **biased axis-aligned
filtering is more practical because of the temporal stability**."*

`[C]` **The 2021 refinement** — Tokuyoshi & Kaplanyan, *Stable Geometric Specular Antialiasing with
Projected-Space NDF Filtering*, JCGT 10(2), 2021 — moves the filter out of slope space (which blows
up at grazing halfvectors) into orthographically-projected space. Listing 1, note the changed
constant:
```hlsl
float SIGMA2 = 0.15915494;                      // = 1/(2π)
float2x2 delta = {ddx(halfvectorTS.xy), ddy(halfvectorTS.xy)};
float2x2 kernelRoughnessMat = 2.0 * SIGMA2 * mul(transpose(delta), delta);
float2 projRoughness2 = roughness2 / (1.0 - roughness2);          // slope -> projected
// filter in projected space, then A = (B^-1 + I)^-1  back to slope space
```
with *"For numerical stability, the determinant is clamped with the lower bound."* Headline
improvement, Fig. 1 at roughness α = 0.01: **RMSE 12631.7 → 0.134, MAE 9.73 → 0.00651.**
— https://jcgt.org/published/0010/02/02/paper.pdf

### 1.5 Band-limiting: fade detail to its mean rather than blurring the image

`[C]` Norton, Rockwood & Skolmoski, *Clamping: A method of antialiasing textured surfaces by
bandwidth limiting in object space*, Computer Graphics (Proc. SIGGRAPH 82) 16(3):1–8. Abstract:
*"an object space method for interpolating between sampled and locally averaged signals, resulting
in an antialiasing filter which provides a continuous transition from a sampled signal to its
selectively dampened local averages. This method is applied to the three standard Euclidean
dimensions and time, resulting in spatial and frame to frame coherence."*
— https://dl.acm.org/doi/abs/10.1145/965145.801252

`[C]` The modern shader form, Iñigo Quílez, *Band-limiting*: *"filtering a cosine wave with a box
filter is the same as multiplying it with a sinc() function"*
```glsl
float fcos(in float x){ float w = fwidth(x); return cos(x)*sin(0.5*w)/(0.5*w); }
// or the cheap approximation:
float fcos(in float x){ float w = fwidth(x); return cos(x)*smoothstep(k2PI, 0.0, w); }
float fnoise(float x, float w){ return noise(x)*smoothstep(1.0, 0.5, w); }
```
verbatim: *"smoothly deactivat[es] the cosine wave (zeroing it out) when the size of a 2π cycle is
smaller than a pixel"*, and the octave discipline: *"the propagation of the filter width w needs to
be in synch with the doubling of the frequency."* — https://iquilezles.org/articles/bandlimiting/

`[I]` **This is the mechanism for the grain**: multiply the stipple amplitude by
`smoothstep(1.0, 0.5, footprint / grain_period)`. Below half a pixel the grain fades to its mean and
contributes zero variance — no moiré — while the *mean* it fades to is preserved (which is what
LEAN/Heitz's `σ²` encodes as roughness). **The material's apparent character is not lost; it is
converted from geometry into roughness.** Note this is the same rule the engine's per-cube grain
already uses (`saturate((cubePixels-1.5)/2.5)`), but the albedo mottle does **not** use it.

### 1.6 The direct fix for the close-range blockiness: variable-radius post-process filtering

`[C]` Laine & Karras §4.3, the full algorithm verbatim:

> *"Our method is based on a sparse set of sampling points, stored in a look-up table in ascending
> order according to distance from the center. **We use a set of 96 samples distributed in a disc
> with radius of 24 pixels.** The density of the samples falls as the square root of distance from
> the center, and each sample has an associated weight corresponding to the area of the disc it
> represents… To process a pixel, we start by determining the desired filter radius r based on the
> voxel in the pixel itself. If the radius is one pixel or less, there is no need for filtering, and
> we return the original color. Otherwise, we start processing sampling points in the order
> determined by the look-up table until their distance from the center exceeds r. For each sample,
> color c′ and blur radius r′ of the corresponding pixel are fetched. **To adapt blur radius to the
> neighborhood, we clamp r to min(r, r′). This prevents visible seams from forming by making filter
> radius agree between nearby regions.** Accumulation weight is calculated by taking sample weight
> and adjusting it so that it tapers off to zero linearly between r−1 and r."*

Pseudocode, `[C]`:
```
1: (c,r) <- fetch(x,y)
2: if r <= 1 then return c
3: accum <- (0,0,0,0)
4: for each sample s in kernel do
5:     (c',r') <- fetch(x+s.x, y+s.y)
6:     r <- min(r, r')
7:     if s.dist > r then break
8:     w <- s.weight * min(r - s.dist, 1)
9:     accum.rgb <- accum.rgb + c'*w
```
`[C]` Storage: *"we store the logarithm of the voxel size into the alpha channel of the result image
when casting the rays. **One byte is sufficient** when the value is stored as 3.5 fixed point,
yielding range from 1 (no blur) to about 128 pixels."*
`[C]` Cost: *"post-process filtering is **one to two magnitudes faster than the ray casting**."*
`[C]` The seam problem it solves is illustrated: *"Left: Filtering each pixel with a radius deduced
from the size of the corresponding voxel. **Seams are visible at hierarchy level changes.**"*

`[C]` Their scene scale, directly comparable: *"The resolution is approximately 5mm throughout the
entire building… The total size of the data in GPU memory is 2.7 GB. Our ray caster is able to cast
60.9 million primary rays per second for this data."* Storage efficiency: *"the actual values are
mostly near the theoretical optimum of 5 bytes per voxel."* Hardware: Quadro FX 5800.

`[C]` **And the normal-precision point, directly relevant to this engine's int8×3 normals:** *"the
**8-bit precision provided by such methods is insufficent for smooth highlights and reflections**.
We thus employ a novel compression scheme that provides up to 14 bits of precision for smoothly
varying normals"* — a 4×4 grid of candidates `nb + cu·nu + cv·nv` with `cu, cv ∈ {−1, −⅓, ⅓, 1}`.

`[I]` **This engine stores int8×3 normals. Laine & Karras explicitly measured 8-bit object-space
normals as insufficient for smooth highlights. If the close-range lumpiness survives everything
else, 8-bit normal quantization is a named, documented cause.**

### 1.7 The option that matches the stated aesthetic: biscale NDF — keep the grain as statistics

`[C]` Zirr & Kaplanyan, *Real-time Rendering of Procedural Multiscale Materials*, I3D 2016,
abstract: *"We present a **stable** shading method and a procedural shading model that enables
real-time rendering of **sub-pixel glints and anisotropic microdetails** resulting from irregular
microscopic surface structure to simulate a rich spectrum of appearances ranging from sparkling to
brushed materials. We introduce a **biscale Normal Distribution Function (NDF)** for microdetails to
provide a convenient artistic control over both the global appearance as well as over the appearance
of the individual microdetail shapes, while efficiently generating procedural details."*
— https://research.nvidia.com/publication/2016-02_real-time-rendering-procedural-multiscale-materials

`[C]` The model, §3: *"the position xi of a microscale detail center on a parallel plane is drawn
from a mesoscale distribution of microdetails Dm, then the actual slope is placed around this center
by drawing a value yi from a local normals distribution for a single microdetail Dl… Since the final
slope position is a sum of two random variables xi and yi, their resulting density Dg is a
convolution of two NDFs at different scales as Dg(x) = ∫ Dm(y)Dl(x−y) dy = ⟨Dm ∗ Dl⟩"* (Eq. 3).
Closed form, Eq. 4: *"given the relation between Beckmann roughness α and Gaussian standard
deviation σ being α = √2σ, the global roughness for distribution Dg can be computed as
**α²g = α²m + α²l**"*, where *"αl is the local roughness of a single individual microdetail; and αm
is the roughness of the mesoscale Beckmann distribution Dm of microdetails."*

`[C]` **And the property that is wanted here**, verbatim: *"the resulting density Dg defines the
global appearance of the material **in the limiting case, when individual microdetails disappear at
distance**. This appearance is a desirable parameter of material for user to control, and thus it is
exposed in our model."*

`[C]` They warn off GGX: *"Trowbridge-Reitz distribution does not have a closed form convolution
neither with itself nor with Beckmann distribution. Due to this reason, hereafter, we consider only
Beckmann distributions."*

`[C]` Cost, GeForce GTX 980: *"(a) sparkling fabric on evening dresses (6.6ms/frame); (b) **procedural
terrain with grainy snow on an overcast day (7.8ms/frame)**; (c) brushed aluminum on a car
(7.3ms/frame)."*

`[I]` **"Grainy snow on procedural terrain at 7.8 ms/frame, temporally stable, sub-pixel" is the
closest published result to the stated target.** The mechanism — a mesoscale distribution of
individual grains convolved with a per-grain local NDF, the pair collapsing analytically to
`α²g = α²m + α²l` as the grains go sub-pixel — is exactly the "grain up close, defined material at
distance" behaviour being asked for, and it is not an ad-hoc hack.

The heavier relative, for true glints: Yan, Hašan, Marschner & Ramamoorthi, *Position-Normal
Distributions for Efficient Rendering of Specular Microstructure*, ACM TOG 35(4):56, 2016 — `[C]`
*"treats a specular surface as a four-dimensional position-normal distribution, and fits this
distribution using millions of 4D Gaussians, which are called elements"* — offline-class cost.
— https://dl.acm.org/doi/10.1145/2897824.2925915

### 1.8 The theory underneath all of it

`[C]` Han, Sun, Ramamoorthi & Grinspun, *Frequency Domain Normal Map Filtering*, ACM TOG 26(3):28,
SIGGRAPH 2007: *"normal map filtering can be formalized as a **spherical convolution of the normal
distribution function (NDF) and the BRDF**, for a large class of common BRDFs"* — applicable to
*"Lambertian, microfacet and factored measurements"*, using *"Spherical Harmonics… to filter the NDF
for Lambertian and low-frequency specular BRDFs"* and *"Spherical von Mises-Fisher Distributions… for
high-frequency materials."* GLSL for both variants is on the project page.
— http://www.cs.columbia.edu/cg/normalmap/index.html

`[C]` Crassin 2011 cites this as the justification for the whole approach: *"we model the directional
information with distributions that describe the underlying data. We apply this to normals and to
light directions. This is more accurate than single values, as shown in [HSRG07]."*

---

## 2. Stochastic / dithered / blue-noise grain as a deliberate style

### 2.1 The distinction that decides everything

`[I]` There are **two completely different uses of noise** here, with **opposite** correct answers.
Conflating them is the main way this goes wrong.

| | **Sampling noise** | **Appearance grain** |
|---|---|---|
| Purpose | decorrelate the estimator (breaks the onion rings) | be *visible* as texture |
| Correct anchoring | **screen-space locked**, blue in space *and* time | **world/object-space locked**, constant *screen* size and density |
| Failure if wrong | slow convergence, low-frequency blotches | "shower door effect" — grain slides over the surface |
| Under TAA | must converge to zero | must survive history clamping |

**This engine needs both, and they must be separate systems.** Note that the current code has one of
each and they are anchored correctly: the per-cube grain is world-locked (`Hash3(cell)` on integer
cube coordinates) and the AO rotation is screen-locked (`Hash2(PSIn.Pos.xy)`). The gap is that
neither is *blue*, and the albedo mottle is world-locked but unfiltered.

### 2.2 Sampling noise: spatiotemporal blue noise, screen-locked

`[C]` Wolfe, Morrical, Akenine-Möller & Ramamoorthi, *Spatiotemporal Blue Noise Masks*, EGSR 2022,
abstract: *"Blue noise error patterns are well suited to human perception, and when applied to
stochastic rendering techniques, blue noise masks can minimize unwanted low-frequency noise in the
final image. Current methods of applying different blue noise masks to each rendered frame result in
either white noise frequency spectra temporally, and thus poor convergence and stability, or lower
quality spatially. We propose novel blue noise masks that retain high quality blue noise spatially,
yet when animated produce values at each pixel that are well distributed over time."*
— https://cseweb.ucsd.edu/~ravir/stbn.pdf

`[C]` §5.3 — **this answers the screen-vs-world question for the sampling noise directly:**

> *"The whole purpose of using temporal antialiasing is being able to have a moving, dynamic scene
> which is able to amortize rendering costs by integrating a render over time. When pixels are under
> motion in TAA, pixel history migrates between the pixels but the sampling sequence does not. If
> the desire is to have sampling with specific properties over space, and specific properties over
> time, a moving pixel has to choose whether to preserve the spatial or temporal properties after it
> has moved. **As the goal of our work is to maximize image quality at the lowest of sample counts,
> we opt to preserve spatial properties and not migrate the sampling sequence.**"*

`[C]` Why it beats the golden-ratio trick: *"Golden ratio animated blue noise needs to restart the
sequence periodically to keep from hitting numerical problems, and creates a spike of error and a
sampling discontinuity when that happens. Our noise does not have that problem by being seamless."*
On TAA history rejection: *"Our masks avoid this by being **toroidally progressive**, providing a
seamless, progressive sampling sequence for each pixel starting at any index. This allows for longer
sampling sequences, and thus higher effective sample counts and image quality."* Masks are
*"resolution 64³."*

Predecessors, for context:

- `[C]` Alan Wolfe, *Animating Noise For Integration Over Time*: the golden-ratio constant is
  *"(1+√5)/2 or approximately 1.61803398875"*, *"THE MOST irrational number that there is"*; method:
  *"For each of the noise types, we'll generate a single texture for frame 0, and each subsequent
  frame we will add the golden ratio to each pixel."* The honest limitation: *"they all use white
  noise over time… if you isolate any individual pixel in any of the images and look at it over the
  8 frames, that single pixel will look like white noise."* And: *"white noise beats blue noise in
  the long run (higher sample counts). It's only at these lower sample counts that blue noise is the
  clear winner."* — https://blog.demofox.org/2017/10/31/animating-noise-for-integration-over-time/
- `[C]` Christoph Peters, *Free blue noise textures*: *"All blue noise textures in the database are
  tileable. Resolutions range from 16² to 1024²."* Void-and-cluster: *"a binary mask of already
  placed pixels is blurred with a tileable Gaussian and the next pixel is placed at the darkest point
  in this blurred mask."* Practical recommendation: *"load all 64 of them into a texture array, pick
  one at random in each frame and apply a random offset in each frame."*
  — https://momentsingraphics.de/BlueNoise.html
- **Interleaved Gradient Noise** (Jimenez, *Next Generation Post Processing in Call of Duty: Advanced
  Warfare*, SIGGRAPH 2014). `[C]` Jimenez: *"for shadow mapping a 8-tap filter with a special
  per-pixel noise A.K.A. 'Interleaved Gradient Noise', which together with a spiral-like sampling
  pattern, increases the temporal stability (like dither approaches) while still generating a rich
  number of penumbra steps (like random approaches)."*
  — https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/
  `[C]` The formula, from Wolfe's analysis:
  `fmod(52.9829189 * fmod(0.06711056*x + 0.00583715*y, 1.0), 1.0)`, with the property that every
  3×3 block *"roughly match[es] all values 0/9, 1/9, 2/9, …, 8/9, but that they are a bit
  randomized"*, and the TAA relevance: *"IGN makes the local area more accurately represent the full
  set of possibilities in small neighborhoods of pixels"*, improving *"temporal anti-aliasing's
  history rejection accuracy."* Temporal offset: `5.588238 * frame` (frame mod 64).
  — https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
- `[C]` Heitz & Belcour, *Distributing Monte Carlo Errors as a Blue Noise in Screen Space by
  Permuting Pixel Seeds Between Frames*, CGF (EGSR 2019): *"Monte Carlo noise in raytraced renderings
  typically has a white spectrum because of the randomization used to decorrelate pixel estimates,
  but their temporal algorithm correlates pixel estimates to obtain a noise with a blue spectrum
  like dithered images. This makes the images appear less noisy despite the errors having
  statistically the same amplitudes."* — https://eheitzresearch.wordpress.com/772-2/

`[C]` SVGF benefits too, per Wolfe et al.: *"it has already been shown that denoising methods such as
SVGF benefit from spatial blue noise."*

### 2.3 Appearance grain: world-locked, screen-constant density — and there is a solved recipe

This comes from NPR, not from rendering, and it is the answer to "why does my stipple crawl."

`[C]` **The formal statement of the problem** — Bénard, Bousseau & Thollot, *Dynamic Solid Textures
for Real-Time Coherent Stylization*, I3D 2009:

> *"Achieving these goals without introducing visual artifacts implies the concurrent fulfilment of
> **three constraints**. First, the style marks should have a **constant size and density in the
> image** in order to preserve the 2D appearance of the medium. Second, the style marks should
> **follow the motion of the 3D objects** they depict to avoid the sliding of the style features over
> the 3D scene (**shower door effect**) [Meier 1996]. Finally, a sufficient **temporal continuity**
> between adjacent frames is required to avoid popping and flickering."*

— https://maverick.inria.fr/Publications/2009/BBT09/BBT09.pdf

`[I]` **These three constraints are mutually contradictory. That is why a stipple will crawl no
matter how it is placed — unless you use the fractal-octave trick below.**

`[C]` **The recipe**, §3.1–3.2: *"we define a dynamic solid texture as the weighted sum of n octaves
Ωi of the original solid texture. Each 3D object is then carved in such a solid texture… we
introduce the notion of **zoom cycle** that occurs every time the appearant size of the texture
doubles. In that case, each octave is replaced by the following one and a new high frequency octave
is created… Empirically we observed that **n = 4 octaves is enough to deceive human perception**."*

Coordinates, quoted exactly:
```
(u,v,w)_i = 2^(i-1) (x,y,z) / 2^floor(log2(z_cam))
s = log2(z_cam) - floor(log2(z_cam))  in [0,1]
```
`[C]` *"the 2^(i−1) term scales the sampling rate so that the texture retrieved for one octave is
twice smaller than the texture for the previous octave. The ⌊log₂(z_cam)⌋ term accounts for the
refreshing of the octaves at each zoom cycle."*

`[C]` The three weight constraints: *"the first octave should appear at the beginning of the cycle
while the last should disappear at its end: α1(0)=0 and αn(1)=0"*; *"the weight of the intermediate
octaves at the end of the cycle should be equal to the weight of the following octaves at the
beginning of the next cycle: αi(1)=αi+1(0)"*; *"the weights should sum to 1 to preserve a constant
intensity."* Their concrete choice:
```
a1(s) = s/2 ;  a2(s) = 1/2 - s/6 ;  a3(s) = 1/3 - s/6 ;  a4(s) = 1/6 - s/6
```
`[C]` The known cost: *"the fractalization process introduces new frequencies in the texture, along
with a loss of contrast."*

`[I]` **Directly implementable in this marcher**: sample a 3D grain function at four octaves keyed
off *world* position, with the octave scale locked to `2^floor(log2 t)` and those four weights. It is
world-locked (no shower door), screen-constant in density (no crawl or clumping), and continuous
through zoom (no popping). It is also *procedural in 3D*, which is the natural fit for a voxel field
— no texture parameterization needed.

`[C]` **The other half: nesting, so the grain doesn't swim between LOD levels** — Praun, Hoppe, Webb
& Finkelstein, *Real-Time Hatching*, SIGGRAPH 2001:

> *"one expects the strokes to have roughly uniform screen-space width when representing both near
> and far objects. Thus, when magnifying an object, we would like to see **more strokes appear** (so
> as to maintain constant tone over the enlarged screen-space area of the object), whereas ordinary
> texture mapping simply makes existing strokes larger… We design the mip-map levels such that
> strokes have the same (pixel) width in all levels. Finer levels maintain constant tone by adding
> new strokes to fill the enlarged gaps between strokes inherited from coarser levels."*

And the failure mode of not doing it: *"The art maps constructed in [9] suffered from lack of
coherence, because each mipmap level was constructed independently… The lack of coherence between
the strokes at the different levels create the impression of **'swimming strokes' when approaching
or receding from the surface**."*

The fix: *"Our solution is to impose a **stroke nesting property**: all strokes in a texture image
(ℓ,t) appear in the same place in all the darker images of the same resolution and in all the finer
images of the same tone – i.e. every texture image (ℓ′,t′) where ℓ′ ≥ ℓ and t′ ≥ t (through
transitive closure)… Consequently, when blending between neighboring images in the grid of textures,
only a few pixels differ, leading to minimal blending artifacts."*

`[C]` And explicitly for this case: *"The concept of tonal art maps is quite general and can be used
to represent a variety of aesthetics (e.g. pencil, crayon, **stippling**, and charcoal)."*
— https://hhoppe.com/hatching.pdf

`[I]` **Praun's nesting property is the answer to "why does my stipple pop when the octree level
changes."** If the grain is derived from the voxel data at a given level, level N+1's grain must be
a *superset* of level N's, in the same places. Combined with Bénard's octave weights you get both
nesting and continuous density. Praun states outright that stippling is one of the intended
aesthetics — this 2001 paper solves the exact thing being asked for.

### 2.4 What TAA does to each — and one serious warning

`[C]` Yang, Liu & Salvi, *A Survey of Temporal Antialiasing Techniques*, CGF 39, 2020 — **the
warning**, §6.1.2:

> *"History rectification techniques are based on the assumption that the current frame samples in
> the neighborhood of each pixel contain the entire gamut of surface colors covered by that pixel.
> Since the current frame samples are sparse (≤1 sample per pixel), the hope is that any thin feature
> in geometry or shading is at least covered by one pixel in any 3×3 neighborhood it touches.
> **Unfortunately, with highly detailed content, this assumption is often violated… causing the
> underestimated color bounding box to clip or clamp away the line color from history. This happens
> commonly in highly detailed scenes, where small, sharp features are smoothed out in the output.**"*

— http://behindthepixels.io/assets/files/TemporalAA.pdf

`[I]` **This is the mechanism by which this engine's TAA is actively destroying the fine grain that
is wanted.** A per-pixel stipple is by construction the "sub-pixel thin feature missing from the
input of certain frames" case, and neighbourhood colour clamping will clip it. The options, from the
same survey: variance-based clamping (bias α by the extent), a reactive/exclusion mask for
grain-bearing pixels, or **applying the grain after the resolve**.

`[C]` **Accumulation numbers**, same survey Eq. 2 and §3.3: `fn(p) = α·sn(p) + (1−α)·f_{n−1}(π(p))`,
and: *"with a commonly used α = 0.1, a result from 5 accumulated frames is equivalent to 2.2 samples
per pixel, 10 frames equivalent to 5.1 samples, and 15 frames equivalent to 9.8 samples. **At steady
state with an infinite number of input frames accumulated, α = 0.1 results in 19 effective samples
at its best.**"* The optimal-convergence alternative: *"By setting α = 1/Nt(p), Eq. 2 assigns the
same weight to all history samples. It then enables optimal convergence rate at the cost of an
additional storage channel."* And: *"from a variance reduction perspective, this is suboptimal. The
optimal variance reduction is achieved when all samples are weighted equally in the sum."*

`[C]` **Jitter sequences actually shipped**: *"Unreal Engine 4 uses a 8-sample sequence from
Halton(2, 3) by default, Inside uses a 16-sample sequence from Halton(2, 3), SMAA T2x uses Quincunx,
and Quantum Break uses rotated grid offsets."*

`[C]` **Mip bias**: *"if the effective sample count per-input-pixel is expected to be 4… then the
mipmap bias is calculated as −½log₂4 = −1.0. In practice, a less aggressive bias between this value
and 0 is sometimes preferred to avoid hurting temporal stability as well as texture cache
efficiency."*

`[C]` **Resampling filter**: *"splines like Catmull-Rom are also commonly used as a resampling
filter"* and *"both BFECC and Catmull-Rom offer significant improvement"* over bilinear, which
*"soften[s] the resampled image."*

`[I]` **This engine's `taa_blend = 0.125` is α = 0.125, i.e. ~8 effective samples at steady state**
by the survey's own arithmetic — close to the α = 0.1 / 19-sample figure but noticeably shorter. It
uses a 3×3 neighbourhood clamp, which is exactly the mechanism the warning above describes.

---

## 3. Creator statements — John Lin and other micro-voxel developers

`[U]` **This section is the weakest in the document and the subagent said so.** Reddit was entirely
unreachable (WebFetch refused; the JSON API returns HTML; mirrors 403; the pullpush archive
rate-limited and its index stops around mid-2025, *before* MishMash's 2026 devlogs). **u/MGMishMash's
comment history and the r/VoxelGameDev threads are unread.** John Lin's pinned YouTube comment about
shelving micro-voxels and his X/Twitter are unreadable from that environment; only secondhand HN
reports were available. **No Reddit account for John Lin was found** — `[C]` the r/VoxelGameDev
attention his work received in Nov 2020 came via a BlueDrake42 repost, per PC Gamer
(https://www.pcgamer.com/john-lins-beautiful-physics-sandbox-gives-me-minecraft-vibes/), *"so do not
assume he posted there."*

**What this repo already has on the question is better than what this pass could add.**
`research/micro-voxel-creators-research.md` (written for Prompt 001 by the previous side session,
with CONFIRMED/INFERENCE labels) established from Lin's own feature-description text that **his
actual technique is a real-time GPU path tracer with 5-bounce global illumination.** That is the
governing fact: *"make it look like Lin"* cannot mean *"copy Lin's method"* at this engine's budget,
and `docs/progress.md` already records the deliberate decision to use *"cheap, real techniques …
chosen to evoke the feeling of that aesthetic"* instead.

Two loose data points that did survive, both `[C]` as quoted but weakly sourced:
- **Voxel Quest** (developer statement): *"Some shots of the new view distance (16 kilometers or more
  if desired)"*, and *"only 1km view range"* at LOD 1/4.
- **An anonymous micro-voxel dev**: *"3.125cm per voxel right now at 20 fps with 100km view
  distance."*

**Follow-up for a future pass**: the Reddit route needs an environment where it is reachable. The
previous side session's note stands and is the practical method — *"Reddit crossposts (e.g. MishMash
posts everything as u/MGMishMash) and redlib mirrors give full devlog text"*, and video descriptions
come through Exa's index verbatim.

---

## 4. Is one ray per pixel even the right operation?

### 4.1 The point-cloud answer: average everything that lands in the pixel

`[C]` Schütz, Kerbl & Wimmer, *Software Rasterization of 2 Billion Points in Real Time*, PACMCGIT
5(3), 2022, §3.6: *"LOD rendering works well in conjunction with high-quality splatting
(point-sprites or surfels) or shading (one-pixel points), a form of **color filtering for point
clouds that blends overlapping points together. Colors and amount of points inside a pixel within a
certain depth range (e.g., 1% behind the closest point) are summed up during the geometry processing
stage, and a post-processing shader then divides the sum of colors by the counters to compute the
average.**"* Implementation cost: *"one that uses two 64-bit atomicAdd instructions per point into
four 32 bit integers to sum up color values and counters, and another variation that uses a single
64 bit atomic instruction per point to compute the sum of up to 255 points"* … *"when using an LOD
structure, the amount of overlapping points with similar depth is essentially guaranteed to be lower
than 255, so we can safely use high-quality shading with just a single 64-bit atomic add instruction
per point. This limit can even be raised to up to 1023 points by using the 'non-robust' variation
without overflow protection."* — https://arxiv.org/pdf/2204.01287

`[I]` **An alternative to cone-marching for this engine**: march one ray, then accumulate *every*
surface voxel within the ray-cone footprint and a small depth window, and divide. That is a
per-pixel weighted average of the actual sub-pixel primitives — **the ground truth a cone-mip is
approximating**, and therefore the reference to compare against when the prefilter looks wrong.

### 4.2 The splatting answer: band-limit the primitive, then box-filter the pixel

`[C]` Yu, Chen, Huang, Sattler & Geiger, *Mip-Splatting: Alias-free 3D Gaussian Splatting*, CVPR
2024, abstract: *"Strong artifacts can be observed when changing the sampling rate by changing focal
length or camera distance, which can be attributed to the lack of 3D frequency constraints and the
usage of a 2D dilation filter."* — https://arxiv.org/pdf/2311.16493

`[C]` The sampling interval, Eq. 6: *"For an image with focal length f in pixel units, the sampling
interval in screen space is 1. When this pixel interval is back-projected to the 3D world space, it
results in a world space sampling interval T̂ at a given depth d, with sampling frequency ν̂ as its
inverse: **T̂ = 1/ν̂ = d/f**."* And the Nyquist consequence: *"given samples drawn at frequency ν̂,
reconstruction algorithms are able to reconstruct components of the signal with frequencies up to
ν̂/2, or f/2d. Consequently, **a primitive smaller than 2T̂ may result in aliasing artifacts** during
the splatting process, since its size is below twice the sampling interval."*

`[C]` The two filters, both convolutions of Gaussians (Eqs. 9, 10): the 3D smoothing filter adds
`(s/ν̂k)·I` to the primitive's covariance; the 2D Mip filter adds `sI` to the projected 2D
covariance, where *"s is chosen to cover a single pixel in screen space"*. Their distinction from
classic EWA splatting: *"Our Mip filter is designed to replicate the box filter in the imaging
process, targeting an exact approximation of a single pixel. Conversely, the EWA filter's role is to
limit the frequency signal's bandwidth, and the size of the filter is chosen empirically. **The EWA
paper even advocates for an identity covariance matrix, effectively occupying a 3x3 pixel region on
the screen. However, this approach leads to overly [smooth results].**"*

`[I]` **The recommended reconstruction filter, converging across all three literatures:** a
**one-pixel-wide box, approximated by a Gaussian** for the pixel integral (Mip-Splatting's 2D Mip
filter; the TAA survey's Gaussian/tent kernel), **plus** a band-limit on the primitive itself so
nothing narrower than 2× the sampling interval survives (Mip-Splatting's 3D filter; Quílez's
`smoothstep`; Norton's clamping; the octree level cutoff in ESVO/GigaVoxels). **A 3×3 support is
explicitly reported as over-smooth.** For history length: α = 0.1 (≈19 effective samples) for
maximum smoothing, α = 0.2 (SVGF) for responsiveness, or α = 1/N with a per-pixel counter for
optimal convergence.

### 4.3 The cheap first move: break the correlation

`[C]` Ruijters (§0): a *"random offset at the beginning of each ray"* breaks the ring pattern into a
dither.
`[C]` Steve Demlow on the Khronos forums, on a GPU raycaster with exactly this symptom: *"A simpler
approach is to break up the regular sampling that leads to Moire patterns. Instead of just using the
first hit on the bounding box as the first sample point, jitter it by some random fraction of the
sampling distance along the ray… That should result in a noisier image but at much lower
frequencies."* — https://community.khronos.org/t/aliasing-effects-in-gpu-based-raycaster/46019
`[C]` And the cone prescription, same forums: *"you need to considerer the volumic shape of this
pixel, and compute the 'average' of the volume intersection (with your voxel grid). With this kind
of consideration, you're transform your current … pixel 'point-sample' to a 'area-sample'
(frustum -> quad, (truncated) cone -> elipsoid, …)."*
— https://community.khronos.org/t/3d-rendering-strange-curved-lines/70436

---

## 5. How far a human can actually see terrain — the numbers, with sources

### (a) Horizon distance

`[C]` Andrew T. Young (San Diego State University), *Distance to the Horizon*:
- geometric: *"OG = sqrt ( 2 R h )"*
- *"the distance to the geometric horizon **3.57 km times the square root of the height of the eye in
  meters**"*; English: *"about 1.23 miles times the square root of the eye height in feet"*
- refraction, with a refraction constant of 1/7 giving *"R′ = R × 7/6"* ≈ 7440 km: *"the distance to
  the horizon in kilometers is about **3.86 km times the square root of the height in meters**"*;
  English *"about 1.32 miles times the square root of the height in feet"*
- classical rule: *"7 times the height in feet is 4 times the square of the distance to the horizon
  in miles"*
- *"refraction lets us see a little farther, if the ray is concave toward the Earth"*

— https://aty.sdsu.edu/explain/atmos_refr/horizon.html

`[I]` Computed from `d = sqrt(2Rh + h²)`, R = 6,371,000 m, alongside both coefficients:

| eye height h | exact `sqrt(2Rh+h²)` | 3.57·√h | 3.86·√h (refracted) |
|---|---|---|---|
| **1.7 m (this engine's eye height)** | **4.654 km** | 4.655 km | **5.033 km** |
| 2 m | 5.048 km | 5.049 km | 5.459 km |
| 10 m | 11.288 km | 11.289 km | 12.206 km |
| 100 m | 35.696 km | 35.700 km | 38.600 km |
| 1000 m | 112.885 km | 112.893 km | 122.064 km |

(The `h²` term is negligible below ~1 km; the 3.57 coefficient is exact to 3 decimals over this
range.)

### (b) Angular resolution — and here the literature genuinely disagrees

`[C]` **The retinal sampling limit** — Williams, *The Photoreceptor Mosaic as an Image Sampling
Device*, in *Advances in Photoreception* (NCBI NBK235550): *"Curcio and her colleagues examined
several eyes and found **foveal Nyquist limits ranging from 50 cycles all the way up to 85**"* …
*"their average value is 65 cycles, so we can still say that **the Nyquist limit of the foveal cones
is on the order of 60 cycles/degree**."* Also: *"Osterberg's (1935) classic measurements of cone
density in a single human eye implied a Nyquist limit for the foveal center of almost exactly 60
cycles/degree"* and *"the spatial bandwidth of the retinal image is on the order of 60
cycles/degree"* — i.e. optics and sampling are matched.
— https://www.ncbi.nlm.nih.gov/books/NBK235550/

`[C]` The source that established the split: Campbell & Green, *Optical and retinal factors affecting
visual resolution*, J. Physiol. 181:576–593, 1965 —
https://physoc.onlinelibrary.wiley.com/doi/10.1113/jphysiol.1965.sp007784. `[C]` Campbell's own
retrospective: *"It turned out that the optics of the eye were well matched to the transmission
properties of the retina. At that time most textbooks maintained that the resolving power of the eye
was solely due to the optics."* — https://garfield.library.upenn.edu/classics1990/A1990EE41800001.pdf

`[C]` **The task dependence, spanning more than an order of magnitude** — University of Arizona
Visual Optics Lab teaching notes:
- *"Point Acuity – 'Binary Star' test – typically 1 arcmin resolution"*
- *"Grating Acuity – … Typically 2 arcmin."* → `[I]` 2 arcmin period = **30 cycles/degree**, i.e.
  *half* the cone Nyquist limit
- *"Letter Acuity – … Typically 5 arcmin."*
- *"Vernier Acuity – Two lines slightly offset from each other. … typically **10 seconds of arc**"*
  → `[I]` 6× finer than 1 arcmin
- *"Stereo Acuity … Typically - 5 seconds of arc"*
- *"Visual Acuity Charts are designed so the 20/20 line subtends 5 arcmin."*

— https://wp.optics.arizona.edu/visualopticslab/wp-content/uploads/sites/52/2016/08/Class04_08.pdf

`[C]` **A fourth figure, from graphics** — Guenter, Finch, Drucker, Tan & Snyder, *Foveated 3D
Graphics* (SIGGRAPH Asia 2012): *"**A figure of 48 cycles per degree (20/12.5) is a good estimate of
average foveal acuity for adults below 50 years of age** [Colenbrander 2001]. We therefore choose the
representative value ω₀ = 1/48°."*
— https://www.microsoft.com/en-us/research/wp-content/uploads/2012/11/foveated_final15.pdf

`[C]` **And a fifth, which matters more than any of the above for a render-distance budget** —
Patney, Salvi, Kim, Kaplanyan, Wyman, Benty, Luebke & Lefohn, *Towards Foveated Rendering for
Gaze-Tracked Virtual Reality* (SIGGRAPH Asia 2016) §2.2: *"After correcting for refraction, the
**detection acuity at 30° eccentricity is 30 cycles per degree, compared to 5 cycles per degree for
resolution acuity** [Thibos et al. 1996]. Thus, **detection rather than resolution should serve as a
conservative acuity estimate** for foveated rendering. Foveated rendering targeting resolution acuity
will eliminate frequencies in the aliasing zone, which can cause apparent loss of contrast in
peripheral vision."* — http://cwyman.org/papers/siga16_gazeTrackedFoveatedRendering.pdf

`[I]` **Reported as a disagreement rather than resolved: the "human resolution limit" is 30 cpd
(gratings), 48 cpd (clinical foveal acuity), 60 cpd (cone Nyquist, average 65, range 50–85), or
effectively unbounded (vernier hyperacuity at ~10 arcsec). For a render-distance budget the relevant
number is the one for *detecting that structure exists*, not resolving it — and Patney's 6×
detection-vs-resolution gap says any budget derived from a resolution-acuity curve is too
aggressive.**

### (c) Atmospheric visibility

`[C]` Koschmieder's law in standard form: `x_V = 3.912 / b_ext`, with the constant's origin:
*"Lab experiments have determined that contrast ratios between 0.018 and 0.03 are perceptible under
typical daylight viewing conditions. Usually, a contrast ratio of 2% (C_V = 0.02) is used to
calculate visual range."* — https://en.wikipedia.org/wiki/Visibility (secondary; the constant is
`ln(1/0.02) = 3.912`, and the 0.05-threshold variant gives `ln(1/0.05) = 2.996 ≈ 3.0`).

`[C]` The Rayleigh ceiling: *"in the cleanest possible atmosphere, visibility is limited to about
**296 km**"*, from a Rayleigh extinction coefficient of *"approximately 13.2 × 10⁻⁶ m⁻¹ at 520 nm
wavelength."* `[I]` Check: 3.912 / 13.2e-6 = 296,364 m ✓.

`[C]` The definition of the standard: MOR is *"the greatest distance at which a black object of
suitable dimensions, situated near the ground, can be seen and recognized when observed against a
bright background."* The WMO CIMO Guide (WMO-No. 8) is the normative reference —
https://community.wmo.int/site/knowledge-hub/programmes-and-initiatives/weather-radar-observations/cimo-guide.
`[U]` **The CIMO Guide's visibility chapter was not read directly**; the constant is cited from a
secondary source and verified arithmetically.

`[I]` Derived table, `V = 3.912/β`:

| β (1/m) | V |
|---|---|
| 1.32×10⁻⁵ (Rayleigh, cleanest) | 296 km |
| 1.0×10⁻⁴ | 39.1 km |
| 5.0×10⁻⁴ | 7.8 km |
| 1.0×10⁻³ | 3.9 km |
| 1.0×10⁻² (haze/light fog) | 0.39 km |
| 1.0×10⁻¹ (dense fog) | 0.04 km |

`[I]` **On a clear day, atmospheric extinction is NOT the binding constraint at ground level — the
4.65 km geometric horizon at 1.7 m eye height arrives first. Atmosphere binds only from elevation.
Beyond ~40 km of visibility you are modelling an unusually clean atmosphere.**

### (d) View distance in shipped games

`[C]` **The best-sourced number, and it is a developer statement** — Mattias Widmark (DICE),
*Terrain in Battlefield 3: A Modern, Complete and Scalable System*, GDC, slide "Scalability":
*"Our definition of scalability — **Arbitrary view distance (0.06m to 30 000m)** — Arbitrary level of
detail (0.0001m and lower) — Arbitrary velocity (supercars and jets)"*, with the design conclusion
*"**It is all about hierarchies!** Consistent use of hierarchies gives scalability 'for free'"*. Also
their virtual texture scale: *"Very large, can easily reach 1M x 1M (= 1Tpixel)!"* and the LOD-blend
mechanism: *"CLOD fade factor — Used to smoothly fade in a newly composited tile (fade-to tile) —
Previous LOD (fade-from tile) is already in atlas and fetched using indirection mips — CLOD factor
updated each frame"*. — https://www.gamedevs.org/uploads/battlefield-3-terrain.pdf

**Minecraft** (secondary — community wiki): render distance 2–32 chunks in Java Edition; 8 chunks =
128 blocks, 16 = 256, **32 chunks = 512 blocks**, max *"32 chunks if at least 1GB of memory is
allocated to the game, and 16 chunks otherwise."* — https://minecraft.wiki/w/Options

`[U]` **No hard developer-stated km figures were obtained** for Microsoft Flight Simulator, Red Dead
Redemption 2, Horizon Zero Dawn/Forbidden West, Ghost of Tsushima, or Death Stranding. MSFS exposes a
*"Terrain Level of Detail"* slider (Ultra = 200, High-End = 100) rather than a distance; that is a
settings value, not a distance, and was not converted.

### (e) The derived table — the section that should change the design

All `[I]`, arithmetic shown. 1 arcmin = π/(180·60) = 2.908882×10⁻⁴ rad. θ ≈ s/d, so `d = s/θ`.

**When does a feature drop below human acuity?**

| feature size | subtends 1 arcmin at | subtends 0.5 arcmin at |
|---|---|---|
| **7.8 mm (this engine's finest voxel)** | **26.81 m** | 53.63 m |
| 1 cm | 34.38 m | 68.75 m |
| 10 cm | 343.77 m | 687.55 m |
| 1 m | 3437.75 m | 6875.49 m |

**When does a feature drop below one *pixel*?** Using Ray Tracing Gems Eq. 30,
`α = arctan(2·tan(ψ/2)/H)`:

| hFOV | resolution | vFOV | α (rad) | α (arcmin) | px/deg | 7.8 mm = 1 px at |
|---|---|---|---|---|---|---|
| 60° | 1920×1080 | 35.98° | 6.014×10⁻⁴ | 2.067 | 29.0 | **12.97 m** |
| 60° | 2560×1440 | 35.98° | 4.511×10⁻⁴ | 1.551 | 38.7 | 17.29 m |
| 60° | 3840×2160 | 35.98° | 3.007×10⁻⁴ | 1.034 | 58.0 | 25.94 m |
| 90° | 1920×1080 | 58.72° | 1.042×10⁻³ | 3.581 | 16.8 | 7.49 m |
| 103° | 1920×1080 | 70.53° | 1.310×10⁻³ | 4.502 | 13.3 | 5.96 m |

**And the number that explains everything** — how many 7.8 mm voxels project into one pixel
(60° hFOV, 1920×1080, α = 6.014×10⁻⁴):

| distance | footprint width | voxels across | **voxels per pixel area** |
|---|---|---|---|
| 10 m | 6.01 mm | 0.8 | 0.6 |
| **13 m** | 7.82 mm | **1.0** | **1.0** |
| 25 m | 15.0 mm | 1.9 | 3.7 |
| 50 m | 30.1 mm | 3.9 | 14.9 |
| 100 m | 60.1 mm | 7.7 | 59.4 |
| 250 m | 150 mm | 19.3 | 372 |
| 500 m | 301 mm | 38.6 | 1,486 |
| 1000 m | 601 mm | 77.1 | **5,945** |
| 2000 m | 1.20 m | 154 | 23,780 |
| 4650 m (horizon @ 1.7 m eye) | 2.80 m | 358 | **128,544** |

> `[I]` **The three conclusions that matter:**
>
> 1. **Beyond ~13 m at 1080p/60°, *every* voxel in this world is sub-pixel.** From 13 m to the
>    4.65 km ground-level horizon is a factor of 358 in distance — **more than 99.7 % of the visible
>    depth range is in the sub-pixel regime.** Sub-pixel is not an edge case in this engine; it is
>    the normal case, and the near field is the exception.
> 2. **At 1 km one primary ray is being asked to estimate the average of ~5,900 surface voxels; at
>    the horizon, ~128,000.** No amount of jitter or TAA fixes a 1-sample estimate of a
>    128,000-element average. **This is a prefiltering problem and only prefiltering solves it.**
>    Heitz & Neyret's *"the timings per pixel are scale-independent"* is the missing property.
> 3. **These voxels are finer than human acuity from 27 m out, and finer than a 1080p pixel from
>    13 m out.** There is *nothing to gain* from resolving individual voxels past ~27 m even with a
>    perfect display. What is wanted past that distance is the correct *statistics* (mean colour,
>    mean normal, variance/roughness, coverage) — precisely what §1.3's Options A–C store.
>    **The grain wanted in the near field and the smoothness wanted in the far field are the same
>    representation evaluated at two footprints.**

---

## 6. Fixed (non-eye-tracked) foveated rendering on a monitor

### 6.1 Guenter et al. 2012 — the model and the numbers

`[C]` The eccentricity model, §3 Eq. 1: *"Given a fixed contrast ratio supported by a particular
display, we therefore assume the linear model: **ω = m e + ω₀** (1) where ω is the MAR in degrees per
cycle, e is the eccentricity angle, ω₀ is smallest resolvable angle of visual acuity at the fovea
(e = 0), and m is the MAR slope."*

`[C]` The slopes, §5: *"j_A = 12 ⇒ m_A = 0.0275 = 1.65′ per eccentricity °"* and *"j_B = 15 ⇒
m_B = 0.0220 = 1.32′ per eccentricity °"*. Abstract: *"we obtain a slope value for the model of
**1.32-1.65 arc minutes per degree of eccentricity**."*

`[C]` **A genuine internal contradiction in the paper, reported rather than resolved:** the abstract
and §5 give the validated slope range as 0.0220–0.0275, but the Conclusion says *"Our user studies
establish a specific range of MAR slopes (0.022-0.034)"*. **The 0.034 upper bound appears nowhere in
§5.** `[I]` It likely comes from the supplement's informal study. **Use 0.022 (= 1.32 arcmin/deg) as
the safe slope — it is the pair-test value, where quality was judged equivalent to non-foveated.**

`[C]` Layers: *"Our current system uses three layers"*, with *"the inner layer at 120Hz to combat
this, and all other layers at 60 Hz. We stagger the update of the middle and outer regions, updating
one on even frames and the other on odd."* Layer sizes are solved by a brute-force optimizer
(Eq. 2: `s_{i+1} = (m·e_i + ω₀)/ω*`). The outer layer is special: *"The outermost layer is not a
square layer centered about the gaze point like the other layers: it subsamples the entire screen
image and preserves the display's aspect ratio."*

`[C]` Their display: LG W2363D, 1920×1080, 120 Hz, *"V* = 59cm, W* = 51cm, D* = 1920, and
α* = 9/16 = 0.5625. This yields an angular display radius of e* = 23.4° (Eq. 6), and an angular
display sharpness of ω* = 0.0516° (Eq. 7), which represents only a fraction of human foveal acuity,
**ω₀/ω* ≈ 40%**."*

`[C]` **Measured savings:**

| | Quality A (m=0.0275) | Quality B (m=0.0220) | "informal" |
|---|---|---|---|
| pixels rendered vs non-foveated | **13.3×** | **9.8×** | ~15× |
| **overall measured speedup** | **5.7×** | **4.8×** | 6.2× |

verbatim: *"The speedup factor in terms of pixels rendered compared to non-foveated was 13.3 for
quality level A and 9.8 for quality level B. Overall measured speedup over non-foveated rendering
was a factor of 5.7 at quality level A and 4.8 at level B."*

`[C]` The FOV dependence, which is the crux for a monitor: *"The 5° foveal region fills a mere 0.8%
of the solid angle of a 60° display"*, and on scaling: *"Our findings suggest much greater savings at
higher display resolution and field of view"*, predicting *"100 times speedup at a field of view of
70° and resolution matching foveal acuity."*

`[C]` **And, directly relevant to a ray marcher**, from their conclusion: *"**Graphics architectures
based on ray tracing may efficiently permit a more continuous falloff of sampling density with
eccentricity** [Murphy and Duchowski 2007]."*

`[C]` Latency, which is why pinning the fovea to a crosshair is not equivalent: Tobii TX300 at
*"300 Hz update, <10 ms latency"*, monitor *"6 ms measured pixel switching"*, best-case end-to-end
**23 ms**, worst-case **40 ms**. An earlier 60 Hz / 35 ms version gave *"an obvious and unacceptable
'pop'"*. 15 subjects; MSAA 4× plus temporal reverse reprojection plus *"per-frame ±0.5-pixel jitter
of each layer's sampling grid."*

### 6.2 Patney et al. 2016 — contrast preservation

`[C]` Abstract: *"We determined that **filtering peripheral regions reduces contrast, inducing a
sense of tunnel vision**. When applying a postprocess contrast enhancement, subjects tolerated up to
**2× larger blur radius** before detecting differences from a non-foveated ground truth."* Target:
*"a contrast-preserving Gaussian filter with a progressive standard deviation of **1 arcmin per
peripheral degree of eccentricity**, or even larger, is barely distinguishable from non-foveated
rendering."*

`[C]` Measured thresholds, §3.2: *"For the HMD setup, threshold for contrast-preserving foveation was
approx. 2× better than temporally stable and approx. 3× better than aliased foveation. For the
desktop setup, threshold for contrast-preserving foveation was approx. **2.3× better** than foveation
without contrast preservation."*

`[C]` Naive blur is detectable: *"Our studies show they often exhibit significant head- and
gaze-dependent temporal aliasing, distracting users and breaking immersion."*

`[C]` How much bigger the sharp region must be than acuity models predict, §4.4.1: *"The estimated
threshold size of the transition region for OUR rendering system is 1.8–3.5× lower than that for a
MULTIRES foveated renderer."* Fig. 9 shows Guenter-style thresholds of ~39–65° of transition-region
size, and *"In our study we set the maximum threshold in our user study to 60°, which can be seen to
be hit for many users."* With the practical caveat: *"the thresholds obtained from both our user
studies are generally conservative… In practice, we find that we can often reduce the transition
region to about 5° before artifacts become prominent."*

`[C]` Measured savings, Table 1 (Oculus DK2, 1184×1464/eye, 300-frame gaze traces), shaded quads as
% of non-foveated: Sponza Guenter 48.4 % vs theirs **26.6 %**; Classroom 49.9 % vs **31.8 %**; San
Miguel 50.7 % vs **38.1 %**. Abstract: *"reduces number of shades by up to **70 %** and allows
coarsened shading up to **30° closer to the fovea** than Guenter et al. [2012]."*

`[C]` The honesty note: *"We designed our system to match a perceptually-validated target rather than
optimizing for highest performance on current hardware. This means our quality is demonstrably
better, but **hardware changes are necessary to realize the performance gains described in our
paper**."*

`[C]` Temporal stability: *"a new temporal antialiasing algorithm that supports multi-resolution
renderings and avoids gaze-dependent blurring artifacts caused by eye saccades. This provides a
**10× reduction in temporal instability**"*, costing *"roughly 1.5ms on an NVIDIA GTX Titan X GPU."*
And: *"Consistent motion and temporal sensitivity requires foveated renderers avoid artifacts like
temporal aliasing, as these are easily perceptible even in the periphery."*

`[C]` Their desktop leg, closest to this setup: *"a 27 inch LCD monitor with 2560×1440 pixels (Acer
XB270HU), with the subject seated at a distance of 81 cm (chosen to approximate 1 pixel/arcminute of
the display)… **We set the central foveal radius for this setup to 7.5°.**"* (HMD: 110° FOV, foveal
radius 15°, *"which is larger than desktop estimates due to the low angular resolution of the
HMD"*.) Only **4 participants**, *"aged 26 to 46, with 20/20 corrected vision."*

### 6.3 Does any shipping non-VR game do crosshair-centred fixed foveation?

**No evidence of any. Every shipped flat-screen VRS implementation that could be verified is
*content*-driven (luminance/edge/motion), not *screen-position*-driven.**

| Title / tech | Ships flat-screen? | Basis | Source |
|---|---|---|---|
| Wolfenstein: Youngblood (NVIDIA Adaptive Shading) | Yes | **content + motion**; no mention of gaze or centre-based operation. `[C]` NAS *"combines two forms of VRS into one content aware option"* | https://www.nvidia.com/en-us/geforce/news/nvidia-adaptive-shading-a-deep-dive |
| Gears Tactics (Tier 1), Gears 5 + Tactics (Tier 2) | Yes | **content**. `[C]` they run *"a sobel edge detection compute shader on our final scene color buffer"* and analyse *"perceptual difference of colors"* | https://devblogs.microsoft.com/directx/gears-tactics-vrs/ , https://devblogs.microsoft.com/directx/gears-vrs-tier2/ |
| DIRT 5 (AMD FidelityFX Variable Shading) | Yes | **content**. `[C]` *"analyzing luminance variance in the previous frame and uses motion vectors to generate a shading rate image"* | https://gpuopen.com/fidelityfx-variable-shading/ |
| Unreal Engine 5 | Yes | **two separate paths**: `[C]` `r.VRS.ContrastAdaptiveShading` is content-based (*"based on the luminance from the previous frame's post-process output"*, default **0 = off**); the *centre-based radial* path is documented as the **XR** Fixed Foveated Rendering feature — *"reducing the shading rate towards the periphery of the view"* — under **XR Performance Features**, not general rendering | https://dev.epicgames.com/documentation/en-us/unreal-engine/xr-performance-features-in-unreal-engine |
| Cyberpunk 2077, Call of Duty, Watch Dogs Legion | — | `[U]` **no primary confirmation of VRS at all** was found | — |
| iRacing / MSFS 2024 / DCS "Foveated Rendering" | Yes, but **VR-only path** | closest thing to a user-facing foveation setting, gated to VR | https://www.uploadvr.com/microsoft-flight-simulator-2024-now-has-foveated-rendering/ |

`[I]` The pattern is consistent and probably causal: on a flat screen you get no guarantee about
where the eye is, but you *do* get a cheap per-frame signal about where detail is.
`[U]` **No primary documentation was found for any per-region "quality by screen region" mode in
DLSS or FSR** — DRS/DLSS scale the whole frame.

**VR does ship it.** `[C]` Meta Quest FFR docs: *"FFR enables the edges of an application-generated
frame to be rendered at a lower resolution than the center portion of the frame"*, levels Off / Low /
Medium / High / High Top, measured savings **6.5 % (Low), 11.5 % (Medium), 21 % (High)** in their
pixel-intensive test app, and *"can result in a 25% gain in performance with pixel-intensive
applications."* Caveats: *"FFR may not improve performance in applications with simple shaders"*;
*"High Top is not available when using the OpenXR interface."*
— https://developers.meta.com/horizon/documentation/unity/os-fixed-foveated-rendering/
NVIDIA VRSS 1 is fixed centre-based supersampling, VR-only, driver-side, per-game allow-listed;
VRSS 2 adds gaze tracking —
https://developer.nvidia.com/blog/nvidia-vrss-2-dynamic-foveated-rendering-no-assembly-required

`[I]` Why VR gets it free and a monitor does not: HMD lenses already degrade peripheral quality, and
head-aiming keeps gaze near lens centre. Neither holds on a monitor.

### 6.4 VRS APIs and hardware

**D3D12** — all `[C]` from https://learn.microsoft.com/en-us/windows/win32/direct3d12/vrs :
- **Tier 1**: *"Shading rate can be specified only on a per-draw-basis; no more granular than that."*
- **Tier 2**: *"…can also be specified by a combination of per-draw basis, and of: Semantic from each
  provoking vertex, and a screen-space image… **Shading rate requested by your application is
  guaranteed to be delivered exactly** (for precision of temporal and other reconstruction filters).
  SV_ShadingRate PS input is supported."*
- Rates: *"The shading rates 1x1, 1x2, 2x1, and 2x2 are supported on all tiers. There is a
  capability, AdditionalShadingRatesSupported, to indicate whether 2x4, 4x2, and 4x4 are
  supported."*
- `[C]` **A contradiction inside the same Microsoft page:** the Tier 2 bullet says *"Screen-space
  image tile size is 16x16 or smaller"*, while the "Tile size" section says *"Tiles are square… the
  tile size is one of these values. 8 / 16 / 32."* `[I]` **Always query
  `D3D12_FEATURE_DATA_D3D12_OPTIONS6::ShadingRateImageTileSize`** (0 if unsupported). Image dims
  `{ceil(W/tile), ceil(H/tile)}`, format `DXGI_FORMAT_R8_UINT`, state
  `D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE`, set via `RSSetShadingRateImage`.
- Combiners: *"Passthrough. C.xy = A.xy. / Override. C.xy = B.xy. / Higher quality. C.xy =
  min(A.xy, B.xy). / Lower quality. C.xy = max(A.xy, B.xy). / Apply cost B relative to A. C.xy =
  min(maxRate, A.xy + B.xy)."*
- **Gotchas that would bite a marcher**: *"When coarse pixel shading is used, depth and stencil and
  coverage are always computed and emitted at the full sample resolution"*; *"when 2x2 coarse pixels
  are used, a gradient will be twice the size… Usage of coarse pixel shading causes lesser-detailed
  mips to be selected"*; *"SV_Position is always interpolated at the center of the coarse pixel
  region"*; a PS *"fails compilation if it inputs SV_ShadingRate and also uses sample-based
  execution"*; and on **Tier 1** only, `SV_Coverage` as in/out, a non-full `SampleMask`, or
  pull-model intrinsics each **silently disable** coarse pixel shading.

`[C]` Measured, DirectX blog: Firaxis with Tier 1 saw *~20 % FPS increase*; with the Tier 2
screenspace image, *14 % FPS increase*. Hardware statement (2019): *"VRS support exists today on
in-market NVIDIA hardware and on upcoming Intel hardware."*
— https://devblogs.microsoft.com/directx/variable-rate-shading-a-scalpel-in-a-world-of-sledgehammers/

**Vulkan** — `[C]` `VK_KHR_fragment_shading_rate`, *"extension number 227, revision 2, ratified, last
modified 2021-09-30"*, three mechanisms: pipeline (per-draw), primitive (per-primitive), attachment
(*"per-region of the framebuffer, specified in a specialized image attachment"*).
— https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_fragment_shading_rate.html
`[C]` Combiners **differ from D3D12's** — `KEEP`=A, `REPLACE`=B, `MIN`, `MAX`, and **`MUL`**=A*B;
attachment lookup `x' = floor(x/region_x)`, max fragment size ≤ 4 per dimension, minimum mandatory
support *"2x2 and 2x1 fragments at sample counts 1 and 4, plus 1x1 at all sample counts"*; query
`min/maxFragmentShadingRateAttachmentTexelSize`.
— https://docs.vulkan.org/spec/latest/chapters/primsrast.html

**Hardware:**
- `[C]` NVIDIA Turing+: *"Coarse shading: 1×1, 1×2, 2×1, 2×2, 2×4, 4×2, 4×4"* plus *"Supersampling:
  2x, 4x, 8x"*; *"Every 16×16 tile of pixels maps to a shading rate entry in the shading rate
  surface"*; needs *"driver revision R410 and later"*. Names gaze-tracked foveation as use case #1
  but gives **no measured percentages**: gains *"depend on pixel shader complexity and type of
  content."* — https://developer.nvidia.com/blog/turing-variable-rate-shading-vrworks/
- `[C]` AMD RDNA2+, via Microsoft: *"Available on all hardware supporting DirectX 12 Ultimate…"*
  Measured on a **6900 XT at 4K**: **8 % / 10 % / 12 %** at Quality/Balanced/Performance; with SSGI
  on at 4K "Insane": **14 % / 15 % / 20 %**. — https://devblogs.microsoft.com/directx/gears-vrs-tier2/
- `[U]` **Intel: only partially confirmed.** Intel's own article says only *"Hardware support for VRS
  is still developing, but it will receive a boost later in 2019 with Intel's new Gen11 graphics
  hardware"*, with a *"1.2x Speedup"* banner. **It states no tier and never mentions Xe/Gen12.** The
  commonly repeated "Gen11 = Tier 1, Xe = Tier 2" split is **NOT confirmed by any Intel primary
  source.** — https://www.intel.com/content/www/us/en/developer/articles/technical/intel-and-microsoft-unveil-variable-rate-shading-support.html
- `[C]` 3DMark VRS feature test: methodology only (two passes, VRS off then on); **UL publishes no
  headline % gain.** — https://support.benchmarks.ul.com/support/solutions/articles/44002136963

### 6.5 The desktop reality check — the number that should decide it

**Tursun, Arabadzhiyska-Koleva, Wernikowski, Mantiuk, Seidel, Myszkowski & Didyk,
*Luminance-Contrast-Aware Foveated Rendering*, SIGGRAPH 2019 (ACM TOG 38(4):98)** — the best-matched
published measurement to this setup.

`[C]` Their displays: *"a 27" ASUS PG278Q display with 2560 × 1440 resolution spanning a visual field
of 48.3° × 28.3° from a viewing distance of 66.5 cm. The second display is a 32" Dell UP3216Q with
3840 × 2160 resolution spanning a visual field of 52.3° × 30.9° from a viewing distance of 71 cm.
The peak luminances of the displays are measured as 214.6 cd/m² and 199.2 cd/m² whereas the peak
resolutions produced at the center are 24.9 cpd and 34.1 cpd."*

`[C]` Their model, Eq. 14: `σs(θ) = 0 if θ < r; k·(θ − r) if θ ≥ r`: *"The first one is the radius r
of the foveal region where the visual content is rendered in the highest resolution. The second
parameter is the rate k at which the resolution is reduced towards the periphery."* Calibrated with
r ∈ {4, 7, 11}°.

`[C]` **Measured desktop timings, Table 3, 2560×1440:**

| Scene | full-res | standard (eccentricity-only) foveation | their content-aware foveation |
|---|---|---|---|
| Sponza | 2.6 ms | 2.3 ms (**1.1×**) | 3.0 ms (**0.9× — slower than not foveating**) |
| Water | 9.5 ms | 5.3 ms (**1.8×**) | 4.3 ms (**2.2×**) |
| Fog | 22.9 ms | 13.9 ms (**1.6×**) | 5.5 ms (**4.2×**) |

verbatim: *"As expected, our technique offers better performance than standard foveation when shader
complexity increases. **For the very simple Phong shader, when the geometry complexity dictates the
rendering performance, our technique cannot provide favorable results.**"*

`[C]` **Their ray-tracer result, the single most relevant datapoint:** *"the image from the figure was
rendered in 15.87 ms on a PC with an NVIDIA RTX 2080 Ti graphics card in 2560×1440 pixels resolution.
The rendering of the full resolution image took 22.70 ms, while the standard foveated rendering took
17.24 ms."* → `[I]` **1.43× content-aware, 1.32× standard-foveated, for a ray tracer at desktop
resolution.** Their foveated ray tracer traced *"47% of all rays."*

`[C]` Their own warning against fixed parameters: *"Recently proposed foveated rendering techniques
use fixed, usually manually tuned parameters to define the rate of quality degradation for peripheral
vision. As shown in this work, **the optimal degradation that maximizes computational benefits but
remains unnoticed depends on underlying content. Consequently, the fix foveation has to be
conservative and in many cases, its performance benefits remain suboptimal.**"*

`[C]` The desktop-vs-HMD split: their local (content) adaptation was preferred in **53 of 70**
comparisons on desktop (p < 0.03) but only **37 of 70** on HMD (p = 0.28).
— https://www.pdf.inf.usi.ch/projects/AdaptiveFoveation/AdaptiveFoveation.pdf ; code
https://github.com/okantursun/AwareFoveation

Also: `[C]` Walton, Dos Anjos, Friston, Swapp, Akşit, Steed & Ritschel, *Beyond blur: real-time
ventral metamers for foveated rendering*, ACM TOG 40(4), SIGGRAPH 2021 — *"To peripheral vision, a
pair of physically different images can look the same"*; real-time ventral metamers *"improves in
quality over state-of-the-art foveation methods which blur the periphery."* Their framing gives the
desktop game away: *"in particular for near-eye displays, the largest part of the framebuffer maps
to the periphery."* `[U]` numeric results not obtained.
— http://www.homepages.ucl.ac.uk/~ucabdw0/beyondblur.html

`[U]` **No peer-reviewed paper making the "desktop foveation is not worth it" argument head-on was
found.** The closest primary admission is Guenter's own scaling argument (savings grow with FOV and
resolution). A practitioner thread makes it informally — *"with a normal monitor, you can see the
whole screen (or near enough) with both eyes, so you would be saving very little work for the added
complexity"* — https://polycount.com/discussion/230968 (opinion, not evidence).

### 6.6 Arithmetic for a 27" / 60 cm monitor

All `[I]`, from Guenter's confirmed Eqs. 1/6/7, with 27" 16:9 (W* ≈ 0.598 m), V* = 0.60 m,
D* = 2560, ω₀ = 1/48°, m = 0.0220:

- Half-FOV `e* = arctan(0.598/1.20) = 26.5°` → **~53° horizontal**.
- `ω* = arctan(2·0.598/(0.60·2560)) = 0.0446°/cycle` → **ω₀/ω* ≈ 47 %**: the display resolves about
  half of foveal acuity (Guenter's 1080p rig was 40 %).
- Eccentricity at which the eye's MAR equals 2ω* (2×2 coarse shading acuity-safe): **e ≈ 3.1°**.
  At 4ω* (4×4): **e ≈ 7.2°**.
- Central resolution ≈ 44.9 px/deg → radii of ≈**139 px** and ≈**321 px** on 2560×1440 → **1.6 %** and
  **8.8 %** of frame area.
- Naive shaded-pixel work holding 1×1 inside 3.1°, 2×2 to 7.2°, 4×4 beyond:
  `0.016·1 + 0.072·0.25 + 0.912·0.0625 ≈ 0.091` → **~11× fewer shaded pixels**, consistent with
  Guenter's 10–15×.

> `[I]` **So the acuity model does not say desktop foveation is pointless. Three confirmed facts cut
> it down anyway:**
> 1. **Guenter converted 10–15× in pixels into only 4.8–5.7× in wall clock.** Everything not
>    per-pixel does not foveate. Tursun measured **0.9×** on a simple-shader scene.
> 2. **Guenter tracked gaze at 300 Hz with <10 ms latency.** Pinning the fovea to the crosshair
>    assumes the eye stays there; across a 53° flat display it does not, and a 3.1° inner radius is
>    6.2° wide. An eye flick of a few degrees puts the fovea in the 4×4 region. **This is the one
>    assumption VR FFR gets for free and a monitor does not.**
> 3. **Patney's 6× detection-vs-resolution gap at 30°** means an acuity-line falloff is already too
>    aggressive even *with* perfect tracking, unless you add contrast preservation (their 2–2.3×
>    threshold gain) and a temporally stable filter (their 10× stability improvement).
>
> **Honest expectation for crosshair-centred fixed foveation on a 27"/60 cm monitor: closer to
> Tursun's measured 1.1×–1.8× (1.32× for a ray tracer) or the shipping-VRS 8–20 %, than to Guenter's
> 5–6×.** The two things the literature says *do* pay off at desktop scale are **contrast
> preservation** (free 2–2.3× more foveation headroom) and **content-adaptive gating** (the only thing
> that turned 1.6× into 4.2× on Tursun's fog scene). Both are cheap in a ray marcher, where step
> count and sample density are continuous knobs rather than a Tier-2 tile image — which is exactly
> what Guenter predicted in his own conclusion.

---

## 7. Synthesis: the ordered plan for this engine

`[I]` All inference, built on the confirmed material above.

**The two complaints are one bug with two faces, and the literature's name for the fix is
"prefiltering," not "anti-aliasing."** §5(e) is the argument: past 13 m every voxel is sub-pixel; at
1 km, 5,900 voxels compete for one sample; at the horizon, 128,544. One primary ray plus 8-frame TAA
cannot estimate that. Meanwhile at 5 m individual 7.8 mm cubes are being resolved with no sub-voxel
signal at all, so the cubes are visible. **The same missing thing causes both: a per-node statistical
descriptor that a footprint query can integrate.**

**Ordered by value ÷ effort, highest first:**

1. **Stop normalizing the per-node average normal.** `[C]` against this repo (§1.3-A's box):
   `pack_summary` computes `length` and discards it. `σ² = (1−|N_a|)/|N_a|` is free roughness sitting
   in data already being written — but the denominator (total exposed face area) must be added to
   `NodeSummary` first, because `normal_sum` is in absolute area units. **Cheapest fix for far-field
   speckle, and the highest-leverage item in this document.**
2. **Add screen-space NDF filtering** (Tokuyoshi & Kaplanyan Listing 2, four lines, `KAPPA = 0.18`,
   `SIGMA2 = 0.25` for the 2019 form or `0.15915494` for the 2021 projected-space form). Measured at
   **0.08–0.11 ms over no anti-aliasing at 8K**. Prefer the axis-aligned/isotropic variant — the
   authors say non-axis-aligned still flickers under animation.
3. **Fix the LOD-selection rule to use the cone footprint including the grazing term**:
   `λ = log2(α·t / |n̂·d̂| / voxel_size)`, `α = arctan(2·tan(vFOV/2)/H)`. Rings live at grazing
   angles, exactly where a distance-only radius under-selects. Blend **three** levels (GigaVoxels:
   *"For proper blending three levels are a must"*).
4. **Jitter the ray start by a random fraction of the step, with screen-locked spatiotemporal blue
   noise.** Ruijters' documented cure for onion rings, and it converts ring structure into the
   dither that is wanted. STBN 64³ masks (Wolfe et al.), screen-locked, **not** migrated with motion.
5. **Add Laine & Karras' variable-radius post-process filter** for the near field: 96 samples in a
   24-px disc, radius from the projected voxel size stored as 3.5 fixed-point in alpha, with the
   `r ← min(r, r′)` seam fix. Measured *"one to two magnitudes faster than the ray casting."* This is
   the published fix for the exact word *blockiness*.
6. **Then, and only then, add the grain as its own system**: four world-space octaves with Bénard's
   weights (`s/2`, `1/2−s/6`, `1/3−s/6`, `1/6−s/6`, summing to 1), keyed off `2^floor(log2 t)`,
   band-limited with Quílez's `smoothstep(1.0, 0.5, w)` so it retires into the roughness term rather
   than aliasing. **Praun's nesting property must hold across octree levels or it will swim.**
7. **Audit TAA for grain destruction.** Neighbourhood min/max clamping will clip a sub-pixel stipple
   (TAA survey §6.1.2). Either variance-based clamping, a grain-pixel exclusion mask, or **apply the
   grain post-resolve**.
8. **The int8×3 normals are a documented limiter.** Laine & Karras: 8-bit object-space normals are
   *"insufficent for smooth highlights and reflections."* If lumpiness survives 1–7, this is next;
   their 14-bit scheme is published.
9. **Foveation last, and probably not crosshair-centred.** Expect 1.1–1.8×, not 5–6×. Spend the same
   effort on Patney's contrast preservation (free 2–2.3× headroom on whatever falloff is used) and
   Tursun's content-adaptive gating — both are continuous knobs in a marcher, the case Guenter
   himself flagged as the better fit for ray tracing.

**The render-distance budget:** at 1.7 m eye height the horizon is **4.65 km** (5.03 km with
refraction). Clear-air extinction does not bind before that. BF3 shipped 0.06 m–30,000 m on
hierarchies alone. **So 4–5 km is the defensible ground-level target and 30 km is defensible from
elevation — but the entire budget beyond 13 m is prefiltered LOD, not detail.** Aokana measured
SSIM ≈ 0.9 at `StreamingFactor = 2.0`, which is the shape of quality/VRAM knob to expose.

---

## 8. Gaps, and where the literature disagrees

**Genuine disagreements, reported not resolved:**

1. **Human resolution limit**: 30 cpd (grating acuity) / 48 cpd (Colenbrander via Guenter) / 60 cpd
   average 65, range 50–85 (Curcio via Williams) / ~10 arcsec vernier hyperacuity. Task-dependent,
   spread > 10×.
2. **Guenter's own slope range**: abstract and §5 say 0.0220–0.0275; the Conclusion says 0.022–0.034.
   The 0.034 is unsourced in the body.
3. **D3D12 VRS tile size**: the same Microsoft page says both *"16x16 or smaller"* and *"8 / 16 /
   32"*. Query at runtime.
4. **Desktop foveation payoff**: Guenter measured 4.8–5.7× (1080p, gaze-tracked, forward renderer);
   Tursun measured 1.1–1.8× (1440p, gaze-tracked, whole-frame time) and 1.32× for a ray tracer. Both
   are real measurements of different things.
5. **Anisotropic vs isotropic NDF filtering under animation**: Tokuyoshi & Kaplanyan report
   non-axis-aligned best by RMSE but say *"some pixels can still flicker in animation"* and recommend
   the biased axis-aligned kernel for dynamic scenes. Quality and stability disagree.

**What could not be obtained:**

- **Kaplanyan et al. 2016 (HPG) itself.** kaplanyan.com's TLS certificate has expired; diglib returns
  403; ACM is paywalled. **All its math here is via Tokuyoshi & Kaplanyan 2019/2021's verbatim
  restatement** (co-authored, so primary-adjacent, not the original text).
- **Reddit, entirely** — see §3. u/MGMishMash's history and the r/VoxelGameDev threads are unread;
  John Lin's pinned YouTube comment and X/Twitter unreadable.
- **Developer-stated km view distances** for MSFS, RDR2, Horizon, Ghost of Tsushima, Death Stranding.
  §5(d) rests on BF3 (strong), Voxel Quest (weak) and Minecraft (wiki, secondary).
- **vulkan.gpuinfo.org coverage** for `VK_KHR_fragment_shading_rate` — both endpoints 403. Check
  manually: https://vulkan.gpuinfo.org/listdevicescoverage.php?extension=VK_KHR_fragment_shading_rate&platform=windows
- **Intel's VRS tier per GPU generation.** Intel's own article names only Gen11 and states no tier.
- **Walton et al. 2021 numeric results** — abstract and project page only.
- **The Meta FFR "26–36 %" figure** circulating in secondary summaries — **not present in the primary
  Meta docs.** Use 6.5 / 11.5 / 21 % and the 25 % pixel-intensive figure.
- **WMO CIMO Guide's visibility chapter text** — cited via a secondary source and verified
  arithmetically, not read directly.
- **Toksvig 2005 (*Mipmapping Normal Maps*, JGT 10(3))** — paywalled. Its formula is quoted from
  Crassin's paper and Stephen Hill's write-up, which state it identically.
- **A measured perceptual result for "grain increases perceived detail."** The weakest link in this
  document: it rests on one SPIE abstract plus practitioner consensus.

---

# 9. Addendum — corrections and new primary sources (same day, two later passes)

Two follow-up research passes landed after §§1–8 were written. **Everything here supersedes or
extends §5 and §7 only; §§1–4 and §6 stand as written.** Four of these are corrections to what is
above, one of them to a recommendation I made in §7.

## 9.1 CORRECTION to §5(b): two different "60"s were conflated — and this moves a budget by 2×

§5(b) listed *30 cpd (gratings)* and *60 cpd (cone Nyquist)* as competing estimates of the **same**
quantity. **They are not.** One is a task threshold in cycles/degree; the other is a sampling
ceiling. The display criterion is in *pixels* per degree, which is twice the cycles per degree.

`[C]` University of Arizona notes, converting 20/20 directly: *"5 arcmin or 25 µm → 1 cycle/2 arcmin
× (60 arcmin/1 deg) = **30 cycles/deg**"* —
https://wp.optics.arizona.edu/visualopticslab/wp-content/uploads/sites/52/2016/08/Class04_08.pdf

`[C]` Webvision (Kalloniatis & Luu), *Visual Acuity*: *"For a visual acuity of 6/6 (20/20), one of
the strokes of the letter subtends one minute of arc at the eye. Therefore, the minimum angle of
resolution (MAR) is one minute of arc."* And, separately: *"Campbell and Green (1965)… used
interference patterns generated by a laser to bypass the optics of the eye… They found that the
maximum resolution was about 60 cycles per degree."*
— https://www.webvision.pitt.edu/book/part-viii-psychophysics-of-vision/visual-acuity/

`[I]` The chain reconciles exactly:

| ppd | Nyquist (c/deg) | pixel pitch | what it is |
|---|---|---|---|
| **60** | **30** | 1.000′ | 20/20 acuity; the "retina display" rule of thumb |
| 94 | 47 | 0.638′ | Ashraf et al. 2025 measured foveal achromatic limit |
| **120** | **60** | 0.500′ | Campbell & Green 1965 interferometric ceiling |
| ~130 | 64.9 | 0.462′ | in-vivo average cone-Nyquist limit |
| 300 | 150 | 0.200′ | Williams' foveal aliasing ceiling |

**20/20 = 1′ MAR = 30 c/deg = 60 ppd. Campbell & Green's 60 c/deg = 120 ppd. Do not treat them as
the same number.**

## 9.2 New primary source, 2025, aimed squarely at display budgets

`[C]` Ashraf, Chapiro & Mantiuk, *Resolution limit of the eye: how many pixels can we see?*, Nature
Communications (2025), DOI 10.1038/s41467-025-64679-2: *"Our results demonstrate that the resolution
limit is higher than what was previously believed, reaching **94 pixels-per-degree (ppd)** for foveal
achromatic vision, **89 ppd** for red-green patterns, and **53 ppd** for yellow-violet patterns."*
And the framing: *"set the north star for display development, with implications for future imaging,
rendering and video coding technologies."* — https://arxiv.org/abs/2410.06068

`[I]` The "higher than previously believed" is measured against the **60 ppd** display criterion,
not against Campbell & Green's 60 c/deg. **60 ppd is the defensible target, 94 ppd the measured
achromatic limit, 120 ppd the physiological ceiling.** The chroma numbers are a free win: grain can
be more aggressive in yellow-violet (53 ppd) than in luminance.

## 9.3 The result that undercuts a pure Nyquist band-limit — and justifies §2's whole approach

`[C]` Williams & Coletta, *Cone spacing and the visual resolution limit*, JOSA A 4(8):1514 (1987),
abstract verbatim:

> *"It is commonly assumed that the visual resolution limit must be equal to or less than the Nyquist
> frequency of the cone mosaic. However, under some conditions, observers can see fine patterns at
> the correct orientation when viewing interference fringes with spatial frequencies that are as much
> as about **1.5 times higher** than the nominal Nyquist frequency of the underlying cone mosaic. […]
> The Nyquist frequency specifies which images can be reconstructed without aliasing by an imaging
> system that samples discretely. However, **it is not a theoretical upper bound for psychophysical
> measures of visual resolution** because the observer's criteria for resolving sinusoidal gratings
> are less stringent than the criteria specified by the sampling theorem for perfect, alias-free
> image reconstruction."*

— https://aria.cvs.rochester.edu/papers/williams-coletta_JOSAA1987.pdf

`[C]` Rossi & Roorda, *Nature Neuroscience* 13:156–157 (2010): *"MAR agreed well with estimates of Nc
at the PRLF"* but *"MAR was worse than predicted by Nc at locations eccentric to the PRLF"*,
concluding *"MAR is governed by the Nyquist limit of the mRGC mosaic across the fovea."* Regression
slope vs cone Nyquist 0.6355 (significantly ≠ 1); vs mRGC Nyquist 1.0111 (not significantly ≠ 1).
— https://pmc.ncbi.nlm.nih.gov/articles/PMC2822659/

`[C]` In-vivo cone data — *Human foveal cone photoreceptor topography and its dependence on eye
length*, eLife 2019 (adaptive optics, living eyes): sampling limit
`SamplingLimit = 1223/√AngularDensity`; *"Potential spatial frequency resolution limits ranged from
**59.1 to 74.01 cycles/degree** at peak density; average **64.9 cycles/degree**"*; cone spacing
*"0.59 to 0.47 arcminutes"*. On the histology discrepancy: *"adaptive optics measurements in living
eyes are consistently lower, possibly due to tissue shrinkage in histology or selection bias."* And:
*"Peak angular density increases significantly with axial length"* — **myopes get more cones per
degree.** — https://elifesciences.org/articles/47148

`[C]` Williams (1988), *Vision Res.* 28(3): the optical ceiling *"for a dilated 8 mm pupil and
λ = 632.8 nm, the highest spatial frequency that can be imaged on the retina is **221 c/deg**"*; the
aliasing ceiling *"The moiré patterns could be seen at spatial frequencies as high as **150
c/deg**."* — https://aria.cvs.rochester.edu/papers/williams_VR1988.pdf

> `[I]` **The engine-relevant reading: the eye is not a clean Nyquist sampler, and "below the Nyquist
> limit" is not a guarantee of invisibility.** Detail 1.5× past the mosaic's Nyquist frequency is
> still discriminable at the right orientation, and outside the very foveal centre the binding limit
> is ganglion-cell, not cone, sampling. **This is the physiological reason a band-limit set to the
> display's Nyquist rate can still look wrong — and the reason §0(a)'s "convert the aliasing into a
> well-distributed dither" beats "filter until it is provably below Nyquist."**

## 9.4 CORRECTION to §5(e)'s pixel-angle arithmetic — two defensible numbers, and which to use

§5(e) gave 7.8 mm sub-pixel beyond **12.97 m** at 1080p/60°. A parallel computation gave **14.30 m**.
Both are right; they answer different questions.

`[I]`
- **Linear deg/px** (frame average): 60/1920 = 0.03125°/px = 5.4542×10⁻⁴ rad = 1.875′ = 32.0 ppd →
  sub-pixel beyond **14.30 m**.
- **Centre-pixel spread angle** (Ray Tracing Gems Eq. 30): 6.0141×10⁻⁴ rad = 2.067′ = 29.0 ppd →
  sub-pixel beyond **12.97 m**.

**The centre pixel subtends 10.3 % more angle than the frame average**, because under perspective
projection edge pixels cover less angle per pixel. **Use the centre-pixel figure (12.97 m): it is
the worst case, it is where the crosshair is, and it is the value a footprint-based LOD rule
consumes. The frame-average value will under-select the octree level exactly at screen centre.**

`[I]` Extended grid, linear deg/px (multiply by 0.907 for centre-pixel):

| width | hFOV | arcmin/px | ppd | 7.8 mm sub-pixel beyond |
|---|---|---|---|---|
| 1920 | 60° | 1.875 | 32.0 | 14.30 m (**12.97 centre**) |
| 1920 | 70° | 2.188 | 27.4 | 12.26 m |
| 1920 | 90° | 2.812 | 21.3 | 9.53 m |
| 2560 | 60° | 1.406 | 42.7 | 19.07 m (17.29 centre) |
| 2560 | 90° | 2.109 | 28.4 | 12.71 m |
| 3840 | 60° | 0.938 | 64.0 | 28.60 m (25.94 centre) |
| 3840 | 90° | 1.406 | 42.7 | 19.07 m |

`[I]` Retina-matched FOV: at 1′/px (60 ppd), 1920 px covers only **32°** and 3840 px covers **64°**.
**A 60° FOV at 4K (64 ppd) is the first common configuration that actually meets the 20/20
criterion.** At Ashraf's 94 ppd, 3840 px covers just **41°**.

> `[I]` **Refined form of §5(e) conclusion 3: at 1080p/60° the DISPLAY is the binding constraint, not
> the eye — by a factor of ~1.9 (14.3 m vs 26.8 m). The eye only becomes binding past 4K. So the
> prefilter target is the pixel footprint at 1080p, and should switch to targeting acuity at 4K+ —
> which is a LOOSER target and buys detail back. Higher-resolution users should get more visible
> grain from the same code, not less.**

## 9.5 CORRECTION to §7: a feature is visible far past *your own* horizon

**§7 said "4–5 km is the defensible ground-level target." That is wrong in one direction and it
under-serves tall geometry by an order of magnitude.**

`[C]` Wikipedia, *Horizon* (secondary): `D_BL = D_B + D_L`, approximated as
*"D_BL < 3.57 ( √h_B + √h_L )"* (km, metres). Worked example: eye 1.70 m (4.65 km) + 100 m tower
(35.7 km) → *"approximately 40.35 km"*. — https://en.wikipedia.org/wiki/Horizon

`[I]` Max distance a feature of height H is visible from eye height h, `D = c(√h + √H)`:

| eye h | H = 1 m | 10 m | 100 m | 500 m | 1000 m | 2000 m | 4000 m |
|---|---|---|---|---|---|---|---|
| 1.7 m, c = 3.57 | 8.2 km | 15.9 | **40.4** | 84.5 | **117.5** | 164.3 | 230.4 |
| 1.7 m, c = 3.856 | 8.9 km | 17.2 | **43.6** | 91.3 | **127.0** | 177.5 | 248.9 |
| 10 m, c = 3.856 | 16.0 km | 24.4 | 50.8 | 98.4 | 134.1 | 184.6 | 256.1 |

> `[I]` **At human eye height, 1 m detail vanishes below the horizon at ~8.9 km — but a 1000 m massif
> stays geometrically visible to ~127 km and a 4000 m peak to ~249 km. If the world has mountains, a
> 4.65 km far plane cuts off geometry the player can physically see. The correct structure is a
> TWO-TIER far range: full prefiltered octree LOD out to ~9 km, plus a very coarse
> silhouette/impostor tier out to 100+ km.**

`[C]` A **third** horizon coefficient, and a genuine standards disagreement. Young: *"OG ≈ sqrt(2 R h)
… **neglecting refraction**"*; *"A typical value of the ratio is about 1/7"*; *"R′ = R × 7/6"* → 7440 km.
And the caveat that should have been quoted in §5(a): *"You can consider them accurate to a few per
cent, most of the time. But, occasionally, they will be wildly off, particularly if superior mirages
are visible."* — https://aty.sdsu.edu/explain/atmos_refr/horizon.html
Young, *Dip of the Horizon*: *"dip = 1.75′ × sqrt(h, meters)"*, with *"k is about 1/6 or 1/7 at sea
level"* but which *"varies from moderate negative values (on sunny days) to values considerably
larger than 1 (at night, or for sight-lines over cold water)."*
— https://aty.sdsu.edu/explain/atmos_refr/dip.html
`[C]` The navigational constant (second-hand, via NavList on Bowditch Table 12): *"1.169 times the
square root of height of your eye = distance to horizon in nautical miles"* (h in feet).
— https://navlist.net/Distance-horizon-Bowditch-other-sources-FrankReed-jan-2024-g55253

`[I]` 1.169 nmi·ft^−½ = **3.9215 km·m^−½**, implying an effective radius of 1.207 R ≈ 7690 km —
**not** Young's 7/6 R = 7440 km. **The full spread across authorities is 3.57 → 3.856 → 3.92, about
10 %.** Also: √(2R)/1000 with R = 6,371,000 m = **3.56959** exactly, ×√(7/6) = **3.85560** — so
"3.86" is 3.8556 rounded, and an 8/7 effective radius would give **3.816**, not 3.86.

`[C]` Measured upper bound: Guinness World Records — *"The longest line of sight on earth photographed
is **493.07 km** (306.37 mi), achieved by Richard Jezik (Slovakia), in Giresun, Turkey, on 15 December
2024."* — https://www.guinnessworldrecords.com/world-records/66661-longest-line-of-sight-on-earth
(prior record 443 km, Pic de Finestrelles → Pic Gaspard, 2016).

## 9.6 CORRECTION to §5(c): the real WMO CIMO Guide prints `≈ 3/σ`, and never 3.912

§5(c) cited 3.912 from a secondary source and flagged the CIMO Guide as unread. **It has now been
obtained and read directly** — the 2023 edition, Chapter 9 (printed pp. 325–347), via a national
meteorological service mirror (Mongolia's NAMEM), because library.wmo.int serves only a JavaScript
shell to a fetcher. Provenance stated honestly: a mirror, internally self-consistent, title and
copyright pages intact.
https://amc.namem.gov.mn/wp-content/uploads/WMO/1.%208_I-2023_en.pdf
(WMO's own record: https://library.wmo.int/records/item/68663-guide-to-instruments-and-methods-of-observation)

`[C]` **WMO-No. 8 eq. 9.6, §9.1.4, verbatim:** *"Hence, the mathematical relation of MOR to the
extinction coefficient is: **P = (1/σ) · ln(1/0.05) ≈ 3/σ** (9.6) where ln is the log to base e or
the natural logarithm."*

**Note the "≈" and the bare "3". The constant 3.912 does not appear anywhere in WMO-No. 8.** If you
are citing WMO, cite `≈ 3/σ`. `[I]` 2.996 is correct arithmetic (ln 20 = 2.995732…) but it is
Wilson et al.'s rendering, not WMO's printed figure. **§5(c) conflated the two.**

`[C]` **And the structural point §5(c) got wrong by implication: within WMO's framework the two
thresholds are not competing alternatives.** §9.1.3, verbatim: *"In 1924, Koschmieder, followed by
Helmholtz, proposed a value of 0.02 for ε. Other values have been proposed by other authors. **They
vary from 0.0077 to 0.06, or even 0.2.** The smaller value yields a larger estimate of the visibility
for given atmospheric conditions. For aeronautical requirements, it is accepted that ε is higher than
0.02, and it is taken as 0.05 since, for a pilot, the contrast of an object (runway markings) with
respect to the surrounding terrain is much lower than that of an object against the horizon. It is
assumed that, when an observer can just see and recognize a black object against the horizon, the
apparent contrast of the object is 0.05, and… this leads to the choice of 0.05 as the transmission
factor adopted in the definition of MOR."* **0.05 IS the MOR definition (T = 0.05, eq. 9.5) and
simultaneously the assumed apparent contrast of a just-recognised black object (eq. 9.10). 0.02 is
Koschmieder's historical value, which WMO reports but does not use.** Edition drift worth noting:
Wilson et al. quote WMO **2010** as giving the range "0.01 to 0.2"; the **2023** edition prints
"0.0077 to 0.06, or even 0.2".

`[C]` Koschmieder's law as WMO states it, §9.1.4: contrast *"**C = (L_b − L_h)/L_h** (9.8)"*; *"if the
object is black (L_b = 0), C = –1"*; *"**C_x = C₀e^(−σx)** (9.9). This relationship is valid provided
that the scatter coefficient is independent of the azimuth angle and that there is uniform
illumination along the whole path"*; *"If a black object is viewed against the horizon (C₀ = –1) and
the apparent contrast is –0.05, equation 9.9 reduces to: **0.05 = e^(−σx)** (9.10)"*. Bouguer–Lambert:
*"**F = F₀e^(−σx)** (9.1)"*, *"**T = F/F₀ = e^(−σx)** (9.4)"*. Allard's law for night point sources:
*"**E = I · x^(−2) · e^(−σx)** (9.11)"*.

`[C]` **WMO's own definitions, §9.1.1**, verbatim: *"**Meteorological optical range.** The length of
path in the atmosphere required to reduce the luminous flux in a collimated beam from an incandescent
lamp, at a colour temperature of 2 700 K, to **5%** of its original value. The luminous flux is
evaluated by means of the photometric luminosity function of CIE."* And *"**The contrast threshold
(symbol ε).** The minimum value of the luminance contrast that the human eye can detect… The contrast
threshold varies with the individual."*

`[C]` **WMO's definition of airlight, §9.1.1 — and the last sentence is a rendering insight:**
*"Light from the sun and the sky that is scattered into the eyes of an observer by atmospheric
suspensoids… **Airlight is the fundamental factor limiting the daytime horizontal visibility for
black objects**… **Contrary to subjective estimates, most of the airlight entering observers' eyes
originates in portions of their cone of vision lying rather close to them.**"*
`[I]` **The near field dominates the haze integral** — which is exactly why a front-loaded froxel
distribution works (Ghost of Tsushima's 64 depth slices over 10 cm–100 km, §9.7).

`[C]` **Maximum reportable visibility, §9.1.2:** *"While for synoptic meteorological requirements, the
scale of MOR readings extends from below 100 m to **more than 70 km**, the measurement range may be
more restricted for other applications. This is the case for civil aviation, where the upper limit
may be **10 km**."* And: *"The errors of visibility measurements increase in proportion to the
visibility… three linear segments with decreasing resolution, namely, **100 to 5 000 m in steps of
100 m, 6 to 30 km in steps of 1 km, and 35 to 70 km in steps of 5 km.**"*
`[I]` That progressive-resolution scale is a validated real-world precedent for LOD banding:
**precision degrades in proportion to distance, deliberately.**

`[C]` Practical caveat: *"The relationship between the transmission factor and MOR is valid for fog
droplets, but when visibility is reduced by other hydrometeors (such as rain or snow) or lithometeors
(such as blowing sand), MOR values should be treated with more care."*

**A genuine unresolved disagreement, in the opposite direction to §5(c)'s Carr figure.** §5(c) gave
Carr (2005) via Wilson et al.: `V′ ≈ V/(1.3 ± 0.3)` — the observer sees ~23 % *less* far.
`[C]` WMO-No. 8 §9.2.5 says the opposite sign, verbatim: *"Middleton (1952) found, from 1 000
measurements, that the mean contrast ratio threshold for a group of 10 young airmen trained as
meteorological observers was **0.033**… **If the Middleton data represent normal observing conditions,
we must expect daylight estimates of visibility to average about 14% higher than MOR with a standard
deviation of 20% of MOR.** These calculations are in excellent agreement with the results from the
First WMO Intercomparison of Visibility Measurements (WMO, 1990), where it was found that, during
daylight, **the observers' estimates of visibility were about 15% higher than instrumental
measurements.**"*
`[I]` **These cannot both be right.** WMO has two independent lines (Middleton's threshold statistics,
and a formal intercomparison) saying human daylight estimates run **14–15 % higher** than MOR;
Carr's relation says observer visibility is ~1.3× *lower*. **For a game: treat MOR and perceived
visibility as equal to within ±20 %, and do not build anything on the 1.3 factor.**

`[C]` **The operative wavelength, which settles §5(c)'s 296-vs-391 km reconciliation** — Queißer et
al., *Atmos. Meas. Tech.* 15:5527–5544 (2022), p. 5528: *"contrast between the object and the
surrounding sky is either Ct = 2 %, or, as later suggested, 5 % … Adopting the well-known
Bouguer–Lambert–Beer law and Ct = 5 %, MOR can be written as MOR = −ln(Ct)/σ ≈ 3/σ … **In practice,
MOR is evaluated at a wavelength of 550 nm**, close to the human eye's sensitivity maximum."*
**550 nm is the operative wavelength, not the 520 nm the Wikipedia figure used.**
— https://amt.copernicus.org/articles/15/5527/2022/amt-15-5527-2022.pdf

`[C]` The second independent Rayleigh anchor, with both thresholds in one sentence — Hyslop,
*Atmos. Environ.* 43 (2009): *"VR and b_ext are inversely related by the Koschmieder equation,
VR = ln(CL)/b_ext, where CL is the minimum observable contrast … and is **equal to 0.02 – 0.05 for
most observers**."* Also *"**gas scattering value of 13.2 Mm⁻¹**"*, and the caveat that matters:
*"The Koschmeider equation is inaccurate if illumination is non-uniform and **for very clean
atmospheres where the curvature of the earth becomes a factor**."*
— https://airquality.ucdavis.edu/sites/g/files/dgvnsk1671/files/inline-files/Impaired%20visibility.pdf
`[I]` **At the 296–391 km end of §5(c)'s table, Koschmieder is being applied outside its own validity
domain. Those figures are order-of-magnitude, not precise.**

`[C]` Also confirming both thresholds: ITU-R Report F.2106-1 (2010) §3.2.1 p. 11: *"In the
literature, the two following values of ε are found ε = 0.02 or ε = 0.05."*
— https://www.itu.int/dms_pub/itu-r/opb/rep/r-rep-f.2106-1-2010-pdf-e.pdf
And the AMS Glossary, *meteorological range*: *"The meteorological range is the distance V′ in the
black target form of the visual-range formula… when the threshold contrast ε is set equal to 0.05.
Thus, V′ is a function only of the extinction coefficient σ."*
— https://glossary.ametsoc.org/wiki/Meteorological_range

`[C]` Koschmieder's original is a **two-part paper** — cite both page ranges: *"Koschmieder, H. 1924.
Theorie der horizontalen sichtweite. Beitr. Phys. Freien Atm., **12:33–53, 171–181**."* (from
Narasimhan & Nayar's reference list). Notable: **Koschmieder 1924 is not in the CIMO Guide's own
reference list** — WMO names him only in running text.

`[U]` **The "Middleton says 350 km" attribution remains unverified after two dedicated searches.**
Full citation confirmed from CIMO Chapter 9's reference list: *"Middleton, W.E.K., 1952: Vision
Through the Atmosphere. Toronto, University of Toronto Press."* No retrievable source quotes a
specific Rayleigh-limited visual range from it. **Do not attribute a km figure to Middleton** — derive
it from a quotable σ instead.

`[C]` **And the independently corroborated far bound** — ECMWF Forecast User Guide §9.4: *"**Visibility
can not be greater than 100km as this reflects the extinction coefficient of clean air used by the
calculation.**"* — https://confluence.ecmwf.int/display/FUG/Section+9.4+Visibility
`[I]` **A production numerical weather model hard-caps visibility at 100 km — the same figure a
shipped AAA renderer chose (§9.7). Two entirely independent domains converging on 100 km is the
strongest support here for 100 km as the practical atmospheric far bound.** Operational
meteorological reporting tops out at 70 km; aviation at 10 km (`[C]` NOAA CO-OPS: *"CO-OPS limits the
reported MOR to a range of 5.4 nautical miles (10 kilometers)"*).

`[C]` **The aerial-perspective authoring rule** — US EPA, *Visibility in Mandatory Federal Class I
Areas* ch. 1: *"Without the effects of manmade air pollution, a natural visual range would be nearly
**140 miles (225 km)** in western areas and **90 miles (145 km)** in eastern areas"*; *"the light
extinction from Rayleigh scattering by air is uniformly set at **10 inverse megameters (Mm⁻¹)**"*;
deciview `dv = 10 ln(b_ext / 10 Mm⁻¹)`. **And directly a rendering spec:** *"noticeable degradation of
scenic appearance (including the disappearance of some features) occurs on some objects as near as
within **10 percent of the visual range**."*
— https://www.epa.gov/sites/default/files/2015-05/documents/chap01.pdf
`[I]` **With clear-air V ≈ 39 km, features start visibly washing out from ~3.9 km — right around the
ground-level horizon. Fog should be doing perceptible work from the first few kilometres, which is
also why it can carry the LOD transition rather than fighting it.**

`[C]` National Academies Press, *Protecting Visibility in National Parks and Wilderness Areas* ch. 4:
*"V = 3.9/b_ext for a contrast threshold of 0.02"*; extinction *"as little as **10⁻² km⁻¹** in
pristine deserts"* to *"as much as **1 km⁻¹** in polluted urban areas"*; *"an average extinction
coefficient of about **0.015 km⁻¹**"* for a dark mountain visible at 100 km; and *"fog is limited to
visibilities less than 0.5 km. Visibilities between 0.5 and 1 km are properly called mist."*
— https://www.nationalacademies.org/read/2097/chapter/6

`[C]` **The two-term model a fog shader needs** — Narasimhan & Nayar, *Vision and the Atmosphere*,
IJCV 48(3):233–254 (2002): attenuation *"E(d,λ) = E₀(λ)e^(−β(λ)d)"*; airlight
*"L(d,λ) = L_∞(λ)(1 − e^(−β(λ)d))"* — *"the radiance of airlight for an object right in front of the
observer (d = 0) equals zero"*; wavelength dependence *"β(λ) = Constant/λ^γ, where γ ∈ [0,4]. **For
fog and dense haze, γ ≈ 0**… In these cases, β does *not* change appreciably with wavelength."*
Their Table 1 (adapted from McCartney 1975): Air, molecule, 10⁻⁴ µm, 10¹⁹ cm⁻³; Haze, aerosol,
10⁻²–1, 10³–10; Fog, water droplet, 1–10, 100–10; Cloud, 1–10, 300–10; Rain, water drop, 10²–10⁴,
10⁻²–10⁻⁵. — https://www.cs.columbia.edu/CAVE/publications/pdfs/Narasimhan_IJCV02.pdf

`[I]` Practical β table. **Only the two Rayleigh rows are quoted values; the rest is σ = 3/MOR applied
to authoritatively-defined visibility thresholds.** To convert to the ε = 0.02 convention, multiply
every σ by 3.912/3 = **1.304**.

| condition | β | V (3.912/β) | V (≈3/β, WMO) |
|---|---|---|---|
| Rayleigh only (EPA 10 Mm⁻¹ / Hyslop 13.2 Mm⁻¹) | 10–13.2 Mm⁻¹ | 391–296 km | 300–227 km |
| pristine desert (NAS) | 0.01 km⁻¹ | 391 km | 300 km |
| clean-air 100 km vista (NAS) | 0.015 km⁻¹ | 261 km | 200 km |
| clear | 0.1 km⁻¹ | 39.1 km | 30.0 km |
| light haze | 0.4 km⁻¹ | 9.8 km | 7.5 km |
| polluted urban (NAS) | 1 km⁻¹ | 3.91 km | 3.00 km |
| fog | 10 km⁻¹ | 391 m | 300 m |

`[I]` **§5(c)'s 296 vs 340 vs 391 km spread resolves completely: every published "clean-air maximum"
is the same formula with a different Rayleigh coefficient and/or threshold. Defensible range
~225–400 km, and the model is outside its validity domain there anyway.**

`[U]` **No authoritative source provides a single published σ table** for pure air / clear / haze /
mist / fog as a set. CIMO Chapter 9 contains none (checked directly). The "International Visibility
Code" tables in the free-space-optics literature give visibility bands and dB/km, not σ, and those
with σ trace to McCartney (1975) or Kim et al. (2001), neither openly available.

## 9.7 §5(d) closed — with three widely-cited numbers that turn out not to exist

`[C]` **The best-sourced shipped far range, and it is a developer talk** — Jasmin Patry (Sucker
Punch), *Real-Time Samurai Cinema*, SIGGRAPH 2021 *Advances in Real-Time Rendering*: *"Froxel grid,
128 W x 64 H x 64 D. Covers entire depth range (**10 cm to 100 km**)."* Terrain probe layout:
*"Regular grid of quadratic SH probes: 16x16x3 per **200m square tile, 20x80 tiles**"* — a
4 km × 16 km probe-covered extent.
— https://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html
Corroborated by the streaming lead, Adrian Bentley: *"one of our **200m x 200m terrain tiles** only
usually takes up around 2 MB (compressed) on disc, including all the terrain and foliage
placements."* — https://kotaku.com/ghost-of-tsushima-devs-slowed-down-load-times-so-you-co-1844409624

> `[I]` **Ghost of Tsushima — a shipped PS4 title — budgets its participating-media volume from 10 cm
> to 100 km. That is the single most useful calibration point in this whole document, it matches
> §9.5's finding that tall silhouettes stay visible to ~127 km, and it matches ECMWF's 100 km cap. It
> also shows the far tier can be almost free: 128×64×64 froxels across five orders of magnitude of
> depth.**

`[C]` **Minecraft — better than §5(d)'s figure.** Microsoft Learn (official, Bedrock):
*"**Render Distance** refers to the distance, measured in chunks, from the player (or camera) the game
engine will draw blocks"*; *"On the PC, it's up to **96 chunks**… The allowable range for render
distances is **4–96 chunks**"*; Realms *"with a typical maximum of 20 chunks"*; a chunk is *"16 blocks
wide and 16 blocks long"*; a block defaults to *"**1 cubic meter**"*. And the fog admission from the
Java options page: *"The most distant terrain is faded into the sky color as if by fog, to avoid a
sharp edge to the visible world; **therefore, this option is also known as 'fog'**."*
— https://learn.microsoft.com/en-us/minecraft/creator/documents/simulationrenderdistanceguide?view=minecraft-bedrock-stable
`[I]` 96 × 16 = **1536 m** (Bedrock max); 32 × 16 = 512 m (Java max). **§5(d) cited only the 512 m
Java figure — the real ceiling is 1536 m. Either way: the most successful voxel game ever ships a
~1.5 km ceiling, two orders of magnitude below the geometric horizon, and hides the difference with
fog.** That is the most instructive number here for a voxel engine.

`[C]` **Microsoft Flight Simulator — no km figure exists, and there is a structural reason.** MSFS
2024 SDK, *LOD Selection System*: LOD selection uses the *"ratio between the size of this bounding
sphere and the **vertical** size of the screen"*; *"An object that has a screen size of less than
**0.1%** is **never displayed**"*; *"The last LOD `minSize` value is always forced to 0.1%."*
— https://docs.flightsimulator.com/msfs2024/html/3_Models_And_Textures/Modeling/LODs/LOD_Selection_System.htm
The only hard metre figure in the SDK is a debug clamp: *"When *on* meshes will only be displayed up
to a distance of **3km** from the camera. When *off* the display distance is essentially infinite."*
— https://docs.flightsimulator.com/html/Developer_Mode/Menus/Debug/Debug_LODs.htm
Asobo's GDC 2022 terrain talk documents a Bing quadtree with tiles requested by *"Estimated tile
screen size"* and *"Estimated available network bandwidth"*, world scale *"510.1 millions km²"* —
**but states no view distance.**
— https://www.asobostudio.com/files/inline-images/Designing_Terrain_System_Fuentes_Lionel.pdf

> `[I]` **MSFS is the strongest existence proof for §1.2's footprint-based LOD: the largest-scale
> terrain renderer that ships selects LOD purely by projected screen size, with a hard
> 0.1 %-of-screen-height cull. Same rule as §7 item 3, expressed as a fraction rather than in
> pixels.**

Other citable numbers, all developer statements:
- `[C]` **Just Cause 2** — Emil Persson (Avalanche), *Creating Vast Game Worlds*, SIGGRAPH 2012:
  *"Unit is meters · [-16384 .. 16384] · **32km x 32km** · 1024 km² · 400 mi²"*, with the precision
  note *"For us the worst precision on world coordinates was in the **8k to 16k** range… Floats have a
  **millimeter** resolution at this range."*
  — https://humus.name/Articles/Persson_CreatingVastGameWorlds.pdf
- `[C]` **Far Cry 5** — GDC 2018 abstract: *"How do you fill up **100 square km** of wilderness with a
  terrain changing every day?"* — https://www.gdcvault.com/play/1025557/Procedural-World-Generation-of-Far
  `[U]` The companion terrain talk's 10 km × 10 km / 0.5 m / 2 km-tile figures come from a third-party
  transcription; the primary PDF exceeded the fetch limit. **Verify before quoting.**
- `[C]` **Star Citizen** — direct dev quotes via press. Chris Roberts: *"Pretty much every 3D engine
  works in 32-bit, which is great for a normal FPS – because you usually don't have maps that are
  bigger than, say, **8km x 8km** – but for us, we've got star systems that are **millions of
  kilometers across**"* — https://gamersnexus.net/gg/2019-star-citizen-multi-crew-interview-chris-roberts
- `[C]` **Unreal Engine 5**: *"The default **WORLD_MAX** size is **88 million kilometers**"*; *"If you
  want to use the UE4 WORLD_MAX size of **21 kilometers**, you can set the global value
  `UE_USE_UE4_WORLD_MAX`"*
  — https://dev.epicgames.com/documentation/unreal-engine/large-world-coordinates-in-unreal-engine-5
- `[U]` **DCS World** — forum transcriptions only: `farDistance = 20000.0`,
  `farFullRenderDistance = 15000.0`, `nearFullRenderDistance = 2000.0`; a High-preset
  `far_clip = 150000` with `trees = {1000, 12000}`. **Treat as unverified.**

**Three negative results worth as much as the positives:**
1. `[U]` **Red Dead Redemption 2 has no attributable view distance.** Rockstar has published no
   GDC/SIGGRAPH talk, blog, or SDK doc on RDR2 rendering. The only evidence is a *unitless*
   `lodScale value="0.900000"` in `system.xml` from a forum. **Every circulating km figure is
   community-derived.**
2. `[U]` **Death Stranding likewise.** KJP's CTO describes three terrain data types *"dynamically
   switching between them based on viewing distance"* — qualitative only.
3. `[U]` **No Assassin's Creed / Anvil talk stating a view distance was found.**

Also `[U]`: Unity's Camera manual page **states no default far-plane value** — the widely-repeated
"far = 1000" is community lore, not official. And **the altitude figures on the SIGGRAPH 2017 *Nubis*
slides ("8 km", "4 km", "1.5 km") are Luke Howard's 1802 meteorological cloud classification, not
renderer parameters — do not cite them as engine numbers.** Guerrilla publish extensively on Horizon
but **no terrain view distance in km**; *Decima Engine: Visibility* gives only *"a high precision
object transform based on a **1m integer grid**"*, and *Nubis³* (2023) gives a cloud domain of
16 km × 16 km.

## 9.8 The revised budget table — this supersedes §5 and §7's figures

| bound | value | basis |
|---|---|---|
| 7.8 mm voxel sub-pixel, 1080p/60°, **centre pixel** | **12.97 m** | §9.4 |
| same, frame-average deg/px | 14.30 m | §9.4 |
| 7.8 mm below 1′ eye acuity | 26.81 m | §5(e) |
| 7.8 mm below 0.5′ ceiling | 53.63 m | §5(e) |
| 1 m feature = 1 px @1080p/60° | 1.83 km | §9.4 |
| 1 m feature = 1 arcmin | 3.44 km | §5(e) |
| Geometric horizon, 1.7 m eye | 4.65 km (5.03 refracted; 5.11 Bowditch) | §9.5 |
| **1 m detail visible past horizon, 1.7 m eye** | **~8.9 km** | §9.5 |
| **1000 m massif visible, 1.7 m eye** | **~127 km** | §9.5 |
| Aerial-perspective degradation onset | within **10 %** of visual range | §9.6 (EPA) |
| Clear-air atmospheric cutoff (β = 0.1 km⁻¹) | 30–39 km | §9.6 |
| Observer vs instrument | **±20 %, sign disputed** (WMO +14–15 %; Carr −23 %) | §9.6 |
| Clean-air Rayleigh ceiling | 225–400 km, λ- and ε-dependent, model out of domain | §9.6 |
| Operational met. reporting ceiling | **70 km** (aviation **10 km**) | §9.6 (CIMO §9.1.2) |
| **NWP model visibility cap** | **100 km** | §9.6 (ECMWF) |
| **Best-documented shipped far range** | **10 cm – 100 km** (Ghost of Tsushima) | §9.7 |
| **Best-documented shipped voxel range** | **1536 m** (Minecraft Bedrock, 96 chunks) | §9.7 |
| Measured record sightline | 493.07 km | §9.5 |

## 9.9 What changes in §7's recommendations

**Two items move:**

- **§7's render-distance conclusion was wrong in one direction.** "4–5 km ground-level" under-serves
  tall geometry by an order of magnitude. **The right structure is a two-tier far range** — full
  prefiltered octree LOD out to ~9 km, plus a very coarse silhouette/impostor tier out to 100+ km.
  Ghost of Tsushima's 10 cm–100 km froxel volume at 128×64×64, and ECMWF's independent 100 km cap,
  are the evidence that the far tier can be almost free.
- **§7 item 3's footprint rule must use the centre-pixel α, not deg/px.** The 10.3 % difference
  matters because the frame-average value under-selects the octree level exactly at screen centre.

**Two items are reinforced:**

- **The band-limit in §7 item 6 should target the pixel footprint at 1080p and switch to targeting
  acuity at 4K+.** The display is binding by ~1.9× at 1080p; the eye becomes binding only past 4K,
  and that is a *looser* target — so higher-resolution users should get *more* visible grain from the
  same code, not less.
- **Williams & Coletta's 1.5×-past-Nyquist result (§9.3) is the physiological argument for §2's whole
  approach.** A band-limit set to the display's Nyquist rate is *not* provably invisible, because the
  eye is not a clean Nyquist sampler. That is precisely why "convert the aliasing into a
  well-distributed dither" beats "filter until it is below Nyquist."

**One new authoring rule falls out:**

- **Fog is not a cheat, it is the documented perceptual behaviour, and it should start early.** The
  EPA's *"within 10 percent of the visual range"* onset means contrast loss is measurable from
  ~3.9 km in clear air — right at the ground-level horizon. Minecraft's own docs call render distance
  *"also known as 'fog'"*. Use the two-term Narasimhan & Nayar model (`e^(−βd)` attenuation plus
  `L_∞(1 − e^(−βd))` airlight, with γ ≈ 0 for fog/dense haze so β is wavelength-independent),
  front-load the froxel/step distribution because *"most of the airlight… originates in portions of
  their cone of vision lying rather close to them"* (WMO §9.1.1), and let fog carry the LOD
  transition rather than fighting it.

**Gaps remaining after these passes:** Campbell & Green 1965's body text (PMC serves page scans only);
Middleton 1952 and the "350 km" attribution; Far Cry 5's terrain PDF; DCS's official graphics
settings; and the vernier-acuity threshold, where sources disagree by 2–5× (2–5 arcsec in the
clinical literature vs *"typically 10 seconds of arc"* in the Arizona notes) depending on contrast,
practice and configuration.
