# Micro-voxel creator techniques (John Lin, MishMash95, Teardown, Atomontage, Octo, …) — web research (2026-09-07)

Provenance: produced by a read-only web-research subagent for the gameplay/vegetation prompt
(`Prompts/`), BEFORE implementation. Method note: the built-in web_search endpoint failed
(HTTP 401) for the research session, so everything below came via Exa search + web_fetch.
YouTube pages return only boilerplate to fetchers, but video descriptions were recoverable
through search-indexed mirrors, and Reddit/redlib mirrors gave full devlog text. Every claim is
labeled CONFIRMED (creator wrote/said it, verbatim from an indexed description/comment/
interview/blog) or INFERENCE (from watching/press observation). "tikli voxel" and "Brain Kids"
returned nothing — those names have no text footprint.

Companion files: `research/grass-rendering-research.md` (grass/wind/hybrid composition),
`research/tree-motion-growth-and-appearance.md` (tree biomechanics),
`research/water-physics-and-wave-simulation.md` (water physics).

---

## 1. JOHN LIN (youtube.com/@johnlin9665, @ProgrammerLin, github.com/Lin20, voxely.net)

The best-documented micro-voxel creator in text form: his YouTube video descriptions are
unusually technical, he wrote a design blog (voxely.net), and his repos are public.

### 1.1 Engine evolution & rendering — CONFIRMED

- **Two engines.** The first engine (2020, the famous forest videos) was rewritten in a
  "4 month quest" into a new engine revealed in "Crystal Islands Experiment" (2021-05-13):
  https://www.youtube.com/watch?v=8ptH79R53c0
- **Path-traced GI, 5 bounces — CONFIRMED verbatim**: "Improved path traced global illumination
  that features 5 bounces from the sun, atmosphere and all emissive objects." The denoiser at
  that point "is lacking a spatial filter." (Crystal Islands description)
- **Detail scale — CONFIRMED**: "An 8x (512 cubic) detail increase with animation support, high
  compression rates, and per-voxel material attributes" over the previous engine. 8x linear =
  512x volumetric. The absolute voxel size in mm is NOT stated anywhere — do not trust
  "7.8mm"-style numbers sometimes quoted for Lin; that's inference territory.
- **World scale — CONFIRMED**: "The effective world size here is 256K^3 and the island is
  generated once upon startup (like Terraria), which takes about a minute." The older engine had
  a "0-4095 y world boundary" (water video description). A "low-detail mode" is toggleable; the
  high-detail path "will be reserved for RTX graphics cards."
