# Grass in a ray-marched micro-voxel engine — web research (2026-09-07)

Provenance: produced by a read-only web-research subagent for the gameplay/vegetation prompt
(`Prompts/` folder), BEFORE any grass implementation. Every claim carries the URL it came from.
Stack context it was researched against: C++20, GPU fullscreen ray-march of a sparse-brick octree
(7.8 mm voxels near camera, `world/svo`), DiligentEngine, no textures (vertex-color materials),
baked AO, TAA, static analytic world, dual renderers (`--renderer svo|mesh`).

Companion local references: `research/tree-motion-growth-and-appearance.md` (tree/leaf physics —
sway, flutter, Vogel exponent, sunfleck 3–5 Hz band) and
`research/water-physics-and-wave-simulation.md` (wave/water physics). Goal: grass, wind, trees.

---

## 1. How blocky/voxel games render grass

### 1.1 Minecraft-style: cross-quads (crossed squares)
- Minecraft's tall grass/flowers/saplings use a dedicated block render type:
  `renderType == 1 → renderCrossedSquares(...)` — two quads crossed in an X inside the block
  cell, drawn with alpha-test (cutout), double-sided, with biome color multiplier applied
  per-vertex (`Block.tallGrass.colorMultiplier(...); this.drawCrossedSquares(Block.tallGrass, 2,
  x, y, z, 0.75F)`). Decompiled source:
  https://github.com/interactivenyc/Minecraft_SRC_MOD/blob/master/mcp811%201.6.4/src/minecraft/net/minecraft/src/RenderBlocks.java
- Minecraft PE's C++ renderer has the same concept as `Tile::SHAPE_CROSS_TEXTURE` →
  `tesselateCrossTexture`.
  https://gitea.sffempire.ru/Dram27/minecraft-pe-0.6.1/src/commit/0ee809daf1b11ff2a4b9b283d329099ec4a1ecdb/src/client/renderer/TileRenderer.cpp
- Rendering layers: grass goes through the alpha-test (cutout) layer, separate from opaque and
  translucent. https://mintlify.wiki/Minecraft-Community-Edition/client/architecture/rendering
- Because it's texture-based, Minecraft's cross-quad grass suffers mipmap darkening at
  transparent texel edges (documented bug MC-114265). Relevant warning for us: **we have no
  textures, so this entire class of alpha/mipmap artifacts vanish by construction.**
  https://mojira.dev/MC-114265

### 1.2 John Lin (tikli.li / madebyJohnLin / voxely.net) — GRASS AS VOXELS
Closest prior art to this engine. Verified facts:
- Press description of his 2020 sandbox: "a stunning forest in which you can count every
  individual blade of grass and leaf, **softly moving in the wind**, while raytraced rays of
  sunshine peak through the canopy" — i.e., grass is individual voxelized blades AND they are
  wind-animated. PC Gamer (Malindy Hetfeld, Nov 2020):
  https://www.pcgamer.com/john-lins-beautiful-physics-sandbox-gives-me-minecraft-vibes/
- His own video description ("New Voxel Engine Reveal - Crystal Islands Experiment", 2021): the
  engine has "a powerful asset pipeline that can utilize Quixel Megascans, PlantFactory/
  PlantCatalog's detailed vegetation, and other highly detailed polygon models **after being
  converted and processed into voxels**" and "Ray-traced world generation that could easily place
  trees, grass, flowers, and other assets... in sunlight, on cave walls, in big open areas". So:
  grass blades are **voxelized polygon assets** (from PlantFactory), placed procedurally by
  ray-tracing the world, rendered as voxels.
  https://www.youtube.com/watch?v=8ptH79R53c0
- His blog post "The Perfect Voxel Engine" (voxely.net, Sept 2021) gives the architecture
  philosophy but NOT the grass internals: use "whatever voxel format is best for the job", a
  general volume pipeline (Allocation, Tagging, Conversion), and lists "Generating voxel
  vegetation by converting 'seed data' into voxels" as a conversion use-case. Rendering is Vulkan
  ray tracing with per-format BLAS/intersection shaders and callable shaders for attribute
  decoding. He explicitly flags that vegetation needs per-voxel attributes only where vegetation
  exists ("We don't want to store vegetation growth state deep underground"). The promised
  follow-up post on the rendering architecture never appeared — **there is no public technical
  writeup of how his grass wind animation works on ray-traced voxel data.**
  https://voxely.net/blog/the-perfect-voxel-engine/ (blog index: https://voxely.net/blog/)
