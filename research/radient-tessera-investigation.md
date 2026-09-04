# Radient Component Research Report

> **Historical-citation note (2026-09-04):** brief filenames cited below (PROJECT_BRIEF.md,
> PHASE_1_BRIEF.md, M1_2_BRIEF.md, PHASE_1_COMPLETION_BRIEF.md, ENGINE_HARDENING_BRIEF.md) refer
> to root-level documents deleted in the docs migration to `docs/progress.md` + `docs/goals.md`.
> They remain retrievable from git history; citations kept verbatim as primary-evidence context.

Subagent B from `PHASE_1_BRIEF.md` §9, completed 2026-09-02. Read-only local-source investigation
(no web access) against the pinned DiligentEngine commit `aca2285`.

Root confirmed: `C:\Users\Tonyt\.claude\cpm-cache\diligentengine\ba74\DiligentFX\Radient\`. Read
every header under `Scene/`, `Render/` (including `Render/Tessera/` and `Render/Tessera/Passes/`),
plus the full public `interface/` umbrella, the `Assets`/`Core`/`Import`/`Math` public headers,
`readme.md`, `docs/`, both CMakeLists.txt files (Radient's and DiligentFX's), and several `.cpp`
bodies where header-only reading left the actual GPU submission mechanism ambiguous (essential for
Task 3). Also read `DiligentFX/PBR/interface/PBR_Renderer.hpp` directly, since Radient's own code
claims a direct relationship to it.

## 1. What problem Radient solves, and its relationship to PBR/

Radient is **not** a render-graph/frame-graph system and **not** a GPU-driven rendering pipeline.
It is a retained-mode **scene-graph + asset-management + PBR-rendering application framework** —
the kind of thing that sits *above* a low-level graphics API/renderer library and gives an
application entities, cameras, lights, materials, and a `Render()` call, rather than raw draw
submission. In spirit it's closer to a lightweight game-engine rendering layer (a scene graph + a
fixed-function-configured renderer, like a scriptable-render-pipeline camera stack) than to a
Frostbite-style frame graph with automatic resource-dependency compilation — the pass order in
`RadientTesseraRenderTechnique`/`RadientTesseraPostProcessPipeline` is hand-sequenced C++
(geometry → skybox → SSAO/SSR/TAA/DOF/Bloom → composite → tonemap), not a declarative DAG.

Five cooperating subsystems, visible directly in `Radient/CMakeLists.txt`'s source list:
- **`Scene/`** — an EnTT-backed ECS (`entt::registry` in `include/Scene/RadientSceneState.hpp`)
  with entities, parent/child hierarchy, dirty-flag propagation for transforms/visibility, and
  built-in components (transform, camera, mesh, mesh-renderer, light, material-bindings) plus a
  generic serialized "custom component" escape hatch. Reads (`IRadientScene`) and writes
  (`IRadientSceneWriter`) are deliberately separate interfaces over the same state.
- **`Assets/`** — URI-addressed mesh/material/texture/scene assets, async loading, and a pluggable
  `IRadientAssetResolver` (filesystem by default).
- **`Import/`** — a GLTF importer that populates the scene graph through the writer interface.
- **`Render/` + `Render/Tessera/`** — the actual renderer. `Render/` holds renderer-agnostic
  plumbing (draw/light lists, G-buffer target management, material SRB caching) and
  `RadientPBRRenderer`. `Render/Tessera/` is the **one** concrete implementation of the
  `IRadientRenderTechnique` strategy interface (`include/Render/RadientRenderTechnique.hpp`) — a
  deferred-ish, G-buffer rasterizer with a full post-effect chain.
- **`Core/`** — engine/backend/view glue (`RadientEngineImpl`, `RadientBackendImpl`,
  `RadientViewImpl`) exposing the top-level `IRadientEngine`.

**Relationship to `DiligentFX/PBR/`: Radient is built directly on top of it, not an alternative to
it.** The decisive evidence is `include/Render/RadientPBRRenderer.hpp`:

```cpp
class RadientPBRRenderer final : public PBR_Renderer
```

This subclasses `Diligent::PBR_Renderer` — the exact class declared in
`DiligentFX/PBR/interface/PBR_Renderer.hpp` (verified by reading that file directly: `class
PBR_Renderer` with `PSO_FLAGS`, `VERTEX_ATTRIB_ID`, PSO hash-map/cache machinery).
`RadientPBRRenderer` overrides only `CreateCustomSignature()` (to split the pipeline resource
signature into frame-global vs. draw/material groups) and adds a frame-SRB cache; everything else
— PSO creation/caching, the GLTF material shading model, debug-view types — is inherited unchanged.
Further corroboration:
- `Radient/CMakeLists.txt` links the `Radient` static library `PRIVATE` against `DiligentFX` itself
  (the umbrella target containing PBR/PostProcess/Components), plus `EnTT` and `abseil`
  (`absl::btree`, `absl::flat_hash_map`, `absl::inlined_vector`).
- `include/Render/Tessera/Passes/RadientTesseraPostProcessPipeline.hpp` directly instantiates
  DiligentFX's existing `Bloom`, `PostFXContext`, `DepthOfField`, `ScreenSpaceAmbientOcclusion`,
  `ScreenSpaceReflection`, `TemporalAntiAliasing` types from `DiligentFX/PostProcess/*/interface/*.hpp`
  — the effects are orchestrated, not reimplemented.
- `DiligentFX/CMakeLists.txt` lists Radient as a sibling `add_subdirectory(Radient)` alongside
  `PBR`, `PostProcess`, `Components`, `Utilities` — architecturally a peer module.

So: `PBR/` answers "how do I shade a pass with PBR materials once I already know what to draw."
Radient answers "how do I build and drive a whole renderable scene (entities/cameras/lights/
materials/import) and get a finished, post-processed image" — using PBR/ and PostProcess/ as its
shading and effects engine underneath.

## 2. Public API entry points

Everything under `interface/*.h` is the real public surface (`Radient/CMakeLists.txt`: `interface`
is a `PUBLIC` include dir, `include/` is `PRIVATE`). The umbrella header `interface/Radient.h`
pulls in, in order: `RadientMath.h`, `RadientTypes.h`, `RadientAssets.h`, `RadientMeshPrimitives.h`,
`RadientScene.h`, `RadientSceneWriter.h`, `RadientSceneImporter.h`, `RadientBackend.h`,
`RadientView.h`, `RadientRenderer.h`, `RadientEngine.h`. All interfaces follow Diligent's standard
ref-counted COM-like pattern (`IXxx : IObject`, `RADIENT_STATUS`-returning factory methods, C- and
C++-callable).

Reconstructed minimal usage sequence (from the interfaces' own doc comments and parameter
dependencies — **no working example exists anywhere in the repo to verify this against**, see Task
4):

1. **`CreateRadientEngine(RadientEngineCreateInfo, IRadientEngine** ppEngine)`** — global factory,
   `interface/RadientEngine.h`. `RadientEngineCreateInfo` wraps an already-created
   `IRenderDevice*`/`IDeviceContext*`/optional `ISwapChain*` (Radient does not create the graphics
   device — the host app does normal Diligent device setup first) plus asset-manager creation info.
2. `engine->GetAssetManager(&pAssetManager)` → `IRadientAssetManager::CreateMesh` /
   `CreateMaterial` / `LoadTexture` / `LoadScene` (`interface/RadientAssets.h`) to build or load
   `IRadientMeshAsset`/`IRadientMaterialAsset`/`IRadientTextureAsset`. `RadientMeshPrimitives.h`
   adds convenience built-in-primitive helpers (`RadientCubeMeshCreateInfo`, sphere, etc.).
3. `engine->CreateScene(RadientSceneDesc, &pScene)`, then
   `engine->CreateSceneWriter(pScene, &pWriter)`. Populate via `pWriter->CreateEntity()` then
   `SetLocalTransform` / `SetCamera` / `SetMesh` / `SetMeshRenderer` / `SetMaterialBindings` /
   `SetLight` / `SetParent`, ending a batch of edits with `pWriter->CommitChanges()`
   (`interface/RadientScene.h`, `RadientSceneWriter.h`). Reads go through the separate
   `IRadientScene` interface (`GetWorldMatrix`, `GetCamera`, `HasComponent`, `GetSceneRevisions`,
   ...).
4. Optionally skip step 3's manual entity building:
   `engine->CreateSceneImporter(pWriter, &pImporter)` then
   `pImporter->ImportScene(RadientSceneLoadInfo{URI="model.gltf"}, RadientSceneInstantiateInfo{...}, &pSceneAsset, &RootEntity)`
   imports a whole GLTF graph in one call (`interface/RadientSceneImporter.h`); async imports
   return `RADIENT_STATUS_PENDING` and are advanced via `pImporter->ProcessPendingImports()`.
5. `engine->CreateRenderer(RadientRendererDesc, &pRenderer)` (`interface/RadientRenderer.h`) —
   configures async PSO compilation, material texture slot count, multi-draw batch size, PostFX
   fade duration.
6. `pRenderer->CreateRenderTarget(RadientRenderTargetDesc{pSwapChain=...}, &pTarget)` and
   `pRenderer->CreateView(RadientViewDesc{pScene, Camera, pRenderTarget, ToneMapping/Bloom/SSAO/SSR/TAA/DepthOfField/Skybox/Environment}, &pView)`
   (`interface/RadientView.h`) — a "view" bundles scene+camera+target+post-effect settings, one per
   output.
7. Per frame: mutate the scene through the writer as needed → `CommitChanges()` →
   `pRenderer->Render(RadientRenderAttribs{pView, DeltaTime, Time})`. `Render` can return
   `RADIENT_STATUS_PENDING` if some drawables are still waiting on async GPU upload/PSO compile;
   the frame still renders whatever is ready.

## 3. Does Tessera provide GPU-driven indirect-draw/culling?

**No — Radient/Tessera implements no culling whatsoever, not even basic CPU frustum culling, let
alone GPU-driven compute culling into an indirect-draw buffer.** Checked two ways: reading the
actual data flow, and an exhaustive case-insensitive grep of the *entire* Radient tree
(`interface/`, `include/`, `src/`) for `Indirect`, `DispatchCompute`, `ComputeShader`,
`BIND_INDIRECT`, `IndirectDrawArgs`, `Frustum`, `Cull`/`Culling` — **zero matches for any of them**.
`Occlusion` matches only the PBR material's ambient-occlusion *texture* map and a debug-
visualization enum (`RADIENT_DEBUG_VISUALIZATION_OCCLUSION` in `interface/RadientView.h`),
unrelated to occlusion culling.

What actually happens:
- **`RadientTesseraDrawableCache`** (`include/Render/Tessera/RadientTesseraDrawableCache.hpp`) is
  pure CPU bookkeeping: it expands each renderable scene entity's mesh into one
  `RadientDrawableSlot` per primitive and buckets them into `RadientDrawLists` **only by alpha
  mode** (opaque/mask/blend — 3 buckets, from `GLTF::Material::ALPHA_MODE_NUM_MODES`).
  `RadientDrawableSlot` has no bounding-volume field at all — a tree-wide grep for
  `AABB`/`BoundingBox`/`BoundingSphere` found matches only in the unrelated GLTF import/loader code
  (`src/Import/RadientGLTFConverter.cpp`, `src/Assets/RadientGLTFLoader.cpp`), never in any
  renderer-facing type. The only "visibility" concept present is the scene's own hierarchical
  `EffectiveVisible` show/hide flag — not camera-relative visibility.
- **`RadientTesseraGeometryPass::Execute()`** (`src/Render/Tessera/Passes/RadientTesseraGeometryPass.cpp`)
  iterates **every** drawable in `m_DrawableBatches` (grouped only by PSO/Material/VertexPool/
  IsIndexed key) and unconditionally builds one `MultiDrawItem`/`MultiDrawIndexedItem` per
  drawable, submitted via Diligent's `IDeviceContext::MultiDraw`/`MultiDrawIndexed` — native
  multi-draw when supported, else a per-item `DrawIndexed` loop using `FirstInstanceLocation`
  purely as a side-channel to emulate a primitive ID. This is a **CPU draw-call-count
  optimization** (fewer state changes, governed by `RadientRendererDesc::MultiDrawBatchSize`,
  default 16) — explicitly not a culling mechanism. "Many similar draws, most off-screen" would
  all still be *submitted*; nothing is skipped pre-GPU.
- Also checked for real geometry instancing as a possible alternate answer to repeated-chunk-block
  rendering — the only `Instanc*` hits are that same base-instance-as-primitive-ID trick, not
  instanced rendering of shared geometry across many transforms.

**Consequence for the project**: adopting Radient would not remove or shrink the GPU-driven
indirect-culling stretch goal. That work is completely orthogonal to what Radient provides and
would need to be hand-built regardless — exactly as much from scratch with Radient as without it.

## 4. README/docs/example check

- **`Radient/readme.md`**: effectively empty — just the title `# Radient`, no body.
- **`Radient/docs/README.md`**: a one-paragraph index pointing to two narrow convention docs:
  - `docs/CameraConventions.md` — axis/handedness conventions (glTF/OpenUSD-style, local `-Z`
    forward) and how `RadientCameraComponent` maps into the projection matrix.
  - `docs/LightConventions.md` — light direction/cone-angle/range conventions (glTF
    `KHR_lights_punctual`-derived).
  Useful as sign/axis reference for someone already integrating Radient, but neither is a
  getting-started guide, API doc, or anything about the render pipeline or performance.
- **No test, example, or sample target exists inside `Radient/` itself** — a targeted file search
  for test/example/sample paths under the Radient tree returned nothing; `CMakeLists.txt` defines
  exactly one target (`Radient`, a static library).
- **`DiligentSamples` has no real reference to Radient.** An initial case-insensitive grep for
  "Radient" across `DiligentSamples/` returned 5 files, but every one is a false positive on the
  substring "radient" inside the unrelated word **"gradient"** — confirmed by reading the matched
  lines, e.g. `Samples/Asteroids/src/simplexnoise1234.c:119`:
  `// Helper functions to compute gradients-dot-residualvectors`, and similar gradient-related text
  in `HemispherePS.fx`, `appveyor.yml`, `Tutorial21_RayTracing/readme.md`, and the Emscripten HTML
  template. There is no tutorial or sample demonstrating Radient anywhere.
- Radient **is** a real, actively-built part of the default build, though — not dead/excluded
  code: `DiligentFX/CMakeLists.txt` has `option(DILIGENT_NO_RADIENT "Do not build Radient" OFF)`
  (built by default; opt-*out*, not opt-in) and unconditionally `add_subdirectory(Radient)` plus
  installs its `interface/` headers alongside the rest of DiligentFX.
- All Radient license headers read "Copyright 2026 Diligent Graphics LLC" versus e.g.
  `PBR_Renderer.hpp`'s "Copyright 2019-2026" — consistent with Radient being newly added relative
  to PBR/PostProcess, which plausibly explains the documentation/sample gap rather than indicating
  abandonment.

Net: nothing shortcuts understanding Radient today. Every conclusion above was derived directly
from interface/implementation headers and a few `.cpp` bodies.

## 5. Recommendation

**Poor fit for the current terrain-rendering task; not worth adopting now.**

Reasoning:
- Radient's Tessera technique is wired end-to-end to consume drawables that originate from
  `IRadientScene` ECS entities, `IRadientMeshAsset`/`IRadientMaterialAsset` *assets*, and
  GLTF-shaped materials (`GLTF::Material` appears throughout `Render/` and `Render/Tessera/`).
  Feeding hand-authored/greedy-meshed voxel-chunk geometry through it means building each chunk as
  a `RadientMeshCreateInfo` and each terrain material as a `RadientMaterialCreateInfo` — real
  integration overhead, not a drop-in swap for a hand-written PSO/shader pair.
- It provides **zero** help toward the specific stretch goal this project is weighing Radient
  against — GPU-driven indirect-draw culling (Task 3) is completely absent from Radient. Adopting
  it wouldn't shrink that scope at all; it would still be entirely hand-built work layered either
  on Radient or on the existing hand-written pipeline.
- It has no working example anywhere in the repo to de-risk adoption, and it's a large, opinionated
  whole-scene framework (ECS + async asset system + GLTF importer + deferred PBR renderer + full
  PostFX stack) — a heavy dependency to take on for a narrower need.
- That said, it is a genuinely solid piece of engineering that reuses rather than reinvents
  DiligentFX's own building blocks (real incremental-dirty-tracked ECS scene graph, real async
  multithreaded material/texture pipeline, `RadientPBRRenderer : public PBR_Renderer`, and
  orchestration of DiligentFX's existing Bloom/SSAO(GTAO·HBAO·VBAO)/SSR/TAA/DOF PostProcess
  modules) — not a reason to avoid it in general, just a reason it doesn't match *this* task.

Worth revisiting later only if the project's scope broadens to general scene composition —
importing arbitrary GLTF props/characters alongside the terrain, wanting cameras/lights/materials
managed uniformly, or wanting the shipped PostFX stack without hand-rolling it. Even then, the more
sensible integration shape would be "keep the hand-written terrain PSO as its own pass and let
Radient own everything else in the scene," rather than routing chunk-terrain draws through
`RadientTesseraGeometryPass` — its per-primitive/GLTF-material assumptions and total absence of
culling make it a weak match for potentially very large voxel-chunk primitive counts specifically.
It is not a candidate for the GPU-driven-culling stretch goal under any scenario — that remains
orthogonal to Radient and must be hand-built either way.