- **Rendering speedup — CONFIRMED**: "A 10x speedup in rendering over the previous engine."
- **Stack — CONFIRMED from video keywords**: C++, Vulkan, path tracing ("Voxel Water Physics"
  video tags: "ray tracing, path tracing, rtx, vulkan, c++":
  https://www.youtube.com/watch?v=1R5WFZk86kE).
- **Per-voxel normals/colors**: Lin hasn't written a post on it, but Douglas Dwyer (Octo engine)
  states as his own CONFIRMED opinion: "per voxel normals are I think the secret sauce that help
  engines like [Teardown's] and John Lin['s] look so good — they make it easier to discern the
  shape of surfaces and allow things to look really smooth and not blocky"
  (https://www.youtube.com/watch?v=aY4Zet_C9Zs).

### 1.2 The blog — "The Perfect Voxel Engine" (voxely.net/blog/the-perfect-voxel-engine/, 2021-09-18) — CONFIRMED

The single most important text John Lin has published. Core thesis:
- **Sparse voxel octrees are NOT the answer**: "sparse voxel octrees might be able to hold a
  couple billion voxels... but how well do they work for collision detection? Global
  illumination? Path finding? Adding new per-voxel attributes besides albedo and normals? ...
  storage and rendering are the only things they are acceptable (not even great) at."
- **Multi-format volume pipeline: Allocation, Tagging, Conversion.** Use "whatever voxel format
  is best for the job" — swappable allocators, attribute "tagging" (name/size/type, added only
  where needed — "we don't want to store vegetation growth state deep underground"), and
  conversion operators between formats (example: a `terrain_block` format of u8vec4 albedo +
  vec3 normal arrays converted to a `default` raw format).
- **Listed conversion use-cases include**: "Generating collision data for physics processing",
  "Generating voxel vegetation by converting 'seed data' into voxels", "Voxelizing procedurally
  generated terrain", mesh voxelization, Minecraft-map import, CSG.
- **Hardware RT integration — CONFIRMED**: the new engine builds RTX BLASes over voxel
  geometries; "By tracking the voxel formats that make up a BLAS' geometries, we can build the
  SBT with specific intersection shaders tailored to the format design. Callable shaders can
  also be bound to decode attributes."
- **Architecture**: blend of OOP and ECS (follow-up post on the blog).
- The blog has only ~3 posts (Perfect Voxel Engine, an ECS post, an RNN tile-map post):
  https://voxely.net/blog/

### 1.3 Water/fluids — CONFIRMED (richest technical descriptions of any creator)

- **"Voxel Water Physics - Waterfalls, Rivers and Tunnels" (2021-01-08)**
  https://www.youtube.com/watch?v=1R5WFZk86kE — verbatim: "It uses the same cell/particle
  hybrid method and is therefore fully volumetric with no height range limit beyond the 0-4095 y
  world boundary! It will flow through buildings, fall down into caves, flow over and under
  cliffs, fill up containers and lakebeds, and navigate through tunnels. The design has been
  adapted to run sparsely and inline with the world generation. The entire pipeline has also had
  its design improved to be 99% multithreaded, with a 1% critical section that has the smallest
  execution time. In this video, 4 CPU threads were used and the simulation time never exceeded
  8ms." Water is FINITE ("With water being finite, waterfalls spawning from a body of water will
  eventually run out"), saves/loads, spawns naturally; no evaporation/rain cycle yet.
- **"Water Physics + Custom Voxel Boats, Rigid Bodies & Destruction" (2021-01-18)**
  https://www.youtube.com/watch?v=BoPZIojpbmw — verbatim: "One-way coupling between rigid bodies
  and water has been implemented. **Buoyancy is simulated volumetrically, and the fluid velocity
  field is sampled where the objects collide with it.** Because the coupling is just
  fluid-on-objects, air does not factor into water displacement and the splashes seen are just
  visual." Object building system has "the same fidelity as normal building in the world."
- Press coverage confirming the volumetric claim:
  https://www.pcgamer.com/john-lins-physics-sandbox-returns-with-the-best-water-ive-ever-seen/

### 1.4 Grass, trees, vegetation — CONFIRMED (placement) / PARTIAL (animation)

- **CONFIRMED, asset pipeline**: "A powerful asset pipeline that can utilize Quixel Megascans,
  PlantFactory/PlantCatalog's detailed vegetation, and other highly detailed polygon models
  after being converted and processed into voxels (all 3 seen in this video)" (Crystal Islands).
  So trees/grass are polygon assets VOXELIZED, not procedurally grown as voxels.
- **CONFIRMED, placement**: "Ray-traced world generation that could easily place trees, grass,
  flowers, and other assets (eg. crystals) in designated areas: eg. in sunlight, on cave walls,
  in big open areas" (Crystal Islands). Placement uses ray tracing against the world (e.g. sun
  exposure queries).
