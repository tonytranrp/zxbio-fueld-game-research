# Phase 1 — Terrain, Vulkan Rendering & Movement — Implementation Brief

Companion to [`PROJECT_BRIEF.md`](PROJECT_BRIEF.md) and [`CLAUDE.md`](CLAUDE.md). Read both
first — this document assumes the tech decisions in `PROJECT_BRIEF.md` §2 and the Phase 0 build
recipe in `CLAUDE.md` as settled fact and doesn't re-derive them. Where this document's milestones
supersede `PROJECT_BRIEF.md` §11's old Phase 1–5 breakdown, §11 below has the literal replacement
text.

Table of contents: [0. Scope reframing](#0-scope-reframing--read-this-first) ·
[1. What's in and out](#1-whats-in-and-out-of-phase-1) ·
[2. Diligent's Vulkan backend](#2-diligents-vulkan-backend-understood-properly) ·
[3. Radient/Tessera](#3-radienttessera--status) ·
[4. Coding style additions](#4-coding-style-additions-for-the-render-layer) ·
[5. Profiling & debug tooling](#5-profiling--debug-tooling) · [6. Camera & movement](#6-camera--movement) ·
[7. Folder/file structure additions](#7-folderfile-structure-additions) · [8. Milestones](#8-milestones) ·
[9. Subagent research plan](#9-subagent-research-plan--ready-to-fire-prompts) ·
[10. Guardrails](#10-guardrails-specific-to-this-phase) ·
[11. Patch for PROJECT_BRIEF.md §11](#11-patch-for-project_briefmd-11)

## 0. Scope reframing — read this first

The request behind this document collapses `PROJECT_BRIEF.md`'s old Phase 1 (engine skeleton),
Phase 2 (world generation), Phase 3 (meshing), Phase 4 (rendering integration), and half of Phase
5 (camera/movement — streaming stays deferred, see §1) into one "Phase 1": the terrain pipeline,
Vulkan-via-Diligent rendering, and a flyable camera, together, researched deeply, with subagents
doing the legwork.

Worth saying directly rather than silently complying or silently refusing: five previously
separate, independently-sized units of work do not become one atomic unit of work by being
described in one document. Packaging them as a single huge prompt without internal structure would
produce either a shallow pass across everything or an unreviewable wall of work with no stopping
points — both worse than what was actually asked for, which is real depth on the Vulkan/rendering
research specifically. The resolution used throughout this document: one comprehensive brief, the
single artifact requested, internally organized into milestones (§8) that each still have their
own definition of done and are each independently resumable — so "Phase 1" is the right size as a
body of work to plan and research together, while remaining executable in the same incremental way
Phase 0 already was. This isn't a smaller version of what was asked for; it's the same scope with
real stopping points preserved, which is what makes "including movements... but rendering using
Vulkan... this is a large task" actually buildable rather than just long.

One update from Phase 0 worth restating up front: the GPU-availability open question from
`PROJECT_BRIEF.md` §14 is resolved — this machine has a working GPU/driver stack, D3D11/D3D12/
OpenGL/Vulkan all link under real MSVC. What Phase 0 confirmed is that they build; what hasn't
been confirmed yet is that they run — actually enumerating a physical device and presenting to a
window is a runtime question, not a compile-time one, and it's the very first thing M1.4 checks
(§8), not an assumption carried in from Phase 0's build success.

## 1. What's in and out of Phase 1

In, matching "most of the game... including movements... rendering using Vulkan... no models or
player yet": the full terrain pipeline (generation → meshing → GPU upload → draw), a working
Diligent-on-Vulkan render path with a hand-written shader pair, a free-flying spectator camera
with keyboard/mouse movement, and the concurrency/job-system plumbing tying generation and meshing
to the render loop. Debug/profiling tooling is pulled forward into this phase too, since "what
profiler" was asked directly.

Still out, unchanged from `PROJECT_BRIEF.md` §1.2: any player character or model (stated
explicitly — the camera is a free-floating viewpoint, not an entity with a mesh),
destructible/editable terrain, vegetation/structures/redstone-style logic, scripting,
networking, modding.

Deliberately deferred out of this phase specifically, back into stretch (§8 explains why for
each): chunk streaming by render distance tied to camera movement (a fixed, generated-once region
is enough to prove the terrain-to-screen pipeline; streaming is a real feature with its own edge
cases — load/unload thrashing near a boundary, in-flight-job cancellation — that deserve their own
milestone rather than being folded silently into "also render it"), and GPU-driven indirect draw +
compute culling (§2.6 explains the staged path — this phase builds the CPU-culled version first
and designs the interface so the GPU-driven version is a real, planned upgrade, not a retrofit).

## 2. Diligent's Vulkan backend, understood properly

This is the section the request specifically asked to be researched thoroughly rather than
answered from a general "Diligent abstracts the graphics API" gloss. Six things, each one a real
design or correctness decision, not trivia.

### 2.1 The resource-binding model: Static / Mutable / Dynamic

Confirmed directly (Diligent's own introductory documentation): Diligent 2.0's binding model
classifies every shader variable into one of three categories, and this classification is
Diligent's API-agnostic abstraction over exactly the concept Vulkan calls descriptor-set update
frequency — understanding one is understanding the other:

- **Static** — constant across every instance of a given shader. Bind once, directly on the
  shader object (`IShader::BindResources()` or its variable interface), before any
  `IShaderResourceBinding` even exists. The right home for data that's the same for the whole
  program's lifetime.
- **Mutable** — constant across one `IShaderResourceBinding` instance's lifetime, bound once
  through that SRB (`IShaderResourceBinding::BindResources()`), not settable through the shader
  object directly. The right home for per-material or per-pass data that changes occasionally,
  not every draw.
- **Dynamic** (the category the fetched documentation cuts off before naming, but consistent with
  Diligent's own published binding-model docs and with the general SRB/descriptor-set pattern
  every Vulkan-abstraction engine converges on) — rebindable per draw call through the device
  context, for data that genuinely varies every single draw.

The concrete design decision this drives for the voxel shader pair: camera view/projection data is
Static or bound once per frame at most (it doesn't change per chunk); a chunk's model-space
transform (just its world-space chunk-coordinate offset, since terrain doesn't rotate/scale per
chunk) is the one thing that's genuinely Dynamic across the chunk draw loop. Getting this
categorization right isn't a style preference — it's the difference between Diligent generating
one descriptor set/PSO-resource-layout that's touched once versus one that's re-validated on every
single chunk draw call, and it's exactly the kind of decision this section exists to make explicit
rather than leave to whatever the first draft happens to do.

### 2.2 PSO creation, HLSL → SPIR-V, and the render state cache

Pipeline state (`GraphicsPipelineStateCreateInfo`) bundles every fixed-function GPU stage
configuration (blend, depth/stencil, rasterizer, shader stages, resource layout) into one object,
confirmed directly from Diligent's own Tutorial01 walkthrough — this is the object that gets bound
once per distinct rendering "mode" (in this case: exactly one PSO for opaque terrain, later a
second for water once it's a distinct material). Shaders are authored in HLSL regardless of target
backend; Diligent's own shader-source converter produces GLSL/SPIR-V/MSL as needed per backend —
write once, runs correctly cross-backend, no `#ifdef VULKAN` in the shader source itself.

A real, concrete quality-of-life feature worth wiring in from the start, not bolting on later:
`IRenderStateCache`, confirmed directly from Diligent's own release notes (v2.5.3) — it wraps
shader and PSO creation, caches the compiled results (so a second run skips recompilation), and
supports hot shader reloading: call `Reload()` and the cache detects which shaders changed and
transparently updates the pipelines using them, live, without restarting the app.
`DiligentSamples`' Tutorial 25 ("Render State Packager") is the concrete reference. For a shader
pair that's going to be iterated on constantly while tuning terrain lighting, this is a real,
measurable iteration-speed win, not a nice-to-have — worth having by the time M1.4 is drawing
anything, not retrofitted after the fact.

### 2.3 The NDC/depth-range trap: GLM vs. Diligent's normalized convention

A specific, well-established, silent-failure-mode bug to design around before it happens, not
after staring at a wrong-looking frame: GLM's `glm::perspective()` defaults to producing a
projection matrix for OpenGL's `[-1, 1]` normalized-device-coordinate depth range. Diligent's
whole reason to exist is presenting one consistent convention to application code regardless of
backend — which means it normalizes to the D3D/HLSL-style `[0, 1]` depth range across every
backend it supports, Vulkan included (Vulkan's native NDC is already Y-down/`[0,1]`-depth,
matching D3D — OpenGL is the actual outlier here, and Diligent's abstraction absorbs that
difference on OpenGL's side so application code doesn't have to). Feed GLM's default `[-1,1]`-range
matrix into a Diligent pipeline without correcting for this and depth testing/clipping is subtly
wrong — not a crash, not an obviously garbled image, just wrong culling and z-fighting that's
expensive to root-cause blind. The fix: set `GLM_FORCE_DEPTH_ZERO_TO_ONE` before including GLM
headers, project-wide, so every `glm::perspective`/`glm::ortho` call already matches Diligent's
normalized convention. One `#define`, and it belongs in one shared header
(`engine/core/include/engine/core/math.hpp` or similar — see §7), not repeated per-file.

### 2.4 Deferred contexts and the real cost/benefit of multithreaded submission

Diligent's deferred contexts are the documented, first-party answer to multithreaded command
generation (`DiligentSamples`' `Tutorial06_Multithreading`) — an immediate context plus N deferred
contexts, D3D11-style, abstracted over Vulkan secondary command buffers and D3D12 command lists
underneath. Confirmed from Khronos's own official Vulkan Samples documentation: the underlying
Vulkan mechanism is real and well-supported — secondary command buffers inherit render-pass state
from a primary buffer via `VkCommandBufferInheritanceInfo`, get recorded concurrently, and are
stitched together with `vkCmdExecuteCommands`. Real infrastructure is required to do this
correctly (a command pool, descriptor pool cache, descriptor set cache, and buffer pool per
frame-in-flight, per thread — per the same official sample), which is precisely what Diligent's
deferred-context abstraction exists to hide from application code.

The caution worth stating plainly, because it's counter to the "multithreading is obviously
faster" intuition: multiple independent community reports (Khronos's own developer forum) describe
multithreaded secondary-command-buffer recording performing worse than single-threaded primary
recording in real, measured cases — one report cites roughly 900 FPS multithreaded versus roughly
4000 FPS single-threaded rendering 500 cubes, and a second separately corroborates poor
multithreaded secondary-command-buffer performance. The likely mechanism in both is that
per-thread pool/command-buffer overhead dominates when the actual per-draw recording cost is small
— exactly the regime a chunk terrain render loop is in in Phase 1, before render distance and
chunk count both grow. The decision this drives: M1.4 draws every chunk from the immediate
context, single-threaded, correct and simple. Wiring up deferred contexts is real, valuable,
well-supported infrastructure — worth building once there's an actual measured frame-time number
showing single-threaded submission is the bottleneck (per `release-codegen-and-tradeoffs.md`'s
closing heuristic, applied to a new place), not before. This is exactly the kind of thing a first
instinct gets wrong by assuming "more threads, faster" without the number to back it up — flagged
here so it isn't rediscovered the hard way.

### 2.5 Native backend handle access — a local-source question, not a web one

Diligent's own design pattern (consistent with backend-specific interfaces documented across its
API, e.g. `IPipelineStateGL::GetGLProgramHandle`, confirmed present from release notes) strongly
suggests Vulkan-specific escape hatches exist too (an `IRenderDeviceVk`/`IDeviceContextVk`/
`ITextureVk`-style `QueryInterface` path exposing the real `VkDevice`/`VkCommandBuffer`/`VkImage`
underneath) — needed for exactly one thing in this phase: wiring Tracy's Vulkan GPU-zone profiling
(§5), which needs the real `VkPhysicalDevice`/`VkDevice`/`VkQueue`/`VkCommandBuffer` handles to
call `TracyVkContext(...)`. This wasn't independently pinned down by web search in this pass, and
it shouldn't be searched for further — the actual `DiligentCore` source is already sitting in the
Phase 0 CPM cache on this machine. This is a `grep -r "Vk(" DiligentCore/Graphics/
GraphicsEngineVulkan/interface/` away, not a web question. Routed to Subagent A (§9) precisely
because it's a local-source-reading task, not a research-the-internet one — worth naming as a
distinct subagent type from the library-research.md web-research pattern, since Claude Code's
subagents can just as well be pointed at `Read`/`Grep`/`Glob` on an already-fetched dependency as
at `WebSearch`/`WebFetch`.

### 2.6 Voxel-to-GPU data shape: vertex layout, the material palette, and the GPU-driven upgrade path

Vertex layout: position (`float3`), normal (`float3`), material ID (packed into a `uint32` or a
single byte with padding) — one interleaved (AoS) vertex struct, not split into separate
per-attribute buffers. This is a deliberate, correct application of `memory-and-performance.md`
§5's own stated exception to its SoA default: SoA wins when a hot loop touches one or two fields
of many elements; a vertex shader invocation touches every field of one vertex at once (position
always, normal for lighting, material ID for the palette lookup below) — exactly the "AoS is
simpler and better" case that file names explicitly, and it's also what GPU vertex-fetch hardware
is built to stream efficiently. Chunk-local indices fit comfortably in `uint16` (a `32³` chunk's
surface-crossing vertex count is well under 65,536) — half the index-buffer size of `uint32` for
free.

Material rendering, v1: no textures yet (matches "no models" — texturing is its own phase). A
per-vertex material ID indexes a small fixed-size color array uploaded as a constant buffer (a
palette, directly continuing John Lin's own material-ID-attribute framing from
`PROJECT_BRIEF.md` §1.1) — land, stone, water each get a flat or simple-Lambertian-shaded color.
Cheap, correct, and it's the natural slot a texture array upgrades into later without changing the
vertex layout.

Normals: computed at generation/meshing time (an analytic gradient of the density/occupancy field,
or a central-difference estimate), not a separate mesh pass — cheaper, and it's data the meshing
stage already has in hand.

The GPU-driven upgrade path, staged rather than built first: real, current, well-precedented
technique for exactly this shape of problem — many similar chunk draws, most of them entirely
off-screen at any given camera angle. Confirmed from three independent, real sources: Khronos's
own official Vulkan Samples (`vkCmdDrawIndexedIndirect` reading draw parameters from a
GPU-resident buffer that a compute shader can populate/cull against, instead of the CPU issuing
one `vkCmdDrawIndexed` per object); `vblanco20-1/vulkan-guide`'s "Project Ascendant" — a real,
actively-maintained, hands-on Vulkan 1.3 tutorial project explicitly built around large-scale
voxel terrain with a GPU-driven indirect culling pipeline, and the single most directly relevant
external reference this phase has; and `VulkanMod`, a real, shipping Vulkan renderer mod for
Minecraft that lists indirect-draw chunk rendering among its actual, shipped optimizations. This
is real, not speculative. But it's a stretch item for this phase, not M1.4's deliverable: §2.4's
multithreading caution generalizes here too — GPU-driven culling is a real win once there are
enough chunks that CPU-side frustum culling and per-draw overhead are the measured bottleneck, and
it's real added complexity (a compute pass, a GPU-visible draw-command buffer, careful
synchronization between the culling compute shader and the indirect-draw consumption of its
output) before that's true. M1.4 does CPU-side frustum culling (bounding-box-vs-frustum test per
chunk, skip the draw call entirely if it fails) and individual draw calls — correct, debuggable in
RenderDoc one draw call at a time, and the `render/interface` boundary from `PROJECT_BRIEF.md` §3
is designed so swapping the submission strategy later doesn't touch `world/` at all.

## 3. Radient/Tessera — status

Phase 0 confirmed `DiligentFX/Radient/` is real: a scene/render-pipeline component
(`Scene/RadientSceneImpl`, `Scene/RadientSceneWriterImpl`, `Render/RadientRenderPipeline`) with a
`Render/Tessera/` subsystem (`RadientTesseraGeometryPass`, `RadientTesseraDrawableCache`,
`RadientTesseraRenderTechnique`). This pass's own web research could not find it documented in any
indexed copy of DiligentFX's README or wiki — consistent with it being recent enough (this project
is pinned to a fresh master commit, `aca2285`, 2026-08-16) not to have propagated into third-party
mirrors or search indices yet, not evidence it's unreal or unstable. The naming (`DrawableCache`,
`GeometryPass`, `RenderTechnique`) is at least suggestive of infrastructure for efficiently
managing and drawing many similar objects — which, if accurate, would be directly relevant to
chunk rendering — but that's a read of the class names, not a finding, and it's stated that
plainly rather than presented as more certain than it is. Routed to Subagent B (§9): same "read
the local source, not the web" logic as §2.5.

## 4. Coding style additions for the render layer

Everything in `SKILL.md` still applies unchanged — this is additive, specific to graphics code,
not a replacement.

- **Name every GPU object.** Every `*CreateInfo`/`*Desc` struct Diligent uses to create a buffer,
  texture, or PSO carries a debug name field for exactly this purpose (the general pattern —
  confirmed present in comparable Vulkan-abstraction engines, e.g. Vulkan `Object::SetName`/
  `BeginDebugLabel` in O3DE's own RHI — is near-universal for serious graphics abstractions; the
  exact Diligent field name per object type is Subagent A's job to confirm, §9). An unnamed buffer
  shows up as a bare hex handle in RenderDoc/Nsight; a named one reads as
  `"ChunkVertexBuffer[12,4,-3]"`. This is cheap and it's the difference between a profiler capture
  being useful and being an anonymous list of hex addresses to guess at.
- Shaders live in `render/diligent/shaders/`, one file per stage, named for what they shade
  (`terrain.vsh.hlsl`/`terrain.psh.hlsl`), not bundled into a monolithic uber-shader — matches the
  render-state-cache's per-file hot-reload model in §2.2 directly: reloading one changed file
  shouldn't force recompiling everything.
- Every cbuffer-mirroring C++ struct gets a comment stating its HLSL-side packing, given §2.3's
  16-byte-boundary gotcha — a one-line `// matches cbuffer CameraCB { float4x4 ViewProj; }`
  comment above the C++ struct is cheap insurance against the two drifting silently apart.
- Frustum-cull and draw-submission code stays in `render/diligent`, not `world/` — culling is a
  rendering concern (it needs the camera frustum), not a world-data concern; `world/chunk` exposes
  a bounding box, `render/diligent` decides what to do with it. Keeps `world/` GPU-free per
  `PROJECT_BRIEF.md` §3's existing rule, not a new one.

## 5. Profiling & debug tooling

Two defaults, decisively, plus one vendor-conditional addition — not "pick whichever," per house
style on tool comparisons:

**RenderDoc** — the default frame-capture debugger. Confirmed directly (its own repository): open
source (MIT), genuinely cross-platform (Windows/Linux/Android, Vulkan/D3D11/D3D12/OpenGL/OpenGL
ES), built with first-class Vulkan support from Vulkan 1.0's own launch. Captures one frame, then
gives full pipeline-state-at-every-draw-call inspection, texture/buffer contents, and a mesh
viewer showing vertex-shader input against output — the right tool for "why does this one chunk's
mesh look wrong," one draw call at a time. Free, no vendor lock-in, works regardless of GPU vendor.

**Tracy** — the default real-time CPU+GPU timeline. Confirmed current and Vulkan-capable (its own
repository and changelog, not just the older OpenGL-only version some tutorials still describe):
nanosecond-resolution, low-overhead, instrumentation-based (`TracyZone`-style scoped macros),
profiles CPU (C++) and GPU (OpenGL, Vulkan, D3D11/12, OpenCL) together in one live timeline, plus
lock contention and thread interaction — exactly what the job system in `PROJECT_BRIEF.md` §6
needs visibility into (is a chunk's generate→mesh→upload pipeline actually parallelizing, or is a
thread sitting idle waiting on something). Concrete integration shape, confirmed directly from
Tracy's own Vulkan header: `TracyVkContext(physDevice, device, queue, cmdBuffer)` once at startup
(needs the real Vulkan handles from §2.5), `TracyVkZone(ctx, cmdBuffer, "name")` around a GPU-side
scope, `TracyVkCollect(ctx, cmdBuffer)` once per frame to pull query results back. On the CPU
side, `ZoneScoped`/`ZoneScopedN("name")` around each job-system task (generate, mesh, upload) is
the natural first set of zones.

**Vendor-specific bonus**, conditional on confirming the actual GPU: NVIDIA Nsight Graphics if the
dev GPU is NVIDIA (confirmed current — version 2026.3 release notes found, dated — with a GPU
Trace profiler showing SM warp occupancy per shader, live shader editing with re-trace comparison,
and Vulkan semaphore/D3D12 fence wait/signal visualization); AMD's Radeon GPU Profiler is the
equivalent if the GPU turns out to be AMD. Which one applies depends on hardware this document
doesn't have visibility into — confirm the actual GPU (`vulkaninfo` or the Windows Device Manager)
before picking, rather than defaulting to Nsight on the assumption it's NVIDIA.

## 6. Camera & movement

No player entity, no model — a free-flying spectator viewpoint, explicitly, matching "no models or
player yet."

- An ECS entity (per `PROJECT_BRIEF.md` §2.4's EnTT pick) carrying a transform (position +
  orientation) and a camera component (FOV, near/far planes) — not a bespoke `Camera` class
  sitting outside the ECS, so the same entity/component machinery that will eventually carry real
  game objects already exercises the camera.
- Orientation as a quaternion, not raw Euler pitch/yaw, even for a simple flycam — GLM ships
  `glm::quat` and the conversion helpers for free, gimbal lock is a real and annoying failure mode
  the moment pitch approaches ±90° (looking straight up/down, which a free-flying camera does
  constantly), and there's no meaningful implementation-cost reason to accept that failure mode
  when the safe option is equally available.
- Input: GLFW's own callback registration (`glfwSetKeyCallback`/`glfwSetCursorPosCallback`)
  feeding a small `engine/input` component that the camera system reads from — WASD + mouse-look,
  the standard scheme, nothing bespoke needed for a spectator camera.
- Movement integrates against frame delta time (`engine/core`'s `Clock`, already in the Phase 1
  skeleton), not a fixed per-frame step, so movement speed doesn't couple to framerate.

## 7. Folder/file structure additions

Extends `PROJECT_BRIEF.md` §4's tree — additive, doesn't restructure what Phase 0 already
scaffolded (`engine/{core,ecs,jobs}`, `render/{interface,diligent}`, `app`, `tools` all stay
exactly as built):

```
engine/
└── input/
    ├── include/engine/input/      # InputState, key/mouse event types
    ├── src/                       # GLFW callback registration lives here, nowhere else
    ├── detail/
    ├── tests/
    └── CMakeLists.txt
world/
├── chunk/          # (Phase 0 stub → Phase 1 real: paletted voxel storage, per PROJECT_BRIEF.md §5)
├── generation/      # (Phase 0 stub → Phase 1 real: FastNoise2 heightmap + fill rules)
└── meshing/         # (Phase 0 stub → Phase 1 real: Surface Nets, chunk-boundary-correct)
render/
├── interface/
│   └── include/render/interface/
│       ├── renderer_backend.hpp   # RendererBackend, DrawList, MeshHandle — unchanged boundary
│       └── camera.hpp             # NEW: camera/frustum data the backend consumes, still no
│                                  #      DiligentCore types crossing this boundary
└── diligent/
    ├── include/render/diligent/
    ├── src/
    │   ├── device_init.cpp        # NEW: §2's device/context/swapchain init
    │   ├── pso_terrain.cpp        # NEW: the one hand-written PSO from §2.2
    │   └── frustum_cull.cpp       # NEW: §2.6's CPU-side culling — lives here, not in world/
    ├── detail/
    └── shaders/                   # NEW, per §4's per-file-per-stage convention
        ├── terrain.vsh.hlsl
        └── terrain.psh.hlsl
```

## 8. Milestones

Each still independently resumable and independently "done" — the internal structure §0 promised.
GPU-required is noted per milestone now that GPU build is confirmed but GPU runtime isn't yet
(§0's own caveat).

**M1.1 — Engine skeleton, for real (no GPU/display needed).** Fill in Phase 0's stub
`engine/{core,ecs,jobs}`: `Clock`, `Config`, `Log` in `core`; the EnTT registry wrapper in `ecs`;
the `std::jthread`-based `ThreadPool` from `PROJECT_BRIEF.md` §6 in `jobs`, unit-tested with
synthetic work under ASan/TSan. Done when: the app boots, ticks at a stable rate, and the job pool
executes and joins correctly under sanitizers — no chunk-shaped work yet.

**M1.2 — World generation (no GPU/display needed).** `world/chunk` (paletted storage, pmr-pooled,
per `PROJECT_BRIEF.md` §5) and `world/generation` (FastNoise2 heightmap + land/mountain/water fill
rules, per `PROJECT_BRIEF.md` §8). Done when: requesting a chunk coordinate deterministically
produces a filled `Chunk`, validated by unit test.

**M1.3 — Meshing (no GPU/display needed).** `world/meshing` (Surface Nets, chunk-boundary-correct),
normals computed per §2.6, `tools/mesh_dump` extended to also emit material IDs. Done when:
`mesh_dump` produces a seamless multi-chunk `.obj`, inspectable on any machine, no Diligent
involved.

**M1.4 — Rendering core (GPU + display required — the first milestone that actually needs the
runtime check §0 flagged).** Device/context/swap-chain init (§2.1–§2.3), the terrain PSO +
hand-written shader pair (§2.2, §4), GLFW window (`PROJECT_BRIEF.md` §2.2), the vertex/index
layout from §2.6, single-threaded immediate-context draw of M1.3's generated chunks with CPU-side
frustum culling. Done when: generated terrain — land, mountains, water — is actually visible on
screen, correct depth/culling (§2.3's gotcha didn't bite), at whatever framerate. Correctness over
speed here.

**M1.5 — Camera & movement.** §6 in full: ECS camera entity, quaternion orientation, GLFW input →
`engine/input` → camera system, delta-time-integrated flight. Done when: free-flying through the
M1.4 terrain feels correct — no gimbal lock, no framerate-coupled speed.

**M1.6 — Profiling & debug tooling.** §5 in full: RenderDoc captures working end to end on a chunk
draw, Tracy wired into the job system (`ZoneScoped` in generate/mesh/upload) and, once §2.5's
native handle is confirmed, into the Vulkan submission path (`TracyVkContext`/`TracyVkZone`). Done
when: a RenderDoc capture of one frame is legible (named objects, per §4), and a live Tracy session
shows the job system's actual parallelism, not just an assumption that it's working.

**M1.7 — Consolidation.** Everything above integrated into one running app (`voxel_app`), not five
disconnected pieces; `PROJECT_BRIEF.md` §11 updated per §11 below. Done when: a fresh build of
`voxel_app` boots, generates and renders terrain, and is flyable — the actual "most of the game, no
models yet" deliverable this document was written for.

Explicitly stretch, not part of Phase 1's done-when: chunk streaming by render distance (§1),
GPU-driven indirect draw + compute culling (§2.6), deferred-context multithreaded submission
(§2.4) — each gated on a real measurement first, not built speculatively.

## 9. Subagent research plan — ready-to-fire prompts

Per `library-research.md` §3's rules (read-only, no nested `Skill`/`Agent` calls, the response is
the deliverable, launched in parallel, cited or explicitly flagged as unverified) — extended here
to cover local-source-reading subagents (`Read`/`Grep`/`Glob` against the already-fetched Phase 0
CPM cache) alongside the usual web-research kind, since two of the four questions below are
genuinely faster and more accurate to answer by reading code already on disk than by searching the
web. Launch all four in parallel, one message, before M1.4 starts — their findings feed directly
into it.

**Subagent A — Diligent Core API surface (local source read).** Confirm the exact
Static/Mutable/Dynamic shader-variable classification and binding methods; the minimal
`GraphicsPipelineStateCreateInfo` shape for an opaque-terrain PSO; the exact `IRenderStateCache`
API and hot-reload semantics; whether Vulkan-specific native-handle interfaces
(`IRenderDeviceVk`/`IDeviceContextVk`/etc.) exist and how to get the real
`VkPhysicalDevice`/`VkDevice`/`VkQueue`/`VkCommandBuffer` for Tracy; the exact debug-naming
facility and whether the Vulkan backend honors it; the deferred-context API shape. Read-only
(`Read`/`Grep`/`Glob` against the local CPM cache), no web access, no guessing — say explicitly
what can't be found.

**Subagent B — Radient/Tessera investigation (local source read).** Read every header under
`DiligentFX/Radient/` and report what problem it solves (scene graph? render/frame graph?
GPU-driven pipeline?), its relationship to `DiligentFX/PBR/`, its public API entry points and a
characterized usage sequence, whether `RadientTesseraDrawableCache`/`RadientTesseraGeometryPass`
already solve some or all of §2.6's GPU-driven-culling stretch goal, any README/example/sample
usage found, and a plain recommendation (adopt now / revisit later / poor fit) with reasoning.
Read-only, local source only.

**Subagent C — GPU-driven voxel rendering & chunk-streaming survey (web research).** Go deep on
`vblanco20-1/vulkan-guide` ("Project Ascendant")'s actual indirect-culling pipeline structure;
find 3+ more real, citable sources on chunk/voxel-terrain rendering at scale specifically (not
general GPU-driven rendering); research chunk-boundary/LOD stitching for smooth-voxel terrain at
different LODs; research synchronization between a culling compute pass and indirect draw
consumption across frames-in-flight. Every claim needs a real, attributed source — never invent a
performance number. Web research only (`WebSearch`/`WebFetch`), no code changes.

**Subagent D — Profiling/debug-tooling integration specifics (web research).** Confirm
Diligent+RenderDoc integration guidance or the generic GLFW/RenderDoc attach workflow; Tracy's
current CPM/CMake integration story (package name, `TRACY_ENABLE` propagation, `TracyVkContext`
queue-family requirements); programmatic RenderDoc capture triggering (`RENDERDOC_GetAPI`) versus
manual UI capture, and which is more practical for iterating on a chunk-rendering bug; whether
Diligent surfaces a toggle for Vulkan validation layers (including synchronization validation) or
they need independent Vulkan SDK layer configuration, and any known conflict with RenderDoc's own
layer. Web research only, every claim cited.

The full, verbatim ready-to-fire prompt text for each subagent (exact task lists and rules) is
preserved in this repo's git history in the message that introduced this brief — reconstruct from
§2/§3/§5's content above if needed; the summaries here are what each subagent must cover, not a
paraphrase that drops requirements.

## 10. Guardrails specific to this phase

In addition to everything in `PROJECT_BRIEF.md` §12, unchanged:

- Confirm GPU runtime availability (device enumeration, a presentable surface) at the very start
  of M1.4 — §0's distinction between "builds" (Phase 0, confirmed) and "runs" (not yet confirmed)
  is real; don't assume Phase 0's build success settles it.
- Don't build the GPU-driven indirect/compute-culling path or deferred-context multithreaded
  submission until a real frame-time measurement says the simple version is the bottleneck — §2.4
  and §2.6 both exist specifically to head off building real complexity on an assumption.
- Don't let culling or draw-submission code migrate into `world/` — it's a rendering concern (§4),
  and the boundary in `PROJECT_BRIEF.md` §3 exists precisely so this kind of scope creep has an
  obvious place to not go.
- Read each subagent's actual report before writing the code it informs — a report that says "not
  confirmed" on some sub-question is a real answer, not a gap to paper over with a plausible guess.

## 11. Patch for PROJECT_BRIEF.md §11

`PROJECT_BRIEF.md`'s existing Phase 1 entry ("Engine skeleton... Done when: the app boots, runs an
empty loop...") and everything through its old Phase 5 entry are superseded by this document's §8.
Replaced in `PROJECT_BRIEF.md` §11 with a pointer to this document plus milestone status — see that
file directly for the live status line.

Phase 6 (tooling/CI hardening) and the stretch list in `PROJECT_BRIEF.md` §11 are left as-is —
profiling tooling has moved earlier (M1.6), but the CI-matrix/sanitizer-job hardening Phase 6
describes is still genuinely later work.

## Sources

Diligent's own introductory API documentation (Gamedeveloper.com's syndicated copy of Diligent
Graphics' own "Introduction to Diligent Engine" post) for the Static/Mutable/Dynamic
resource-binding model; DiligentSamples' own Tutorial01 (Hello Triangle) walkthrough for the
PSO-creation shape; DiligentEngine's own GitHub release notes (v2.5.3, v2.5.4, v2.5.5) for the
render-state-cache/hot-reload feature and general API-evolution context; a third-party
documentation index's summary of DiligentEngine confirming current WebGPU-backend and C#/.NET-
binding support; the official Khronos Vulkan Samples documentation ("Command buffer usage and
multi-threaded recording," "GPU Rendering and Multi-Draw Indirect") for the secondary-command-
buffer and indirect-draw mechanics; two Khronos developer-forum threads reporting measured
multithreaded-secondary-command-buffer performance regressions; `vblanco20-1/vulkan-guide`
("Project Ascendant") as the primary GPU-driven voxel-rendering reference; VulkanMod's own
repository for a real, shipping indirect-draw voxel (Minecraft) chunk renderer; Sascha Willems'
indirect-drawing example writeup; RenderDoc's own repository and wiki for its Vulkan support;
Tracy Profiler's own repository, README, and `TracyVulkan.hpp` header history for its current
Vulkan GPU-profiling API; NVIDIA's own dated Nsight Graphics 2026.3 and 2026.1 release notes; and
O3DE's own Vulkan RHI API reference as a cross-engine confirmation that per-object debug naming is
a standard facility in serious Vulkan-abstraction layers.

In-repo: this document is a direct extension of `PROJECT_BRIEF.md` §2 (technology decisions,
unchanged), §3 (the render/interface boundary, extended in §7 above), §5–§6 (memory/concurrency,
unchanged), and §11–§12 (roadmap and guardrails, patched in §11 above) — and of `SKILL.md` itself,
specifically `references/concurrency-and-parallelism.md` §1 (`std::jthread`, underlying M1.1's job
pool) and `references/memory-and-performance.md` §5 (the SoA/AoS reasoning §2.6 applies to vertex
layout specifically) and `references/library-research.md` §3 (the subagent protocol §9 extends to
local-source-reading subagents).