- Takeaway: John Lin proves voxelized grass at micro-voxel scale looks stunning and is
  achievable, but the animation mechanism is undocumented — we would be inventing it.

### 1.3 Teardown (Tuxedo Labs, "Ray Grid" style)
- No special grass system is documented. Everything is volumetric: "There are no triangles in
  this game... Everything is volumetric data, except for water surfaces and the power lines."
  Primary rays: rasterize each object's oriented bounding box, then march the object's voxel grid
  in the fragment shader ("for each such volume, it's just rasterized, a bounding box and then
  raymarched in a fragment shader to see which voxel you hit. And then there's a material lookup
  of indexed palettes for each object"). Voxels are ~10 cm; materials are an 8-bit palette with
  per-material properties (including a "foliage" physical material type). Game Developer
  interview: https://www.gamedeveloper.com/design/how-beautiful-voxels-laid-the-way-for-i-teardown-s-i-heist-y-framework
- Renderer details (community G-buffer breakdown + interviews): deferred renderer on OpenGL 3.3;
  world volume is a 1252×128×1252 texture with 8 voxels packed per byte and 3 mip levels for
  sparse tracing; a copy of the linear depth buffer is made during the G-buffer pass to early-out
  the expensive march ("this texture is used to implement a form of early-z culling in the
  fragment shader, allowing the fragment to be discarded... if the rasterized box is behind other
  geometry"); transparency (which grass-like foliage would need) is done by **dithered
  screen-door transparency + TAA** resolving it.
  https://juandiegomontoya.github.io/teardown_breakdown.html
- Later interviews confirm the pipeline: "the GPU marches through these voxel volumes along the
  ray to see where it hits something... we have several layers of acceleration structures"; light
  occlusion via a separate 1-bit-per-voxel bitmap ("hundreds of millions of voxels... super fast
  to just traverse the bits"). https://80.lv/articles/teardown-developer-breaks-down-multiplayer-and-voxel-destruction-tech
  and transcript: https://softwareengineeringdaily.com/wp-content/uploads/2024/12/SED1772-Teardown.txt
- Grass in Teardown is therefore: voxelized surface geometry + projected albedo/blend/normal maps
  ("Each texel maps to one voxel") + material palette. The map-projection approach is the one
  thing we can't reuse (no textures).

### 1.4 Atomontage
- Microvoxel platform ("sub-millimetre resolution" voxels streamed over the network). Grass/
  vegetation is handled by **voxelizing high-poly polygon assets** (ray-based and projection-based
  voxelizers bake texel colors into surface voxels); LOD is inherent to the voxel format ("LODs
  are inherently cheap with voxels and they are great for keeping the voxel size smaller than the
  size of a pixel on the screen"). Interview: https://80.lv/articles/how-oxels-became-the-next-big-thing ;
  https://gamesbeat.com/atomontage-launches-first-test-of-virtual-matter-voxel-based-technology/
  (Atomontage docs: https://atomontagedocs.netlify.app/api/VoxelRenderer)
- No documented wind/animation for voxelized vegetation.

### 1.5 Roblox voxel terrain (the best-documented hybrid)
ARM's GDC/Mali dev-conference PDF on Roblox's voxel terrain (sparse multiresolution voxels,
dual-contouring-ish mesher) documents that grass is NOT part of the voxel mesh — it's "clutter"
rendered on top:
- "Present: clutter. Experimented with card-based and geometric grass. **Geometric grass was
  noticeably faster on tilers.**"
- "**3-5 vertices per grass blade** (level of detail). Grass points placed using **vertex seeds
  (stable randomness)**. Very custom shading... wrap diffuse, more translucency hacks/math,
  height-based gradient for diffuse/specular."
- So: voxel terrain mesh + GPU-instanced geometric grass blades with vertex-seed placement. This
  is the most direct precedent for "voxel world + instanced grass overlay".
  PDF: https://developer.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/4_2D00_mmg2020_2D00_voxel_2D00_landscapes_2D00_arseny.pdf

### 1.6 Grass AS voxels — volumetric/procedural approaches (ray-march native)
- **42yeah "Raymarching Grass" (2023)**: grass field as a 2D Voronoi/Worley "SDF slice" extruded
  along height; render with a **fixed-step volumetric march begun only after the primary
  raymarch hits the ground** ("we can actually perform traditional raymarching first - up until
  the ray hits the tallest grass surface. Then we begin volume raymarching"); color accumulated
  with alpha blending over ~100 steps. **Wind by rotating the 3D noise sampling coordinate**: "by
  applying a small height-based rotation to the grass field function, we can emulate wind." This
  is the key prior art for wind in a no-mesh march.
  https://blog.42yeah.is/rendering/2023/03/25/grass.html
- **xbdev.net "Grass Ray-Marching" (WebGPU)**: grass as procedural 3D cell noise where dot size
  shrinks with height (blades taper to nothing at height 1.0); after a cheap analytic ray-plane
  hit, a short fixed-step march (400 steps at 0.01) checks whether the noise exceeds local
  height. Explicitly suggests "Add 'wind' so the grass wobbles - use the distortion effect"
  (offset the noise coordinates) and notes the technique maps onto any surface via the
  base-surface distance/normal. https://www.xbdev.net/internet/webgpu/?article=grassraymarching
- **Classic academic grounding — Neyret's multiscale volumetric textures (Kajiya-lineage)**:
  grass/fur modeled as density + local reflectance in voxels, stored in an octree with
  pre-filtered coarser levels so "a lawn on 1404 bilinear patches" with 16 blades per texel
  renders at cost linked to *apparent* complexity, not data complexity.
  http://evasion.imag.fr/~Fabrice.Neyret/publis/GI95.pdf
- **SVO memory warning (Laine & Karras, "Efficient Sparse Voxel Octrees")**: vegetation is the
  pathological content class for voxel memory — "leaves and grass blades are two- or perhaps only
  one-dimensional, but they (more or less) **fill the entire space until a certain scale is
  reached**", so total data grows sharply with view distance; their voxels with color+normal+
  contour cost ~5 bytes/voxel. If we store grass blades as bricks at 7.8 mm over any significant
  area, budget for this. https://users.aalto.fi/~laines9/publications/laine2010tr1_paper.pdf

### 1.7 MishMash / Voxel Hex and other devlogs
- **Voxel Hex** (MishMash95-adjacent; Ministry-of-Voxel-Affairs): a Rust/WGPU sparse voxel-brick
  tree ("leaf nodes contain voxel bricks... voxels of different resolutions can be mixed
  together") with GPU raytracing. Same architecture family as this engine. No grass-specific
  technique published in the repo. https://github.com/Ministry-of-Voxel-Affairs/VoxelHex
- A voxel devlog titled **"Grass, textures, and a new codecode [Voxel Devlog #22]"** (channel
  "Douglas", Feb 2025) exists — a Rust voxel engine that voxelizes assets and renders per-voxel
  normals + per-voxel textures; the retrievable transcript covers the codebase rewrite (sparse
  4×4×4-branching trees, SIMD region fill/copy, C API) rather than grass technique details, so
  treat it as evidence that hobby voxel engines are adding grass/texture passes, not as a
  technique source. https://www.youtube.com/watch?v=YTZBFz3Et40
- **Voxel Farm (Miguel Cepero)** — the most instructive negative result: in "Voxels in the wind"
  he animates grass/branches with overlapping sine waves and **explicitly admits no voxels were
  animated — "just traditional polygonal billboards"** with plans to feed wind vectors to the
  vertex shader. Even the most famous procedural voxel engine rasterized grass on top rather
  than animating voxels. http://procworld.blogspot.com/2012/09/voxels-in-wind.html
- **Nelari.us voxel raytracer devlog** — direct prior art for hybrid voxel pipelines: "A hybrid
  renderer is implemented... quads are generated for camera-facing brick faces, the brick quads
  are **rasterized into a depth buffer**, ray tracing uses the rasterized depth as the ray origin
  for DDA" — enabling "a billion voxels rendered in real time". (Inverse composition of what we
  would do, but proof the two pipelines compose.) https://nelari.us/post/voxel-ray-tracing/

## 2. Wind animation

### 2.1 Vertex-shader sine/noise wind (rasterized grass)
- **GPU Gems Ch. 7 "Rendering Countless Blades of Waving Grass"** (the foundational reference):
  trig-based (sine/cosine) animation of only the top vertices (detected via texture-coordinate
  v≈0), three variants: (a) CPU-computed cluster translation vectors (many draw calls — bad),
  (b) per-vertex wind in the VS (one draw call for the whole meadow, but blade edges distort and
  motion is homogeneous), (c) **per-object-center animation stored in the vertex format** — "each
  vertex must know the center position of its object... must be in the vertex format... because
  the vertex shader has to read this value" — few draw calls, no distortion, local chaos.
  Distance fade-in/out of grass objects.
  https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-7-rendering-countless-blades-waving-grass
- **Ghost of Tsushima (Sucker Punch)** — the production reference, with exact numbers:
  - Scene considers "just over 1 million blades of grass and renders about **83,000** of them,
    each individually animating with the wind, taking about **two and a half milliseconds end to
    end**" (PS4). GDC talk: https://www.youtube.com/watch?v=Ibe1JBF5i5Y ;
    vault: https://www.gdcvault.com/play/1027214/Advanced-Graphics-Summit-Procedural-Grass
  - **16 floats of per-blade instance data** (position 3, facing 2, wind strength at blade,
    per-blade position hash, type/clump/shape params). Blades generated by a compute shader per
    terrain tile (tiles have 512×512 textures = one texel per ~39 cm). Voronoi clumping drives
    height/direction/clump cohesion. LOD: **15 verts/blade high LOD, 7 verts low LOD**; low-LOD
    tiles are 2× the size with the same blade count; "the high LOD tiles LOD out **three out of
    every four** grass blades before they transition to the low LOD tiles"; shape-blending at
    the transition to avoid popping; for short grass the verts **fold to form two blades** (idea
    credited to YouTuber "Altera"). Double-buffered compute: 8 tiles of instance data, 4 in
    flight while 4 draw.
  - Wind system ("Blowing from the West"): "The main wind vector has a constant direction, but
    we varied the magnitude a bit from place to place using **time-varying Perlin noise**. This
    is visible on large fields of grass when you can see gusts of winds blow through." Plus
    curl-ish "vorticles", and "a TD displacement buffer around the camera which we write into
    using a special kind of particle; as a hero passes by, the blades will bend and tilt
    accordingly" for player interaction. Blades per frame drawn ≈ 100,000.
    https://www.youtube.com/watch?v=d61_o4CGQd8 ;
    https://www.gamedeveloper.com/design/using-vorticles-to-simulate-wind-in-i-ghost-of-tsushima-i- ;
    https://www.gdcvault.com/play/1027124/Blowing-from-the-West-Simulating
- **Simple layered formulation** (typical VS wind): `offset = windDir * ((primaryWave +
  turbulence) * strength * heightFactor)` where primaryWave = two sines of world XZ at different
  frequencies, turbulence = higher-frequency sine; multiply by height so roots stay planted.
  https://strata.game/shaders/vegetation/
- **Bezier-blade wind** (GoT-inspired reimplementations): blade spine = cubic Bezier with
  p0=root, p1=root+stiffness, p2=p1+wind*bend, p3=tip+windDisplacement; macro gusts from
  scrolling simplex noise (10–20 m waves) + micro flutter `sin(time*15 + phase*10)` with unique
  per-blade phase. https://github.com/Mithzzx/Project-GrassFlow/blob/main/README.md ; SimonDev's
  from-scratch implementation notes (≈1.5 ms GPU for millions of blades in-browser):
  https://www.youtube.com/watch?v=bp7REZBV4P4

### 2.2 Player interaction (displacement/bending) — three documented families, cheapest first
1. **Player-position sphere mask in the shader** — pass player position via a constant buffer,
   bend blades away within a radius, weighted by a height mask. Used widely; explicitly called
   "not revolutionary" but effective and cheap.
   https://www.youtube.com/watch?v=84BeP1sqNY0
2. **Render-target vector field** — orthographic camera under the ground renders "brush"
   particles to an RT; grass VS samples the RT and rotates the blade about its root. Scales to
   many actors, supports persistent trails via double buffering.
   https://www.kodeco.com/6314/creating-interactive-grass-in-unreal-engine-4
3. **GPU displacement buffer around the camera** (GoT's TD buffer) — a small texture centered on
   the player into which interaction particles write; grass samples it per blade.
   https://www.youtube.com/watch?v=d61_o4CGQd8

### 2.3 Wind when there is NO rasterized mesh (this engine's case)
- **Domain-warp the procedural grass field by time** — the only demonstrated fully-raymarched
  wind: rotate/offset the 3D noise sampling coordinate as a function of height and time (42yeah:
  "applying a small height-based rotation to the grass field function... emulate wind"; xbdev:
  "use the distortion effect"). Works because the grass is evaluated procedurally per-ray-step,
  not stored. https://blog.42yeah.is/rendering/2023/03/25/grass.html ;
  https://www.xbdev.net/internet/webgpu/?article=grassraymarching
- **Analytic bend in the march shader against STORED voxel grass**: no public prior art found.
  John Lin's voxelized grass visibly moves in wind (press-verified) but the mechanism is
  unpublished; his architecture (per-format RT intersection shaders) suggests the bend happens
  inside intersection/attribute decoding, i.e., the ray-hit position in grass material regions is
  displaced by an analytic wind function before attribute lookup. This is inference, not
  documentation — treat as uncharted territory.
- **Thin instanced overlay for the finest LOD ring** — the escape hatch everyone actually ships
  (Roblox clutter, Voxel Farm billboards): keep the volume static, rasterize animated blades near
  the camera.

## 3. Hybrid: ray-marched terrain + rasterized grass overlay (depth composition)

Well-trodden ground. Canonical pattern: **march pass writes SV_Depth/gl_FragDepth; raster passes
then execute afterwards with normal depth testing against it.**

- **raylib official "hybrid rendering" example** (closest published equivalent to this pipeline):
  a fullscreen quad raymarch shader writes per-pixel depth; a standard raster shader *also*
  writes depth ("You are required to write depth for all shaders if one shader does it"); order
  = raymarch first (fullscreen), then raster draws; they compose via the shared depth buffer
  with depth test enabled for both. Uses a framebuffer with a depth *texture* (not
  renderbuffer). C source:
  https://github.com/raysan5/raylib/blob/master/examples/shaders/shaders_hybrid_rendering.c
- **jimbo00000 "Raymarching + Rasterization"**: same recipe with the depth math spelled out —
  encode eye-hit Z into NDC depth via the projection's near/far and `gl_DepthRange`, write
  `gl_FragDepth`; "The raymarched scene renders first to color and depth buffers, then the
  rasterized scene renders into the same buffers."
  http://jimbo00000.github.io/opengl/portable/programming/vr/perspective/matrixmath/2016/02/15/raymarching-and-rasterization.html
- **Unity (bgolus, authoritative)**: "use `SV_Depth` to modify the output depth from the
  fragment shader. This has the advantage of not needing a camera depth texture, meaning it
  works with deferred rendering and mobile. However this means you have to transform your
  raytraced hit location... into a proper window depth value. It also only works with opaque
  objects." https://discussions.unity.com/t/raymarcher-with-depth-buffer/787930/4 ; uRaymarching
  does exactly this (`float depth : SV_Depth` in the out struct, "Use Raymarching Depth" toggle).
  https://github.com/hecomi/uRaymarching/blob/master/Documents/Legacy.md
- **Depth-unit mismatch gotcha**: the rasterizer stores view-plane-parallel depth; a ray marcher
  produces true ray distance. They compare correctly only after projection; if comparing
  manually: `rasterDepth = rayDepth * cos(A)` where cosA is the ray's forward component.
  https://computergraphics.stackexchange.com/questions/7674/how-to-align-ray-marching-on-top-of-traditional-3d-rasterization
- **Godot 4 case study**: sample scene depth and stop the march when opaque scene is closer; "If
  the SDF wins, the shader projects the hit point and writes its depth. On a miss it preserves
  the sampled scene depth, because Godot requires every branch to supply DEPTH once the shader
  writes it anywhere." Also flags: writing depth breaks transparency sorting.
  https://vav-labs.com/case-studies/sdf-raymarcher-godot/
- **Early-Z costs to budget for**:
  - Writing `SV_Depth` forces late-Z **for that draw only** — subsequent raster draws still get
    early-Z against the march-written depth. https://discussions.unity.com/t/general-questions-regarding-early-z/830595/1
  - So: march first (one fullscreen draw; it never had early-Z anyway), grass after — grass
    alpha-test (`discard`/`clip`) partially disables early-Z, which is why Unity HDRP **added a
    Z prepass specifically to make grass rendering efficient**.
  - Conservative depth (`SV_DepthLessEqual` etc.) preserves early-Z when you can promise the
    direction of the write. Cross-API table:
    https://github.com/gpuweb/gpuweb/blob/main/proposals/fragment-depth.md
  - Reverse composition (Teardown's way): rasterize proxy geometry FIRST, then march in the
    fragment shader against a copy of the depth buffer as an early-out. And Nelari.us
    rasterizes brick AABBs into a depth buffer and starts the DDA from the rasterized depth.
    https://nelari.us/post/voxel-ray-tracing/
- **Implication for this engine**: the svo march already writes `SV_Depth`
  (`svo_march.psh.hlsl` PSOutput.Depth). A grass overlay then needs zero special composition —
  one instanced alpha-tested draw with ordinary depth test against march depth. Wind +
  interaction live in the grass vertex shader. Main risks: (a) TAA ghosting where animated grass
  meets march-written depth (grass writes motion vectors, or rely on the existing history-clamp);
  (b) depth precision — write the march depth from the SAME projection convention the grass pass
  tests with.

## 4. Performance budget realities (exact numbers where given)

| Source | Blade count | Cost | Per-blade data | Geometry | Notes |
|---|---|---|---|---|---|
| Ghost of Tsushima (PS4) | 1M+ considered, **~83K drawn/frame** | **2.5 ms end-to-end** | **16 floats (64 B)** | 15 verts high / 7 verts low LOD | 4× fewer blades at low LOD; compute double-buffered. https://www.youtube.com/watch?v=Ibe1JBF5i5Y |
| GoT wind talk | "millions" in scene, **~100K drawn** | "small fraction of PS4" GPU | — | — | https://www.youtube.com/watch?v=d61_o4CGQd8 |
| Roblox (ARM) | clutter over voxel terrain | geometric faster than card-based (tilers) | vertex seeds (stable) | **3–5 verts/blade** | https://developer.arm.com/.../4_2D00_mmg2020_2D00_voxel_2D00_landscapes_2D00_arseny.pdf |
| Project-GrassFlow (Unity 6, M2 GPU) | 100K / 500K / 1M / 2M / 5M | 2.1 / 3.8 / **6.2** / 11.5 / 28 ms | **32 B/blade** (1M = 32 MB) | LOD0 5 segs, LOD1 3, LOD2 1 | 3–5 indirect draws. https://github.com/Mithzzx/Project-GrassFlow |
| Jahrmann & Wimmer 2017 | 397,881-blade scene | real-time | — | tessellated Bezier strips | culls ~75% (frustum > distance > orientation). https://www.cg.tuwien.ac.at/research/publications/2017/JAHRMANN-2017-RRTG/JAHRMANN-2017-RRTG-draft.pdf |
| Acerola (GTX 1660) | 1.2M billboard objects, 2.16M tris/frame | 500 fps billboard; ~100 fps @ 7.3M blades | **7 floats = 28 B/blade**; 7.3M = **204 MB/buffer** | 12 verts/6 tris per tuft | chunking 5×5; single-triangle far LOD: 64→82 fps. https://www.youtube.com/watch?v=Y0Ko0kvwfgA ; https://www.youtube.com/watch?v=PNvlqsXdQic |
| Vulkan grass renderer (RTX 5080) | 8K→1M blades | no-cull 10.7 fps @1M; all-cull 46.2 fps | — | tessellation 10→2 | **distance culling alone = 1.45–3.98×**; recommends 32K–131K blades production. https://github.com/tonytgrt/Vulkan-Grass-Renderer |
| thegeeko (RX 5600XT) | 6.77M visible | compute 4.7 ms + draw 8 ms | — | triangle-strip blades (meshopt) | 19M blades over 1 km²: 2.5 + 6 ms. https://thegeeko.me/blog/foliage-rendering/ |

Common LOD/culling strategy (consensus across GoT, Acerola, Jahrmann, Vulkan renderers):
1. **Chunk/tile the field** — culling unit and LOD switch unit.
2. **Compute-shader culling per frame**: frustum → distance → orientation (edge-on blades
   culled) → occlusion.
3. **Probabilistic distance falloff**: stable per-blade position hash decides survival per
   distance bucket — "prevents temporal flickering while creating smooth density falloff".
4. **LOD = fewer verts per blade** with shape-blending at the transition.
5. **Fog / distance fade** hides the cull boundary.

For a *ray-marched* engine the analog of 1–3 collapses into "only place grass in the full-res
LOD ring" — the octree already does distance culling by construction.

## 5. Ranked recommendations for THIS engine

### #1 (best fit): Thin instanced raster overlay for the finest LOD ring, composed via the march's depth buffer
Only approach with shipped-engine-proven wind + player interaction (GoT: 2.5 ms for 83K
animated blades; Roblox does exactly this over voxel terrain; Voxel Farm fell back to it too).
Composition with our march is solved prior art — raylib's hybrid example is literally our
pipeline shape: march writes SV_Depth, one instanced alpha-tested grass draw follows with
ordinary depth test; early-Z applies to the grass pass against march depth. No textures means
blades are pure vertex-color (3–7 verts). Budget: 30–80 K blades in a ~15–30 m ring at 3–7
verts = under 1 ms; per-blade data 16–32 B; wind = layered sine/scrolling-noise in the VS keyed
on world position; player interaction = player position/velocity in a constant buffer + sphere
mask. Terrain-side memory cost: zero. Risks: a raster path enters an otherwise pure-march
renderer (new PSO, TAA motion handling); grass is invisible to march-derived effects (baked AO,
the march's shadow ray) unless added; blades must sit exactly on the march surface.

### #2: Grass AS voxels in the SVO, finest levels only (John Lin / Atomontage approach)
Perfect stylistic consistency — voxelized blades inherit vertex-color materials, baked AO, the
grain, and the march's shadows automatically; John Lin demonstrated it's gorgeous at micro-voxel
scale; placement is trivial with the existing tree-builder machinery (voxelize small blade
clusters, tag grass material, only within the full-res radius so LOD does the culling). No new
pipeline at all. BUT: wind is the killer — no public prior art for animating stored ray-marched
voxels (John Lin's mechanism unpublished; Voxel Farm explicitly bailed to billboards; Atomontage
doesn't document it). Plausible routes: (a) analytic domain-warp in the march — displace the
ray's sample position inside grass-material bricks by a height-and-time wind function before
brick lookup, which risks breaking DDA coherence at brick boundaries and fights baked AO; or (b)
keep blades static and fake life with per-blade normal/color shimmer. Plus the documented memory
cliff (Laine & Karras: vegetation is the worst-case SVO content class), so 7.8 mm grass bricks
over even a 30 m ring is a real brick-count multiplier on a tree build that is already the
frame-cost driver. Viable if we accept static or shimmer-only grass, or ship it as the distant/
medium representation under overlay #1.

### #3: Procedural grass layer inside the march (42yeah / xbdev style secondary march)
Zero memory, zero new pipeline, no textures needed by construction. Prior art demonstrates the
exact recipe for a march-first engine: after the primary march hits ground with grass material,
run a short fixed-step secondary march (tens of steps, bounded by grass height) through a
procedural blade field; wind = time-varying rotation/offset of the noise sampling coordinate
(the only *demonstrated* wind-without-mesh technique). Distant grass degrades to just tinted
ground. Weaknesses: per-pixel cost stacks onto the measured 3.2–6.3 ms march (42yeah needed
~100 steps); documented results are stylized/flat; composes poorly with baked AO + per-cube
grain; soft silhouettes (no true blade edges), which TAA will smear. Best as a detail garnish
on top of #2's static voxel grass, or the cheap distant layer under #1.

**Recommended combination for the John Lin look with wind**: #2 for structure (static voxelized
blades in the near ring, inheriting AO/shadows) + #1's overlay only within a few meters of the
camera where wind and player interaction are actually perceptible + a very cheap #3-style noise
shimmer (no secondary march — just warping the *shading* of grass-material hits by time) for
distant life. This is essentially what Roblox ships (voxel terrain + geometric clutter), scaled
to micro-voxel fidelity.