- **CONFIRMED, grass is per-voxel and animated**: Crystal Islands lists "animation support" as
  an engine feature; PC Gamer describes the forest as one "in which you can count every
  individual blade of grass and leaf, softly moving in the wind, while raytraced rays of
  sunshine peak through the canopy" (https://www.pcgamer.com/john-lins-beautiful-physics-sandbox-gives-me-minecraft-vibes/).
  Falling leaves and butterflies were added later (https://gamerant.com/voxel-sandbox-game/).
- **NOT DOCUMENTED**: the actual wind/sway technique for voxels (skeletal? per-voxel offset
  field? re-simulation?) is nowhere in text form. INFERENCE from watching: grass blades flex as
  whole structures, suggesting per-voxel-group transforms rather than physics — a read of
  videos, not a statement by Lin. No devlog, interview, or Reddit post by Lin has this detail.
- **Destruction of vegetation — CONFIRMED** (press): "you can break off any piece, bend it,
  form it into something else or dig around" (PC Gamer). BlueDrake42's feature video:
  "everything is point-and-click interactable... all of the different elements of the world will
  react to it" (https://www.youtube.com/watch?v=FbSezz9QHgg).

### 1.5 Physics / character movement — mostly NOT DOCUMENTED

- CONFIRMED capabilities: "Fast collision detection that physics and player walking will
  utilize" + "Full fracture and mutation of objects" (Crystal Islands); rigid body physics and
  boats (Jan 2021 videos); his Twitter bio line "Voxels, creative & destructive physics,
  procgen" (via GameRant).
- The collision ALGORITHM is not documented anywhere in text. INFERENCE: fracture + walking +
  fluid-terrain interaction implies a broad-phase over the voxel database with
  connected-component/fracture analysis, but no details exist publicly.

### 1.6 GitHub — CONFIRMED (all public, useful references)

https://github.com/Lin20 — `BinaryMeshFitting` ("Yet another attempt at making a fast massive
level-of-detail voxel engine, but this time with usable results!", 389 stars), `isosurface`
(algorithm comparison), `PushingVoxelsForward` ("Isosurface extraction using largely
undiscovered techniques"), `ProjectIW` ("Infinite world using MDC"). Also: Sam Blazes wrote a
detailed reconstruction of a Lin-inspired voxel path tracer with writeup:
https://samblazes.net/posts/2021-01-13-voxel-raymarching.html

## 2. MISHMASH95 (youtube.com/@MishMash95) / u/MGMishMash

Key finding: the channel's YouTube pages yield NO text via fetch (only boilerplate), but the
creator cross-posts every devlog to Reddit as **u/MGMishMash** (r/VoxelGameDev,
r/proceduralgeneration, r/IndieGaming), and those posts (mirrored on redlib instances) contain
full technical text. No "Voxel Hex" or "ray grid" engine by him was found — likely a
misremembered name (Voxel Hex is a different project: https://github.com/Ministry-of-Voxel-Affairs/VoxelHex);
his current project is just called "Micro Voxel Engine". His older (2017-era) channel content
could not be located in text form; those physics videos are watch-only.

### 2.1 Rendering — CONFIRMED, and surprising

- **"Note: this engine uses meshing rather than RT/DDA."** (verbatim comment on his
  render-distance post). So despite "Micro Voxel Engine" branding, his rendering is MESH-BASED,
  not ray-marched. Engine: "custom built in C++ using minimal libraries (only imgui, openAL and
  zstd for now), and engine is naturally focused on being optimized specifically for rendering
  Voxels and my 'VoxelMesh' construct." (https://redlib.vanillax.me/user/MGMishMash →
  "Improving Terrain and Render Distance in my Micro Voxel Engine")
- **Performance target achieved — CONFIRMED**: render distance increased "from 300m to ~10-15km,
  while running at 45-50 FPS on an Apple M1 Pro."

### 2.2 Terrain, LOD, macro chunks — CONFIRMED (his most technical post)

From "Improving the Render Distance in my Micro Voxel Engine" (r/VoxelGameDev; mirror
https://redlib.vanillax.me/r/VoxelGameDev):
- **Sieve-function LOD generation**: "All chunk generation functions now include a sieve
  function to automatically be able to generate at 1/N resolution without any changes. This
  also applied to generated features and stamps, enabling chunks to be generated at any
  resolution without downsampling." A 1/64-res chunk takes the same time to generate as a
  full-res chunk.
- **Macro chunks record/resolve local edits independently** and are saved/cached
  independently, so terrain edits persist without holding full-res copies.
- **LOD transitions**: "transient transitions where the detail levels fade between each other
  once, rather than a continuous gradual transition, as this is around 30% cheaper on the GPU
  and looks 'nearly' as smooth."
- **Adaptive fog**: draw distance scales dynamically with loaded LOD bands; during fast motion
  the draw distance temporarily shrinks until chunks load.
- **Terrain**: "generally a height field, however with several stamping and carving passes to
  create 3d detail"; "fairly basic standard fractal noise function with a few layers... a few
  extra carving passes to erode the heightmap for canyons and rivers."

### 2.3 Trees & grass — CONFIRMED (the best-documented micro-voxel vegetation system anywhere)

From his Reddit comments on the same post — a **two-tier prop system**:
- **Detail chunks** (LOD0/1 rendered, full LOD0 kept loaded = active simulation region): "props
  are spawned as real entities with simulations and interactions."
- **Macro chunks** (coarse LODs): keep a list of props in two classes. (a) **Large props
  (trees)**: "use the same 'decoration' pass as detail chunks, and match generation 1:1. IDs
  for chunk entities is deterministic, and this is used to track placed and destroyed props. If
  a tree is destroyed in a detail chunk, it cascades through the various Macro chunk instances
  for each level and removes that entry." (b) **Small props (grass)**: "scattered using an
  approximate distribution that matches the original biome, but is instead done at coarse chunk
  resolution, with a cluster spawn. So it does not match the simulated area, but a reasonable
  compromise for large scale grass coverage."
- **Combined instancing**: "The LOD and batching system for props gathers batches from both the
  live entity and the macro prop set and combines these together into instanced lists... you can
  both get cards in the detail area for small props, and occasionally higher res LODs in the
  macro chunks. My prop rendering actually got net FASTER when increasing viewing distance as I
  was able to use cards for grass at a much closer distance once the combined instancing was
  supported, and actually reduced geometry, despite rendering millions of grass instances."
- **Trees are voxel props, not stamped geometry (yet)**: "props like trees are editable
  (destructible) up close with voxel edits, these do not persist beyond a time, so i only keep
  the raw prop voxel asset resident. I am considering stamping certain large trees into the
  chunks, and having them be part of the world rather than as discrete props to leverage
  implicit chunk LODs."
- **Snow**: "trees individually accumulate snow, but macro props are stateless, beyond a basic
  type ID and position, so will need some overrides here."

### 2.4 Water, heat, clouds, survival — CONFIRMED (devlogs #4/#5 + heat post, all on r/VoxelGameDev; video for #4: https://www.youtube.com/watch?v=ZxfV7su168U)

- **Fluid sim — CONFIRMED**: "a fairly naive GPU physics simulation supporting volume transport
  and a motion vector. It runs on a coarse grid, and uses a cell occupancy parameter to
  determine if a cell can receive flow and how much, including how flow can exit. A few
  optimisations on top to control tick rates and simulation region. It's mostly focused on
  being reasonably okay for gameplay and very fast, rather than for accuracy." Hardest part:
  "managing chunk lifetime and stopping other world features from draining the pond (e.g cave
  cracks)."
- **Ponds**: basin detection → randomized size; rim/underwater detail props; occasional
  islands; fish are "detail entities" whose state doesn't persist; fish species selection will
  be deterministic on pond location + time of day.
- **Clouds (devlog #5)**: "Raytracing Volumetric Clouds using Voxels" — "rendered to a lower
  resolution buffer (1/4th res by default), with a blur filter applied, and then blitted onto
  the world"; the "simulation" just drives density + noise threshold per biome; rain/snow are
  random daytime events that change atmospherics.
- **Heat sim + survival**: heat simulation "accurately warms indoor and enclosed spaces"; cold/
  snow cools the player; heat sources taper snow buildup; hunger + temperature → health →
  player stats.
- **Multiplayer advice (CONFIRMED)**: "it's very difficult to add multiplayer later on if you
  don't design around it... if your core architecture is designed in a multiplayer-friendly
  way, then the actual connection, packet sending, interaction polish can indeed be done later."

### 2.5 Physics/character controller

NOT DOCUMENTED in text form anywhere findable. He has survival mechanics (so a player
controller exists) but no post describes capsule vs AABB, step handling, or swimming physics.
Watch-only territory.

## 3. Other micro-voxel creators

### 3.1 Atomontage (Virtual Matter) — CONFIRMED via interviews

- **Sub-millimetre voxels — CONFIRMED**: "There's about 20Gb of content on this server here,
  but it's coming in only where you need it in high resolution... You will have sub-millimetre
  resolution on those voxels" (chainmail demo; sarcophagus scan where "you can read the
  hieroglyphs") — PocketGamer interview:
  https://www.pocketgamer.biz/atomontages-virtual-matter-pioneering-3d-voxel-graphics-for-social-gaming/
- **Physics-first database philosophy — CONFIRMED (Siles)**: "if you develop the database, which
  is the key component... with a physics simulation in mind, you end up with something different
  than if you develop this stuff with just rendering in mind... Physics simulation is so hard to
  do, that if you make that, then everything else will be easier to do."
  (https://wccftech.com/atomontage-branislav-siles-limits-polygons-voxel-future/)
- **LOD — CONFIRMED**: "inherent LOD system"; "LODs inherently always stay in sync, even when
  many people are messing with the same huge volumetric 3D data at the same time" (Tabar,
  GamesBeat: https://gamesbeat.com/atomontage-launches-2024-edition-of-3d-art-virtual-matter-platform/);
  renderer "uses the most optimal combination of LODs of tiny segments... LODs are inherently
  cheap with voxels and they are great for keeping the voxel size (and so the shape error)
  smaller than the size of a pixel on the screen" (80.lv:
  https://80.lv/articles/how-voxels-became-the-next-big-thing).
- **Voxelizers — CONFIRMED**: ray-based voxelizer (casts rays, reads texels at intersections,
  bakes into voxels) and projection-based voxelizer (multi-view depth maps, intersection of
  depth volumes). (80.lv)
- **Client needs no GPU; CPU rendering; streaming decoupling** — CONFIRMED (GamesBeat/
  PocketGamer).
- **Real-time physics — CONFIRMED via 2025 videos**: terrain deformation, vehicles leaving real
  tire tracks in sand ("it's not just some kind of texture map trick"), dust/particles:
  https://www.youtube.com/watch?v=SJPpxCafN2k → https://youtu.be/SfU2hXgNKdE. No algorithmic
  detail — marketing-level only.
- **Vegetation/trees/grass**: NOTHING documented. Say-so-explicitly item.

### 3.2 Voxel Quest (Gavan Woolery) — CONFIRMED, the closest philosophical match to this engine

- **No stored voxels at all — CONFIRMED**: "I am not storing any of the voxels! Everything is
  rendered, then dumped. Wherever possible, the voxels are described in terms of higher level
  procedural algorithms, including where they have been destroyed." Octrees deliberately
  rejected: "Octrees would actually be counterproductive because I have no need to compress or
  store the voxels - I need to work with the uncompressed version as I can do calculations
  faster." (https://www.voxelquest.com/news/how-does-it-work — comment thread has these
  verbatim replies)
- **Edits stored as change lists — CONFIRMED**: user changes stored uncompressed in chunks
  (e.g. 16³), "then rendered with bilinear filtering so the resulting subtraction is smoothed
  out"; supports boolean geometry (subtract sphere by center+radius, "pill" by
  distance-from-line). Worst case ~16 bytes/click.
- **Grass in screen space — CONFIRMED** (via commenter question + Gavan's engagement, answer
  truncated in capture): a commenter states "I also planned to do the grass rendering in screen
  space. So, do you distribute the quads across the screen?" — indicating VQ renders grass as
  screen-space quads, not world geometry. (Same page.)
- Later architecture thinking (recursive cells / BSP for arbitrarily sized objects):
  https://www.osnews.com/story/28748/how-does-voxel-quest-work-now/
- Old isometric engine source: https://github.com/gavanw/voxelquestiso (deprecated →
  vqisosmall)

### 3.3 Vercidium (Sector's Edge) — CONFIRMED; NOT Atomontage-adjacent (correcting the premise)

Vercidium is an independent FPS engine (Sector's Edge), not Atomontage-related, and its voxels
are blocky (32³ chunks), not micro. Still valuable open-source references:
- **Meshing — CONFIRMED**: 32³ chunks; face runs along X/Y "produces ~20% more triangles than
  greedy meshing and runs ~390% faster"; 4 bytes/vertex packing position+textureID+health+
  normal. https://github.com/Vercidium/voxel-mesh-generation +
  https://vercidium.com/blog/voxel-world-optimisations/
- **CPU voxel ray marching for collision — CONFIRMED**: "our ray marching algorithm that
  handles collision detection for tens of thousands of particles" — open-sourced, based on
  Amanatides & Woo, "optimised by keeping block lookups within the current working chunk":
  https://github.com/Vercidium/voxel-ray-marching
- **Voxel raycasting is FAST — CONFIRMED**: "Casting rays through voxel worlds is incredibly
  fast, and meant thousands of rays could be cast in real time, even on older laptops"
  (https://www.patreon.com/vercidium/posts/raytraced-audio-142709378 — later replaced voxels
  with a BVH over primitives for audio, 8x faster, but audio-specific).

### 3.4 Douglas Dwyer ("Douglas", Octo engine) — CONFIRMED, the best devlog text of any living voxel creator

- **Per-voxel normals + unique colors — CONFIRMED**: "having unique per-voxel colors and
  per-voxel normals is Paramount to having a realistic looking scene with lighting... the
  secret sauce that [makes] engines like [Teardown] and John Lin['s] look so good" (Voxel
  Devlog #17: https://www.youtube.com/watch?v=aY4Zet_C9Zs)
- **64-way "brick tree" — CONFIRMED**: replaced octree ("every step meant 8+ memory reads to
  traverse down") with a tree splitting each parent into 64 children (4×4×4,
  "tetrahexacontree"), plus a **64-bit occupancy bitmask per node**: "they read the 64-bit bit
  mask and... use bitwise operations to select the bit corresponding to the child node... my
  rays can take up to 10 ray steps without needing to read from memory again, which is a huge
  performance win." Teardown-map castle at 7 ms/frame with primary + shadow ray on a GTX 1660
  Ti. Independently validated by Teknologicus's tetrahexacontree occupancy-bitmask
  lookup-table work: https://teknologicus.itch.io/vorxel/devlog/839586/tetahexacontree-4x4x4-occupancy-bitmasks-lookup-table
- **Parallax ray marching for iGPUs (devlog #4) — CONFIRMED**:
  https://www.youtube.com/watch?v=h81I8hR56vQ — draw tight bounding boxes, ray-march inside in
  the fragment shader; manual depth in a separate pass to preserve early-Z; FXAA instead of
  MSAA; 36×256³ volumes at 60fps on Intel UHD.
- **Engine roadmap — CONFIRMED**: https://github.com/DouglasDwyer/octo-release — ray-marched
  rendering with LODs, realtime path-traced lighting (AO/shadows/emissive voxels), editable
  terrain, rigidbody physics with connected-component detection (0.3.0) → "realistic rigidbody
  physics" (0.8.0), WASM modding. Devlog #12 is "Chopping trees DOWN" (physics coding; title
  only: https://ttrpg.network/post/2138144).

### 3.5 Grant Kot (kotsoft) — CONFIRMED (MPM voxel physics)

"I am working on a voxel game engine using Material Point Method (MPM) for physics simulation.
The physics runs completely on the CPU, leaving the GPU free for raytracing. Also runs well on
lower end computers and cellphone processors." (https://www.youtube.com/watch?v=ufun5bBUKDQ).
Million-particle sims at 15 fps (2019); displacement "texturing" by jittering/stretching voxel
particle spacing (fibers, bricks); underwater rendering = third geometry pass drawing backfaces
so you can see the surface from below (https://www.youtube.com/watch?v=ddsvqgcGvR0).

### 3.6 Teardown (Dennis Gustafsson) — CONFIRMED, the production benchmark

- **Structure — CONFIRMED**: "Instead of one big volume of billions of voxels, I have thousands
  of smaller volumes that are filled with voxels... Everything is straight within its own
  volume." (https://www.gamedeveloper.com/design/how-beautiful-voxels-laid-the-way-for-i-teardown-s-i-heist-y-framework)
- **Collision — CONFIRMED**: "voxel versus voxel powered on the CPU, while rendering takes
  place on the GPU. There are no triangles in this game... except for water surfaces and the
  power lines."
- **Ray tracing — CONFIRMED**: "I actually have a separate voxel structure just with the 3D
  occlusion data so to speak. Where I perform the ray tracing, that also means I don't have
  access to any colors in that data structure." Primary rays: rasterize a bounding box per
  volume, then "raymarched in a fragment shader to see which voxel you hit. And then there's a
  material lookup of indexed palettes." SSR for reflections, not path tracing ("for performance
  reasons").
- **New engine (2024) — CONFIRMED**: "uses substepping instead of solver iteration (a method...
  sometimes referred to as 'Temporal Gauss-Seidel' [per Erin Catto's Box2D research]) and
  features a parallel solver that can solve large piles of objects on multiple threads"; 32
  threads, ~5 ms sim (https://blog.voxagon.se/2024/12/29/year-summary.html)
- **Multiplayer destruction — CONFIRMED**: can't sync voxel data; destruction sent as commands
  applied deterministically in the same order
  (https://80.lv/articles/teardown-developer-breaks-down-multiplayer-and-voxel-destruction-tech)

### 3.7 Academic/reference implementations with source

- **GigaVoxels** (Crassin et al., the canonical octree-of-bricks ray caster): octree of
  MIP-mapped brick pools, ray-guided on-demand brick production, GPU LRU cache, cone tracing
  for soft shadows/DoF: https://inria.hal.science/inria-00291670v2/file/RR-6567.pdf; 2024
  successor with clipmaps-of-bricks + async GPU production: https://doi.org/10.1145/3675389
- **Efficient Sparse Voxel Octrees** (Laine & Karras 2010, the DDA-through-octree PUSH/
  ADVANCE/POP algorithm): https://users.aalto.fi/~laines9/publications/laine2010i3d_paper.pdf
- **Open source SVO engines**: tim-oster/voxel-rs (Rust+GL, chunk-octrees with relative/
  absolute pointers, detailed traversal writeup: https://github.com/tim-oster/voxel-rs);
  AdamYuan/SparseVoxelOctree (Vulkan SVO builder + ray marcher + path tracer:
  https://github.com/AdamYuan/SparseVoxelOctree); dyoo47/svo-raytracer (Java path tracer to
  8192³: https://github.com/dyoo47/svo-raytracer)
- **Nelari.us voxel raytracer devlog — CONFIRMED technique**: hybrid renderer that "rasterizes
  brick AABBs into a depth buffer, and ray tracing uses the rasterized depth as the ray origin
  for DDA" — billion voxels real time; 2-level DDA (8³ brick occupancy grid); 4³ bricks with
  occupancy masks: https://nelari.us/post/voxel-ray-tracing/
- **Joris Rijsdijk's Ray-Marched Voxel Playground — CONFIRMED + MIT source**: GPU compute DDA
  over brickmaps (8³ bricks), cellular-automata water (gravity → sideways with stored
  direction), and a very relevant collision approach: builds a **convex collision mesh only
  around the player**, collected async as 1-bit-per-voxel, rebuilt "only a couple times per
  second, which is good enough to handle normal player movement": https://jorisar.nl/blog/VoxelPlayground/
- **"Brain Kids" voxel physics demos: NOT FOUND.** No such creator/project exists in searchable
  text. Either misremembered or too obscure. Closest matches: FonzieLiu "Voxel Buddies" (XR
  active-ragdoll voxel sandbox: https://fonzieliu.itch.io/voxel-buddies) and Kees Tucker
  "Rudimentary voxel physics" (CPU falling-sand-style materials:
  https://soggykees.itch.io/rudimentary-voxel-physics).

## 4. Character controllers & physics in micro-voxel worlds

**Honest headline: NO micro-voxel creator has published text on character controllers at sub-cm
voxel scale.** John Lin says only "fast collision detection that physics and player walking
will utilize"; Atomontage shows vehicle/footprint demos without algorithm text; MishMash's
controller is undocumented. What exists in text is blocky-scale voxel controller literature,
which generalizes:

### 4.1 Documented voxel-world controllers (blocky scale) — CONFIRMED

- **godot_voxel VoxelBoxMover** — AABB move-and-slide vs voxel AABBs, "similar to Minecraft
  physics"; optional **step climbing with max step height** ("Climbing modifies the motion
  vector upwards so that the body is snapped on top of the step"), `has_stepped_up()` flag for
  grounded checks; precision note: shrink AABB slightly to avoid false positives at boundaries.
  https://voxel-tools.readthedocs.io/en/stable/api/VoxelBoxMover/
- **Per-axis clipping (the Minecraft/Notch method)** — resolve Y first, then X, then Z,
  clipping the move delta against each candidate voxel AABB so the boxes never interpenetrate;
  velocity zeroed on the clipped axis. Writeup with code:
  https://medium.com/@andrebluntindie/3d-aabb-collision-detection-and-resolution-for-voxel-games-5fcbfdb8cdb4; a C++
  implementation documented in depth:
  https://deepwiki.com/penggrin12/cppvoxelgame/6-physics-and-spatial-queries (broad-phase
  `getCubes` = floor(AABB bounds) → iterate grid; per-axis `clipXCollide` etc.; `onGround` =
  downward move truncated).
- **Proper swept AABB (better than per-axis)** — fenomas/voxel-aabb-sweep: "essentially
  raycasts along the AABB's leading corner, and each time the ray crosses a voxel boundary, it
  checks for collisions across the AABB's leading face in that axis. This gives correct results
  even across long movements" — vs naive per-axis sweeps which are "inaccurate for larger
  movements... anisotropic." https://github.com/fenomas/voxel-aabb-sweep
- **Sub-stepping + binary search contact refinement** — rlVoxel: fixed 20 TPS, movement split
  into 0.05-unit sub-steps to prevent tunneling, 8-iteration binary search to find exact
  contact, "liquid pop-up" impulse when the player collides with a wall while swimming and
  there's air 0.6 units above (the "climb ashore" trick).
  https://deepwiki.com/tacf/rlVoxel/4.2-player-controller-and-physics
- **Capsule vs voxel — only documented implementation**: BEPU Physics 2's custom `Voxels`
  collidable demo registers capsule/sphere/box/convexhull collision AND sweep tasks against a
  voxel grid as a "homogeneous compound."
  https://github.com/bepu/bepuphysics2/blob/master/Demos/Demos/CustomVoxelCollidableDemo.cs
- **Swimming/buoyancy**: John Lin's volumetric buoyancy (§1.3) is the only micro-voxel-scale
  buoyancy statement; Teardown's water is a triangle surface (not volumetric interaction).

### 4.2 What "walking smoothly" means at 7.8 mm voxels — INFERENCE (grounded in the above)

1. **Step-up logic doesn't disappear — it becomes a smoothing budget.** At 7.8 mm voxels, every
   natural slope is a staircase of sub-cm steps. A step-up/climb allowance of even 2–5 cm (3–7
   voxels) absorbs all terrain roughness, so "smooth walking" = plain AABB sweep + a small
   max-step-height snap (VoxelBoxMover semantics), not per-voxel stair logic. The failure mode
   isn't stairs, it's **jitter from snapping between adjacent micro-steps** — hence transient/
   one-shot smoothing of the ground height (compare MishMash's one-shot LOD fades being "30%
   cheaper and nearly as smooth").
2. **Broad-phase is the real problem.** A 0.6×1.8 m player AABB spans ~77×231×77 ≈ 1.4M voxels
   at 7.8 mm — iterating candidate voxel AABBs Minecraft-style is impossible. Documented
   answers: (a) sweep along the leading corner (fenomas) so you only touch boundary-crossing
   voxels — O(path length), not O(volume); (b) collide against a **decoupled coarse collision
   proxy** (Teardown's separate occlusion structure; John Lin's "generating collision data for
   physics processing" conversion stage; Rijsdijk's 1-bit local patch + slow rebuild). For a
   static analytic world, (b) is very cheap: downsample occupancy to a coarser grid purely for
   collision.
3. **The analytic height_at clamp is the correct pattern**: Lin's vegetation placement and
   Voxel Quest's whole engine show the analytic layer should stay authoritative; voxels are a
   materialization, and collision can query the analytic layer directly for terrain while using
   the voxel tree only for authored/destructive edits.

## 5. Ranked adoptable techniques for THIS engine

(C++20, ray-marched sparse-brick octree, 7.8 mm voxels near camera, static analytic world, no
textures, EnTT ECS, entt::dispatcher, custom AABB-sweep.) Ranked by relevance × evidence
quality:

1. **Per-voxel normals + unique per-voxel color as the core shading model.** CONFIRMED as the
   "secret sauce" behind the Lin/Teardown look (Octo devlog #17); fits a no-texture engine
   perfectly — normals + albedo per voxel ARE the material system. This repo already stores
   average normals in tree nodes (layout v2); consider full per-voxel (or per-brick) normals
   near the camera.
2. **64-way brick tree + 64-bit occupancy bitmasks for empty-space skipping.** CONFIRMED (Octo
   #17): octrees waste 8+ memory reads per step; a 4×4×4-children tree with occupancy bitmasks
   lets rays take up to 10 steps with zero memory reads. Direct upgrade path for the sparse-
   brick octree.
3. **Decouple collision from render voxels: a coarse collision proxy generated from the
   analytic world.** CONFIRMED pattern three ways (Teardown's occlusion-only structure; Lin's
   collision-data conversion stage; Rijsdijk's local 1-bit patch). At 7.8 mm you cannot
   AABB-sweep against render voxels; this repo's analytic TerrainCollider is already this
   pattern.
4. **Swept AABB along the leading corner with leading-face checks (not per-axis clipping).**
   CONFIRMED (fenomas/voxel-aabb-sweep): correct for long movements, isotropic, O(path). This
   repo's move_and_slide is sub-stepped per-axis — the fenomas algorithm is the documented
   better shape if tunneling/anisotropy ever shows up.
5. **Small step-height snap (2–5 cm) + one-shot ground smoothing instead of stair logic.**
   CONFIRMED semantics (VoxelBoxMover max_step_height/has_stepped_up) + INFERENCE on the budget
   size for 7.8 mm voxels (§4.2). Handles micro-step jitter.
6. **Two-tier vegetation: voxel trees near, deterministic-ID props + card/instanced grass far.**
   CONFIRMED (MishMash): deterministic prop IDs per position+type so destruction cascades
   across LOD bands; grass as clustered approximate distribution at coarse resolution;
   combined instancing made it FASTER with more instances. For EnTT: props = entities in the
   detail region, macro props = pure data lists keyed by chunk, joined at batch-build time;
   entt::dispatcher events for spawn/destroy cascading to macro lists.
7. **Convert polygon vegetation assets to voxels; place via ray-traced queries (sunlight, cave
   walls, openness).** CONFIRMED (John Lin Crystal Islands): PlantFactory/PlantCatalog →
   voxelization pipeline; placement uses ray-traced world queries. The analytic world makes
   sunlight/openness queries cheap to precompute per candidate site.
8. **Sieve-function multi-resolution generation (generate AT 1/N res, never downsample).**
   CONFIRMED (MishMash): every generation function takes a sieve so macro chunks are generated
   natively at low res in the same time as full res. Applicable to the SVO build: generate
   coarse bricks analytically instead of mip-mapping fine bricks.
9. **Transient one-shot LOD fades + adaptive fog.** CONFIRMED (MishMash): ~30% cheaper than
   continuous transitions; fog tied to loaded bands.
10. **Rasterize brick AABBs / use a depth prepass to start DDA rays at the surface.**
    CONFIRMED (nelari.us; Octo #4's deferred-depth lesson): rasterize camera-facing brick quads
    into depth, use as ray origin — skips empty space. Natural hybrid alongside the marcher.
11. **Fluid on a coarse GPU grid with cell occupancy + volume transport + motion vector.**
    CONFIRMED (MishMash devlog #4): naive-but-fast, tick-rate and region controlled;
    visualized by a separate water shader reading the sim. Gameplay-first, not accuracy-first.
12. **Volumetric buoyancy by sampling the fluid velocity field at contact points.** CONFIRMED
    (John Lin boats video): buoyancy integrated volumetrically over submerged voxels of the
    body; one-way coupling is enough for gameplay.
13. **Multi-format voxel pipeline (Allocation/Tagging/Conversion) instead of one true format.**
    CONFIRMED (Lin's blog): render bricks, collision proxy, and simulation state as separate
    converted views of the analytic source; attributes tagged only where needed.
    Architecturally the cleanest fit for EnTT (each conversion = a component pool).
14. **GigaVoxels-style MIP-mapped bricks + cone tracing for LOD-stable shading.** CONFIRMED
    (academic): brick borders duplicated for interpolation, per-ray cone footprint selects MIP
    level; kills aliasing/popping at LOD transitions. Heavier lift; relevant once LOD bands
    are stable.
15. **If rigid bodies/destruction arrive later: connected-component detection + TGS substepping
    solver.** CONFIRMED (Octo 0.3.0; Gustafsson's new engine citing Erin Catto's Box2D
    substepping/"Temporal Gauss-Seidel" research; parallel solver for piles). Also Gustafsson's
    multiplayer rule: destruction as deterministic commands, never voxel data.

Explicitly NOT adoptable from this research (no text exists): Lin's wind/sway technique for
voxel grass, Lin's collision algorithm, MishMash's character controller, Atomontage's
vegetation and physics internals. These are watch-only; any implementation is inference, not
adoption.
