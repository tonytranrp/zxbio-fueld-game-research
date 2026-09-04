# DiligentEngine API Surface Research Report

> **Historical-citation note (2026-09-04):** brief filenames cited below (PROJECT_BRIEF.md,
> PHASE_1_BRIEF.md, M1_2_BRIEF.md, PHASE_1_COMPLETION_BRIEF.md, ENGINE_HARDENING_BRIEF.md) refer
> to root-level documents deleted in the docs migration to `docs/progress.md` + `docs/goals.md`.
> They remain retrievable from git history; citations kept verbatim as primary-evidence context.

Subagent A from `PHASE_1_BRIEF.md` §9, completed 2026-09-02. Read-only local-source investigation
(no web access) against the pinned DiligentEngine commit `aca2285`.

**Source root confirmed:** `C:\Users\Tonyt\.claude\cpm-cache\diligentengine\ba74\DiligentCore\`
(siblings `DiligentTools\`, `DiligentFX\` at same `ba74` level). All findings below are from direct
`Read`/`Grep`/`Glob` against this exact checkout — no web search, no general knowledge substituted.
Every claim cites an absolute file path and, where useful, exact line numbers from what was read.

---

## TASK 1 — Shader-variable classification (Static/Mutable/Dynamic) and binding methods

### The enum

**File:** `DiligentCore\Graphics\GraphicsEngine\interface\ShaderResourceVariable.h`, lines 49–67:

```cpp
DILIGENT_TYPED_ENUM(SHADER_RESOURCE_VARIABLE_TYPE, Uint8)
{
    /// Shader resource bound to the variable is the same for all SRB instances.
    /// It must be set *once* directly through Pipeline State object.
    SHADER_RESOURCE_VARIABLE_TYPE_STATIC = 0,

    /// Shader resource bound to the variable is specific to the shader resource binding
    /// instance (see Diligent::IShaderResourceBinding). It must be set *once* through
    /// Diligent::IShaderResourceBinding interface. It cannot be set through Diligent::IPipelineState
    /// interface and cannot be change once bound.
    SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE,

    /// Shader variable binding is dynamic. It can be set multiple times for every instance of shader resource
    /// binding (see Diligent::IShaderResourceBinding). It cannot be set through Diligent::IPipelineState interface.
    SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC,

    /// Total number of shader variable types
    SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES
};
```

Exact member names to use in code: `SHADER_RESOURCE_VARIABLE_TYPE_STATIC`,
`SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE`, `SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC`. This is a
`Uint8`-backed `DILIGENT_TYPED_ENUM`, value 0/1/2.

Same file also defines (lines 70–161, all confirmed by direct read):
- `SHADER_RESOURCE_VARIABLE_TYPE_FLAGS` (bitflag mirror: `_FLAG_STATIC`, `_FLAG_MUTABLE`,
  `_FLAG_DYNAMIC`, `_FLAG_MUT_DYN`, `_FLAG_ALL`)
- `BIND_SHADER_RESOURCES_FLAGS` (`BIND_SHADER_RESOURCES_UPDATE_STATIC/_MUTABLE/_DYNAMIC/_ALL`,
  `_KEEP_EXISTING`, `_VERIFY_ALL_RESOLVED`, `_ALLOW_OVERWRITE`)
- `SET_SHADER_RESOURCE_FLAGS` (`SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE`)

### Declaring a variable's tier

**File:** `PipelineState.h`, lines 140–189, struct `ShaderResourceVariableDesc`:
```cpp
struct ShaderResourceVariableDesc
{
    const Char*                   Name         DEFAULT_INITIALIZER(nullptr);
    SHADER_TYPE                   ShaderStages DEFAULT_INITIALIZER(SHADER_TYPE_UNKNOWN);
    SHADER_RESOURCE_VARIABLE_TYPE Type         DEFAULT_INITIALIZER(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    SHADER_VARIABLE_FLAGS         Flags        DEFAULT_INITIALIZER(SHADER_VARIABLE_FLAG_NONE);
    ...
};
```
This array is hung off `PipelineResourceLayoutDesc::Variables`/`NumVariables` (same file, lines
212–291), itself embedded in `PipelineStateDesc::ResourceLayout` (line 624), which is embedded in
`GraphicsPipelineStateCreateInfo::PSODesc` (via public inheritance — see Task 2's `DILIGENT_DERIVE`
note). `PipelineResourceLayoutDesc::DefaultVariableType` (line 218, default
`SHADER_RESOURCE_VARIABLE_TYPE_STATIC`) governs any resource name not explicitly listed in
`Variables`.

### Exact binding-method signatures per tier

**Static — bound directly on the PSO**, `PipelineState.h` interface `IPipelineState`
(`DILIGENT_BEGIN_INTERFACE(IPipelineState, IDeviceObject)`, lines 1200–1401):
```cpp
VIRTUAL void METHOD(BindStaticResources)(THIS_
                                         SHADER_TYPE                 ShaderStages,
                                         IResourceMapping*           pResourceMapping,
                                         BIND_SHADER_RESOURCES_FLAGS Flags) PURE;

VIRTUAL Uint32 METHOD(GetStaticVariableCount)(THIS_ SHADER_TYPE ShaderType) CONST PURE;

VIRTUAL IShaderResourceVariable* METHOD(GetStaticVariableByName)(THIS_
                                                                 SHADER_TYPE ShaderType,
                                                                 const Char* Name) PURE;

VIRTUAL IShaderResourceVariable* METHOD(GetStaticVariableByIndex)(THIS_
                                                                  SHADER_TYPE ShaderType,
                                                                  Uint32      Index) PURE;
```
Doc comment on `GetStaticVariableByName` (lines 1254–1268) is explicit: "This method is only
allowed for pipelines that use implicit resource signature… For pipelines that use explicit
resource signatures, use `IPipelineResourceSignature::GetStaticVariableByName()`." (i.e., if you
pass `ppResourceSignatures` explicitly in `PipelineStateCreateInfo` instead of relying on
`ResourceLayout`, static-variable access moves to `IPipelineResourceSignature` instead of
`IPipelineState`.)

**Mutable/Dynamic — bound through the SRB**, `ShaderResourceBinding.h` interface
`IShaderResourceBinding` (lines 58–157):
```cpp
VIRTUAL void METHOD(BindResources)(THIS_
                                   SHADER_TYPE                 ShaderStages,
                                   IResourceMapping*           pResMapping,
                                   BIND_SHADER_RESOURCES_FLAGS Flags) PURE;

VIRTUAL IShaderResourceVariable* METHOD(GetVariableByName)(THIS_
                                                           SHADER_TYPE ShaderType,
                                                           const Char* Name) PURE;

VIRTUAL Uint32 METHOD(GetVariableCount)(THIS_ SHADER_TYPE ShaderType) CONST PURE;

VIRTUAL IShaderResourceVariable* METHOD(GetVariableByIndex)(THIS_
                                                            SHADER_TYPE ShaderType,
                                                            Uint32      Index) PURE;
```
Doc comment on `GetVariableCount` (lines 127–134) confirms: "The method only counts mutable and
dynamic variables that can be accessed through the Shader Resource Binding object. Static
variables are accessed through the Shader object." (Note: "Shader object" here is slightly loose
wording in the header itself — the actual static accessors are on `IPipelineState`/
`IPipelineResourceSignature`, as shown above; `IShaderResourceBinding::GetVariableByName`/
`GetVariableByIndex` only ever return mutable/dynamic variables.)

**Actually setting a resource once you have an `IShaderResourceVariable*`** (same regardless of
tier, obtained via either of the two paths above), `ShaderResourceVariable.h` interface
`IShaderResourceVariable` (lines 175–287):
```cpp
VIRTUAL void METHOD(Set)(THIS_
                         IDeviceObject*            pObject,
                         SET_SHADER_RESOURCE_FLAGS Flags DEFAULT_VALUE(SET_SHADER_RESOURCE_FLAG_NONE)) PURE;

VIRTUAL void METHOD(SetArray)(THIS_
                              IDeviceObject* const*     ppObjects,
                              Uint32                    FirstElement,
                              Uint32                    NumElements,
                              SET_SHADER_RESOURCE_FLAGS Flags DEFAULT_VALUE(SET_SHADER_RESOURCE_FLAG_NONE)) PURE;

VIRTUAL void METHOD(SetBufferRange)(THIS_
                                    IDeviceObject*            pObject,
                                    Uint64                    Offset,
                                    Uint64                    Size,
                                    Uint32                    ArrayIndex DEFAULT_VALUE(0),
                                    SET_SHADER_RESOURCE_FLAGS Flags      DEFAULT_VALUE(SET_SHADER_RESOURCE_FLAG_NONE)) PURE;

VIRTUAL void METHOD(SetBufferOffset)(THIS_ Uint32 Offset, Uint32 ArrayIndex DEFAULT_VALUE(0)) PURE;
VIRTUAL void METHOD(SetInlineConstants)(THIS_ const void* pConstants, Uint32 FirstConstant, Uint32 NumConstants) PURE;
VIRTUAL SHADER_RESOURCE_VARIABLE_TYPE METHOD(GetType)(THIS) CONST PURE;
VIRTUAL IDeviceObject* METHOD(Get)(THIS_ Uint32 ArrayIndex DEFAULT_VALUE(0)) CONST PURE;
```

**Practical rule confirmed by the doc comments themselves:** for a terrain PSO you'll typically
want per-frame constants (camera matrices etc.) as `STATIC` (set once on the PSO right after
creation, then call `IPipelineState::CreateShaderResourceBinding(&pSRB, true)` — the
`Bool InitStaticResources` param, `PipelineState.h` line 1309, copies the static bindings into the
SRB at creation time) and any per-draw/per-material resource as `MUTABLE` (bound once on the SRB)
or `DYNAMIC` (rebindable every frame on the SRB without GPU sync in D3D11/OpenGL/Metal, but per the
`SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE` doc at lines 154–158, requires the app to ensure the GPU
isn't accessing the SRB when overwriting mutable bindings in D3D12/Vulkan).

No `IShader::BindResources` method exists — `IShader` (`Shader.h`) does not expose a
resource-binding method; resource binding is exclusively via `IPipelineState` (static tier) and
`IShaderResourceBinding` (mutable/dynamic tier), as shown above.

---

## TASK 2 — Minimal opaque-terrain `GraphicsPipelineStateCreateInfo` shape

**File:** `PipelineState.h`. Structure hierarchy (all read directly, lines cited):

### `GraphicsPipelineStateCreateInfo` (lines 905–963)
```cpp
struct GraphicsPipelineStateCreateInfo DILIGENT_DERIVE(PipelineStateCreateInfo)
    GraphicsPipelineDesc GraphicsPipeline;
    IShader* pVS DEFAULT_INITIALIZER(nullptr);
    IShader* pPS DEFAULT_INITIALIZER(nullptr);
    IShader* pDS DEFAULT_INITIALIZER(nullptr);
    IShader* pHS DEFAULT_INITIALIZER(nullptr);
    IShader* pGS DEFAULT_INITIALIZER(nullptr);
    IShader* pAS DEFAULT_INITIALIZER(nullptr);
    IShader* pMS DEFAULT_INITIALIZER(nullptr);
    ...
};
```
`DILIGENT_DERIVE(TypeName)` (defined in `Primitives\interface\CommonDefinitions.h`, lines 102–105)
expands, in the C++-interface build, to real `: public TypeName {` — i.e. this is genuine public
inheritance, not a nested struct. So a caller writes `PSOCreateInfo.PSODesc.Name = "Terrain PSO"`
and `PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = ...` directly, no extra nesting.

**Required for an opaque VS/PS terrain pipeline:** `pVS` and `pPS` (both default `nullptr` — must
be set; the other 5 stage pointers stay `nullptr` for a non-tessellated/non-mesh/non-geometry
pipeline).

### `PipelineStateCreateInfo` (base, lines 792–901)
- `PSODesc` (a `PipelineStateDesc`, see below)
- `Flags` default `PSO_CREATE_FLAG_NONE`
- `ResourceSignaturesCount` default 0, `ppResourceSignatures` default `nullptr` → **leaving this
  null is what makes the PSO build an *implicit* resource signature from
  `PSODesc.ResourceLayout`**, which is what makes `IPipelineState::GetStaticVariableByName` etc.
  from Task 1 valid.
- `NumSpecializationConstants` default 0, `pSpecializationConstants` default `nullptr`
- `pPSOCache` default `nullptr` (this is exactly the hook for `IRenderStateCache` — see Task 3)

### `PipelineStateDesc` (lines 600–662, `DILIGENT_DERIVE(DeviceObjectAttribs)` → carries `Name`)
- `PipelineType` default `PIPELINE_TYPE_GRAPHICS` (auto-set by the `GraphicsPipelineStateCreateInfo`
  constructor at line 934, so you don't need to set it yourself if you default-construct)
- `SRBAllocationGranularity` default 1
- `ImmediateContextMask` default 1 (bit 0 → immediate context index 0; must be widened if you plan
  to submit this PSO's draws from a different immediate-context index)
- `ResourceLayout` (a `PipelineResourceLayoutDesc`, see Task 1)

### `GraphicsPipelineDesc` (lines 297–406) — the actual fixed-function state

| Field | Default | Must you set it for opaque terrain? |
|---|---|---|
| `BlendDesc` | `BlendStateDesc{}` = no blending anywhere | No — default is correct for opaque |
| `SampleMask` | `0xFFFFFFFF` | No |
| `RasterizerDesc` | `RasterizerStateDesc{}` = solid fill, back-face cull | No — default matches typical terrain |
| `DepthStencilDesc` | `DepthStencilStateDesc{}` = depth test+write on, LESS, no stencil | No — default is correct for opaque terrain with a depth buffer |
| `InputLayout` | empty (`LayoutElements=nullptr, NumElements=0`) | **Yes** — custom position/normal/materialID vertex, must supply `LayoutElement[]` |
| `PrimitiveTopology` | `PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` | No — matches mesh topology |
| `NumViewports` | 1 | No, unless multi-viewport |
| `NumRenderTargets` | **0** | **Yes** — must set to at least 1 |
| `RTVFormats[8]` | all `TEX_FORMAT_UNKNOWN` | **Yes** — `RTVFormats[0]` must match your swap chain/target format |
| `DSVFormat` | `TEX_FORMAT_UNKNOWN` | **Yes, if you want a depth buffer** — `TEX_FORMAT_UNKNOWN` means *no* depth-stencil attachment at all, which combined with `DepthStencilDesc.DepthEnable=true` (the default!) would be an invalid/undefined combination — must both set a real `DSVFormat` (or explicitly disable depth) |
| `SmplDesc` | `{Count=1, Quality=0}` | No, unless MSAA |
| `pRenderPass` | `nullptr` | No (only needed for explicit Vulkan/D3D12-style subpass pipelines) |
| `NodeMask` | 0 | No (multi-GPU only) |

`DILIGENT_MAX_RENDER_TARGETS` is `8`, confirmed in `Constants.h` line 42.

**Sub-struct defaults, fully read and confirmed:**

- **`BlendStateDesc`** (`BlendState.h`, lines 379–447): `AlphaToCoverageEnable=False`,
  `IndependentBlendEnable=False`, `RenderTargets[8]` each defaulting to
  `RenderTargetBlendDesc{}` = `BlendEnable=False, ..., RenderTargetWriteMask=COLOR_MASK_ALL`.
  Correct opaque default — no changes needed.
- **`RasterizerStateDesc`** (`RasterizerState.h`, lines 96–205): `FillMode=FILL_MODE_SOLID`,
  `CullMode=CULL_MODE_BACK`, `FrontCounterClockwise=False`, `DepthClipEnable=True`,
  `ScissorEnable=False`, `AntialiasedLineEnable=False`, `DepthBias=0`, `DepthBiasClamp=0.f`,
  `SlopeScaledDepthBias=0.f`. Matches a typical back-face-culled solid terrain mesh out of the box.
- **`DepthStencilStateDesc`** (`DepthStencilState.h`, lines 160–239): `DepthEnable=True`,
  `DepthWriteEnable=True`, `DepthFunc=COMPARISON_FUNC_LESS`, `StencilEnable=False`,
  `StencilReadMask=0xFF`, `StencilWriteMask=0xFF`, `FrontFace`/`BackFace` = `StencilOpDesc{}`
  (`KEEP/KEEP/KEEP`, `COMPARISON_FUNC_ALWAYS`). Correct default for standard opaque depth-tested
  terrain.
- **`InputLayoutDesc`** / **`LayoutElement`** (`InputLayout.h`, lines 69–255): must supply
  `LayoutElements` (pointer to an array) and `NumElements`. Per-element defaults:
  `HLSLSemantic="ATTRIB"` (this default is what lets the same HLSL source cross-compile to Vulkan
  SPIR-V/GLSL — a non-default semantic "will only work in Direct3D11 and Direct3D12 backends", per
  the doc comment lines 71–76 — **important**: keep default `"ATTRIB"` since Vulkan is the primary
  backend), `InputIndex=0` (matches the HLSL `ATTRIBn` register bound to), `BufferSlot=0`,
  `NumComponents=0` (**no sensible default — must set**, e.g. 3 for position/normal, 1 for a scalar
  materialID), `ValueType=VT_FLOAT32` (must override if materialID's actual type differs),
  `IsNormalized=True` (only affects integer `VALUE_TYPE`s),
  `RelativeOffset=LAYOUT_ELEMENT_AUTO_OFFSET` (0xFFFFFFFF sentinel → auto-packed),
  `Stride=LAYOUT_ELEMENT_AUTO_STRIDE`, `Frequency=INPUT_ELEMENT_FREQUENCY_PER_VERTEX`,
  `InstanceDataStepRate=1`.

### Shader objects feeding `pVS`/`pPS`

**File:** `Shader.h`. `ShaderCreateInfo` (lines 466–742) — key fields with defaults, all read
directly:
- `FilePath` / `Source` / `ByteCode` — mutually exclusive (only one non-null); doc comments at
  lines 470, 481, 486 state this explicitly.
- `pShaderSourceStreamFactory` — needed when using `FilePath` (also feeds `#include` resolution).
- `SourceLength`/`ByteCodeSize` — a union (lines 503–518); zero-length `Source` is treated as
  null-terminated.
- `EntryPoint` default `"main"`.
- `Desc` — a `ShaderDesc` (line 137, `DILIGENT_DERIVE(DeviceObjectAttribs)`): carries `ShaderType`
  (default `SHADER_TYPE_UNKNOWN` — **must set** to `SHADER_TYPE_VERTEX`/`SHADER_TYPE_PIXEL`, exact
  names `SHADER_TYPE_VERTEX = 0x0001`, `SHADER_TYPE_PIXEL = 0x0002` in `GraphicsTypes.h` lines
  73–74) plus `UseCombinedTextureSamplers`/`CombinedSamplerSuffix`.
- `SourceLanguage` default `SHADER_SOURCE_LANGUAGE_DEFAULT` (GLSL on Vulkan!) — **must explicitly
  set `SHADER_SOURCE_LANGUAGE_HLSL`** for hand-written HLSL.
- `ShaderCompiler` default `SHADER_COMPILER_DEFAULT` (on Vulkan+HLSL this resolves to "built-in
  glslang (with limited support for Shader Model 6.x)" per the enum doc at `Shader.h` line 105;
  pass `SHADER_COMPILER_DXC` explicitly if SM6.x features are needed).
- `HLSLVersion`/`GLSLVersion`/etc. all default `{0,0}` = "use highest supported".
- `CompileFlags` default `SHADER_COMPILE_FLAG_NONE`, `ShaderOptimizationLevel` default
  `SHADER_OPTIMIZATION_LEVEL_DEFAULT`, `LoadConstantBufferReflection` default `false`.

Creation call, **`RenderDevice.h`** lines 115–118:
```cpp
VIRTUAL void METHOD(CreateShader)(THIS_
                                  const ShaderCreateInfo REF ShaderCI,
                                  IShader**                  ppShader,
                                  IDataBlob**                ppCompilerOutput DEFAULT_VALUE(nullptr)) PURE;
```

PSO creation call, **`RenderDevice.h`** lines 186–188:
```cpp
VIRTUAL void METHOD(CreateGraphicsPipelineState)(THIS_
                                                 const GraphicsPipelineStateCreateInfo REF PSOCreateInfo,
                                                 IPipelineState**                          ppPipelineState) PURE;
```

### Matching RTV/DSV format to the actual swap chain

**File:** `GraphicsTypes.h`, `SwapChainDesc` (lines 1476–1550): `ColorBufferFormat` default
`TEX_FORMAT_RGBA8_UNORM_SRGB`, `DepthBufferFormat` default `TEX_FORMAT_D32_FLOAT`. Query the real
(possibly overridden) values via `pSwapChain->GetDesc()` (`SwapChain.h` line 63) after swap chain
creation, and feed them into `GraphicsPipeline.RTVFormats[0]` / `GraphicsPipeline.DSVFormat` —
don't hardcode the defaults, since `EngineFactoryVk::CreateSwapChainVk` takes a caller-supplied
`SwapChainDesc` that may not match.

---

## TASK 3 — `IRenderStateCache` API, `Reload()`, and the retrofit question

**File:** `DiligentCore\Graphics\GraphicsTools\interface\RenderStateCache.h` (note: this lives in
**GraphicsTools**, not GraphicsEngine — a separate CMake target that must be linked).

### Creation

`RenderStateCacheCreateInfo` (lines 77–139):
```cpp
struct RenderStateCacheCreateInfo
{
    IRenderDevice* pDevice DEFAULT_INITIALIZER(nullptr);                       // required, not null
    struct IArchiverFactory* pArchiverFactory DEFAULT_INITIALIZER(nullptr);    // required, not null
    RENDER_STATE_CACHE_LOG_LEVEL LogLevel DEFAULT_INITIALIZER(RENDER_STATE_CACHE_LOG_LEVEL_NORMAL);
    RENDER_STATE_CACHE_FILE_HASH_MODE FileHashMode DEFAULT_INITIALIZER(RENDER_STATE_CACHE_FILE_HASH_MODE_BY_CONTENT);
    bool EnableHotReload DEFAULT_INITIALIZER(false);
    bool OptimizeGLShaders DEFAULT_INITIALIZER(true);
    IShaderSourceInputStreamFactory* pReloadSource DEFAULT_INITIALIZER(nullptr);
};
```
Global creation function (lines 329–330, via `DILIGENT_GLOBAL_FUNCTION` macro — resolves to
`Diligent::CreateRenderStateCache` in the C++ build):
```cpp
void DILIGENT_GLOBAL_FUNCTION(CreateRenderStateCache)(const RenderStateCacheCreateInfo REF CreateInfo,
                                                      IRenderStateCache**                  ppCache);
```
Confirmed in `DiligentCore\Graphics\GraphicsTools\src\RenderStateCacheImpl.cpp` constructor (lines
154–230, read directly) that `pDevice==nullptr` and `pArchiverFactory==nullptr` both
`LOG_ERROR_AND_THROW` — these two fields are hard requirements, not soft defaults.
`pArchiverFactory` doc comment says to obtain it via `LoadAndGetArchiverFactory()` from
`ArchiverFactoryLoader.h`.

Also confirmed at constructor lines 174–177:
```cpp
if (CreateInfo.FileHashMode == RENDER_STATE_CACHE_FILE_HASH_MODE_BY_NAME && CreateInfo.EnableHotReload)
    LOG_WARNING_MESSAGE("Hot reloading is not compatible with by-name file hashing. Use by-content hashing instead.");
```
— i.e. `RENDER_STATE_CACHE_FILE_HASH_MODE_BY_CONTENT` (the default) is required for real hot
reload; `BY_NAME` only warns (doesn't hard-fail) but silently won't detect content changes.

### `Reload()` exact signature

**File:** `RenderStateCache.h`, lines 289–291:
```cpp
VIRTUAL Uint32 METHOD(Reload)(THIS_
                              ReloadGraphicsPipelineCallbackType ReloadGraphicsPipeline DEFAULT_VALUE(nullptr),
                              void*                              pUserData              DEFAULT_VALUE(nullptr)) PURE;
```
Returns the number of shaders+pipelines actually reloaded. Callback typedef, line 144:
```cpp
typedef void(DILIGENT_CALL_TYPE* ReloadGraphicsPipelineCallbackType)(const char* PipelineName, GraphicsPipelineDesc REF GraphicsDesc, void* pUserData);
```
This callback lets you patch `GraphicsPipelineDesc` (e.g. re-supply an `InputLayout` pointer that
may have gone stale) right before a graphics PSO is recreated during reload.

Confirmed in `RenderStateCacheImpl.cpp` (lines 921–978, read directly): if
`!m_CI.EnableHotReload`, `Reload()` immediately `DEV_ERROR`s and returns 0 — calling `Reload()` on
a cache created with `EnableHotReload=false` is a hard no-op with a dev-mode error, not silently
ignored.

Also: `CreateShader`/`CreateGraphicsPipelineState`/etc. on the cache return `bool` (not `void`) —
`true` means the object was rehydrated straight from the cache (skip-compile fast path), `false`
means it was compiled fresh and added to the cache. This is a materially different signature from
`IRenderDevice::CreateShader`/`CreateGraphicsPipelineState`, which return `void`.

### The retrofit question — definitively answered: NO, hot reload cannot be retrofitted

This required reading the implementation, not just the header. Evidence, all from
`RenderStateCacheImpl.cpp` and `RenderStateCacheImpl.hpp` (both under
`DiligentCore\Graphics\GraphicsTools\`):

1. **`RenderStateCacheImpl.hpp`** (lines 144–154) declares two tracking maps:
```cpp
std::mutex                                                   m_ReloadableShadersMtx;
std::unordered_map<UniqueIdentifier, RefCntWeakPtr<IShader>> m_ReloadableShaders;

std::mutex                                                          m_ReloadablePipelinesMtx;
std::unordered_map<UniqueIdentifier, RefCntWeakPtr<IPipelineState>> m_ReloadablePipelines;
```
2. **`RenderStateCacheImpl.cpp::CreateShader()`** (lines 241–300, read directly): after the
   underlying `IShader` is created (via `CreateShaderInternal`, which itself calls
   `m_pDevice->CreateShader(ShaderCI, ppShader)` at line 481 when not already cached), *only if*
   `m_CI.EnableHotReload` is true does it wrap the shader:
```cpp
ReloadableShader::Create(this, pShader, _ShaderCI, ppShader);
...
m_ReloadableShaders.emplace(pShader->GetUniqueID(), RefCntWeakPtr<IShader>{*ppShader});
```
3. **Pipeline creation** funnels all four entry points (`CreateGraphicsPipelineState`/
   `CreateComputePipelineState`/`CreateRayTracingPipelineState`/`CreateTilePipelineState` —
   confirmed in `RenderStateCacheImpl.hpp` lines 67–93, each a one-line forwarder) into one private
   template `CreatePipelineState<CreateInfoType>()`, which (grep-confirmed at
   `RenderStateCacheImpl.cpp` lines 743–763) does the identical wrap-and-register dance:
```cpp
ReloadablePipelineState::Create(this, pPSO, PSOCreateInfo, ppPipelineState);
...
m_ReloadablePipelines.emplace(pPSO->GetUniqueID(), RefCntWeakPtr<IPipelineState>(*ppPipelineState));
```
4. **`Reload()`** (lines 921–978) iterates **only** `m_ReloadableShaders` and
   `m_ReloadablePipelines` — nothing else:
```cpp
for (auto shader_it : m_ReloadableShaders) { ... pReloadableShader->Reload() ... }
...
for (auto pso_it : m_ReloadablePipelines) { ... pReloadablePSO->Reload(ReloadGraphicsPipeline, pUserData) ... }
```

**Conclusion, grounded in this trace:** an `IShader`/`IPipelineState` created directly via
`IRenderDevice::CreateShader()`/`CreateGraphicsPipelineState()` (bypassing the cache entirely) is
never inserted into `m_ReloadableShaders`/`m_ReloadablePipelines`, and is therefore invisible to
`Reload()` forever — there is no "register this pre-existing object with the cache" API anywhere
in `IRenderStateCache`'s public interface (all 12 methods checked, lines 164–303: `Load`,
`CreateShader`, `CreateGraphicsPipelineState`, `CreateComputePipelineState`,
`CreateRayTracingPipelineState`, `CreateTilePipelineState`, `WriteToBlob`, `WriteToStream`,
`Reset`, `Reload`, `GetContentVersion`, `GetReloadVersion` — none accept an already-existing
`IShader*`/`IPipelineState*`). **Every shader and every PSO that needs hot reload must be created
through `IRenderStateCache::CreateShader()` / `IRenderStateCache::CreateGraphicsPipelineState()`
from the very start of the object's life**, with a cache constructed with `EnableHotReload=true`
and `FileHashMode=RENDER_STATE_CACHE_FILE_HASH_MODE_BY_CONTENT`. Direct implication for
`pso_terrain.cpp`: if the terrain PSO should be hot-reloadable later, route its
`CreateShader`/`CreateGraphicsPipelineState` calls through the cache from the first line of code,
not add it on later.

---

## TASK 4 — Vulkan native-handle interfaces (device/queue/command-buffer access for Tracy)

Confirmed: Diligent **does** expose a full family of Vulkan-specific interfaces, directly
analogous to the `IPipelineStateGL::GetGLProgramHandle` pattern. Directory listing of
`DiligentCore\Graphics\GraphicsEngineVulkan\interface\` shows: `BottomLevelASVk.h`,
`BufferViewVk.h`, `BufferVk.h`, `CommandQueueVk.h`, `DeviceContextVk.h`, `DeviceMemoryVk.h`,
`EngineFactoryVk.h`, `FenceVk.h`, `FramebufferVk.h`, `PipelineStateCacheVk.h`, `PipelineStateVk.h`,
`QueryVk.h`, `RenderDeviceVk.h`, `RenderPassVk.h`, `SamplerVk.h`, `ShaderBindingTableVk.h`,
`ShaderResourceBindingVk.h`, `ShaderVk.h`, `SwapChainVk.h`, `TextureViewVk.h`, `TextureVk.h`,
`TopLevelASVk.h`.

### `VkPhysicalDevice` / `VkDevice` / `VkInstance`

**File:** `Graphics\GraphicsEngineVulkan\interface\RenderDeviceVk.h`, interface
`IRenderDeviceVk : IRenderDevice` (lines 51–172):
```cpp
VIRTUAL VkDevice         METHOD(GetVkDevice)(THIS) PURE;          // line 54
VIRTUAL VkPhysicalDevice  METHOD(GetVkPhysicalDevice)(THIS) PURE; // line 57
VIRTUAL VkInstance        METHOD(GetVkInstance)(THIS) PURE;       // line 60
VIRTUAL Uint32             METHOD(GetVkVersion)(THIS) PURE;       // line 65
```
Also on this interface: `CreateTextureFromVulkanImage`, `CreateBufferFromVulkanResource`,
`CreateBLASFromVulkanResource`, `CreateTLASFromVulkanResource`, `CreateFenceFromVulkanResource`,
`GetDeviceFeaturesVk`, `GetDXCompiler`. `IID_RenderDeviceVk` GUID declared lines 37–39.

Obtain it from an `IRenderDevice*` via the standard `IObject::QueryInterface` pattern (see
`Primitives\interface\Object.h`, lines 41–63 — includes both the raw
`QueryInterface(const INTERFACE_ID&, IObject**)` and a templated overload that avoids the manual
cast: `template<typename DerivedType> void QueryInterface(const INTERFACE_ID& IID, DerivedType** ppInterface)`).

### `VkCommandBuffer` currently being recorded by an immediate context

**File:** `Graphics\GraphicsEngineVulkan\interface\DeviceContextVk.h`, interface
`IDeviceContextVk : IDeviceContext` (lines 52–87):
```cpp
/// Returns the Vulkan handle of the command buffer currently being recorded
/// Any command on the device context may potentially submit the command buffer for
/// execution into the command queue and make it invalid. An application should
/// never cache the handle and should instead request the command buffer every time it
/// needs it.
VIRTUAL VkCommandBuffer METHOD(GetVkCommandBuffer)(THIS) PURE;   // line 86
```
Also on this interface: `TransitionImageLayout(ITexture*, VkImageLayout)`,
`BufferMemoryBarrier(IBuffer*, VkAccessFlags)`.

**Implementation detail confirmed by reading `Graphics\GraphicsEngineVulkan\src\DeviceContextVkImpl.cpp`
lines 3278–3283** (not just the header):
```cpp
VkCommandBuffer DeviceContextVkImpl::GetVkCommandBuffer()
{
    EnsureVkCmdBuffer();
    m_CommandBuffer.FlushBarriers();
    return m_CommandBuffer.GetVkCmdBuffer();
}
```
So the call is always safe (lazily begins a command buffer if none is currently active), and it
**flushes pending pipeline barriers** before returning the handle. **Critical implication for
Tracy**: per the header's own warning, Diligent may submit and swap the underlying
`VkCommandBuffer` at essentially any Diligent API call — so `TracyVkContext`'s own setup call is
fine to cache once, but any `TracyVkZone`/`TracyVkCollect` call must re-fetch `GetVkCommandBuffer()`
fresh, immediately before use, every single time — never hold the handle across other Diligent
calls.

### `VkQueue`

Not on `IDeviceContextVk` directly. Two-step path, both confirmed:

1. **File:** `Graphics\GraphicsEngine\interface\DeviceContext.h`, base `IDeviceContext` interface,
   lines 3731–3750:
```cpp
/// Locks the internal mutex and returns a pointer to the command queue that is associated with this device context.
/// Only immediate device contexts have associated command queues.
/// An application must release the lock by calling UnlockCommandQueue()...
/// The queue pointer never changes while the context is alive, so an application may cache and
/// use the pointer if it does not need to prevent potential simultaneous access...
VIRTUAL ICommandQueue* METHOD(LockCommandQueue)(THIS) PURE;   // line 3747
VIRTUAL void METHOD(UnlockCommandQueue)(THIS) PURE;           // line 3750
```
2. **File:** `Graphics\GraphicsEngineVulkan\interface\CommandQueueVk.h`, interface
   `ICommandQueueVk : ICommandQueue` (lines 51–95):
```cpp
/// Returns Vulkan command queue handle. May return VK_NULL_HANDLE if queue is unavailable
/// Access to the VkQueue must be externally synchronized.
/// Don't use this method to submit commands directly, use SubmitCmdBuffer() or Submit(), which are thread-safe.
VIRTUAL VkQueue METHOD(GetVkQueue)(THIS) PURE;   // line 80
```
Also on `ICommandQueueVk`: `SubmitCmdBuffer(VkCommandBuffer)→Uint64`,
`Submit(const VkSubmitInfo&)→Uint64`, `Present(const VkPresentInfoKHR&)→VkResult`, `BindSparse(...)`,
`GetQueueFamilyIndex()→uint32_t`, `EnqueueSignalFence`, `EnqueueSignal`. `IID_CommandQueueVk` GUID
at lines 37–39. Base `ICommandQueue` (`Graphics\GraphicsEngine\interface\CommandQueue.h`, lines
50–60) only adds `GetNextFenceValue()`, `GetCompletedFenceValue()`, `WaitForIdle()`.

**Full pattern to feed `TracyVkContext(physDevice, device, queue, cmdBuffer)`, entirely grounded in
the above:**
```cpp
RefCntAutoPtr<IRenderDeviceVk> pDeviceVk;
pDevice->QueryInterface(IID_RenderDeviceVk, reinterpret_cast<IObject**>(static_cast<IRenderDeviceVk**>(&pDeviceVk)));
VkPhysicalDevice physDevice = pDeviceVk->GetVkPhysicalDevice();
VkDevice         device     = pDeviceVk->GetVkDevice();

ICommandQueue* pQueueBase = pImmediateContext->LockCommandQueue();
RefCntAutoPtr<ICommandQueueVk> pQueueVk;
pQueueBase->QueryInterface(IID_CommandQueueVk, reinterpret_cast<IObject**>(static_cast<ICommandQueueVk**>(&pQueueVk)));
VkQueue queue = pQueueVk->GetVkQueue();
pImmediateContext->UnlockCommandQueue();   // per the doc, release once done reading the pointer/handle

RefCntAutoPtr<IDeviceContextVk> pContextVk;
pImmediateContext->QueryInterface(IID_DeviceContextVk, reinterpret_cast<IObject**>(static_cast<IDeviceContextVk**>(&pContextVk)));
VkCommandBuffer cmdBuffer = pContextVk->GetVkCommandBuffer();   // fetch fresh every time, never cache
```
No dedicated `IPhysicalDeviceVk`-style separate interface was found — `VkPhysicalDevice`/
`VkDevice`/`VkInstance` all hang directly off `IRenderDeviceVk`, as shown. No example of Tracy
integration exists anywhere in this pinned source tree (grep for "Tracy" across the whole
`GraphicsEngineVulkan` directory returned nothing) — the wiring above is composed from confirmed
individual API pieces, not a copied reference implementation; test the `LockCommandQueue`/
`UnlockCommandQueue` pairing carefully against `TracyVkContext`'s own one-time setup-submission
requirement.

---

## TASK 5 — Debug-naming facility

**Confirmed: every `*Desc` struct carries a `Name` field via a common base struct; there is no
separate `SetName()`-style method anywhere on `IDeviceObject`.**

### The base struct

**File:** `Graphics\GraphicsEngine\interface\GraphicsTypes.h`, lines 1316–1332:
```cpp
struct DeviceObjectAttribs
{
    /// Object name
    const Char* Name DEFAULT_INITIALIZER(nullptr);
    ...
};
typedef struct DeviceObjectAttribs DeviceObjectAttribs;
```
Every object-description struct derives from it via `DILIGENT_DERIVE(DeviceObjectAttribs)` —
confirmed directly for `PipelineStateDesc DILIGENT_DERIVE(DeviceObjectAttribs)` (`PipelineState.h`
line 600) and `ShaderDesc DILIGENT_DERIVE(DeviceObjectAttribs)` (`Shader.h` line 137); the pattern
is structurally confirmed a third way for `BufferDesc` too (`BufferVkImpl.cpp` reads
`m_Desc.Name` directly).

`DILIGENT_DERIVE` is real public C++ inheritance in the C++-interface build, so `.Name` is a plain
top-level member, e.g. `BufferDesc BuffDesc; BuffDesc.Name = "Terrain VB";`.

### No `SetName()` method exists

**File:** `Graphics\GraphicsEngine\interface\DeviceObject.h`, full interface
`IDeviceObject : IObject` (lines 52–100, entire file read). Its only four methods:
```cpp
VIRTUAL const DeviceObjectAttribs REF METHOD(GetDesc)(THIS) CONST PURE;
VIRTUAL Int32 METHOD(GetUniqueID)(THIS) CONST PURE;
VIRTUAL void METHOD(SetUserData)(THIS_ IObject* pUserData) PURE;
VIRTUAL IObject* METHOD(GetUserData)(THIS) CONST PURE;
```
No `SetName`/`Rename` anywhere. **The name is write-once, supplied in the `*Desc.Name` field at
the moment the corresponding `Create*` method is called, and is immutable for the rest of the
object's life** — there is no API to rename an object after creation.

### Confirmed honored by the Vulkan backend specifically (traced end-to-end, not assumed)

**File:** `Graphics\GraphicsEngineVulkan\src\VulkanUtilities\Debug.cpp` — the real mechanism:
- Lines 232–233: `SetDebugUtilsObjectNameEXT` is loaded via
  `vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT")` inside `SetupDebugUtils()`,
  only invoked when the `VK_EXT_debug_utils` extension / validation layers are active.
- Lines 324–340, `SetObjectName()`:
```cpp
void SetObjectName(VkDevice device, uint64_t objectHandle, VkObjectType objectType, const char* name)
{
    if (SetDebugUtilsObjectNameEXT == nullptr || name == nullptr || name[0] == '\0')
        return;   // silent no-op if validation/debug-utils isn't enabled, or name is empty
    VkDebugUtilsObjectNameInfoEXT ObjectNameInfo{};
    ObjectNameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    ObjectNameInfo.objectType   = objectType;
    ObjectNameInfo.objectHandle = objectHandle;
    ObjectNameInfo.pObjectName  = name;
    VkResult res = SetDebugUtilsObjectNameEXT(device, &ObjectNameInfo);
    ...
}
```
- Typed one-line wrappers for every Vulkan handle type exist (lines 359–472): `SetCommandPoolName`,
  `SetCommandBufferName`, `SetQueueName`, `SetImageName`, `SetImageViewName`, `SetSamplerName`,
  `SetBufferName`, `SetBufferViewName`, `SetDeviceMemoryName`, `SetShaderModuleName`,
  `SetPipelineName`, `SetPipelineLayoutName`, `SetRenderPassName`, `SetFramebufferName`,
  `SetDescriptorSetLayoutName`, `SetDescriptorSetName`, `SetDescriptorPoolName`, `SetSemaphoreName`,
  `SetFenceName`, `SetEventName`, `SetQueryPoolName`, `SetAccelStructName`, `SetPipelineCacheName`
  — plus a templated dispatcher `SetVulkanObjectName<T, VulkanHandleTypeId::X>` (lines 475–611).

**Traced two concrete call sites all the way from `*Desc.Name` to the real Vulkan API call:**
- `Graphics\GraphicsEngineVulkan\src\VulkanUtilities\LogicalDevice.cpp`, `CreateBuffer()` (lines
  187–192) and `CreateGraphicsPipeline()`/`CreateComputePipeline()`/`CreateRayTracingPipeline()`
  (lines 264–324) all take a `const char* DebugName` parameter and internally call
  `SetPipelineName(m_VkDevice, vkPipeline, DebugName)` (confirmed at lines 279, 299, 318) or the
  equivalent per-type helper.
- `Graphics\GraphicsEngineVulkan\src\BufferVkImpl.cpp`, lines 190 and 230:
  `m_VulkanBuffer = LogicalDevice.CreateBuffer(VkBuffCI, m_Desc.Name);` — `BufferDesc::Name` flows
  straight through.
- `Graphics\GraphicsEngineVulkan\src\PipelineStateVkImpl.cpp`, lines 240, 475, 509:
  `Pipeline = LogicalDevice.CreateComputePipeline(PipelineCI, vkPSOCache, PSODesc.Name);` /
  `...CreateGraphicsPipeline(PipelineCI, vkPSOCache, PSODesc.Name);` /
  `...CreateRayTracingPipeline(PipelineCI, vkPSOCache, PSODesc.Name);` — `PipelineStateDesc::Name`
  flows straight into the real `vkSetDebugUtilsObjectNameEXT` call, visible in
  RenderDoc/Nsight/Tracy's own Vulkan object inspector.

Confirmed concretely for `Buffer` and `Pipeline` objects (the two call sites traced fully), and by
the same established convention (a typed `SetXxxName` helper per Vulkan object type in
`Debug.cpp`) for every other object type Diligent creates. The mechanism is entirely conditional on
validation/`VK_EXT_debug_utils` being active — in a pure release build without
`EngineCreateInfo::EnableValidation`, `SetDebugUtilsObjectNameEXT` stays null and every name-set
call becomes a silent no-op (by design, no runtime cost in release).

---

## TASK 6 — Deferred-context API shape

### How many, and when created

**File:** `Graphics\GraphicsEngine\interface\GraphicsTypes.h`, `struct EngineCreateInfo` (from
line 3489), fields read directly:
```cpp
const ImmediateContextCreateInfo* pImmediateContextInfo DEFAULT_INITIALIZER(nullptr);  // line 3512
Uint32 NumImmediateContexts DEFAULT_INITIALIZER(0);                                     // line 3519
    // doc: "If not specified, single graphics context will be created."
    // warning: "If an application uses more than one immediate context, it must
    //           manually call IDeviceContext::FinishFrame for additional contexts..."

Uint32 NumDeferredContexts DEFAULT_INITIALIZER(0);                                      // line 3532
    // doc: "If non-zero number is given, pointers to the contexts are written to
    //       ppContexts array by the engine factory functions ... starting at
    //       position max(1, NumImmediateContexts)."
    // remark: "Additional deferred contexts may be created later by calling
    //          IRenderDevice::CreateDeferredContext()."
    // warning: "An application must manually call IDeviceContext::FinishFrame for
    //           deferred contexts to let the engine release stale resources."
```
So: `NumDeferredContexts` is a plain count passed at device-creation time via `EngineVkCreateInfo`
(which `DILIGENT_DERIVE`s `EngineCreateInfo`, confirmed `GraphicsTypes.h` line 4049), and the
resulting `IDeviceContext*` pointers land in the `ppContexts` output array of:
```cpp
// EngineFactoryVk.h, lines 78-81
VIRTUAL void METHOD(CreateDeviceAndContextsVk)(THIS_
                                               const EngineVkCreateInfo REF EngineCI,
                                               IRenderDevice**              ppDevice,
                                               IDeviceContext**             ppContexts) PURE;
```
starting at index `max(1, NumImmediateContexts)` (index 0 is always at least one immediate
context, even if none were explicitly requested).

**Additional deferred contexts after the fact**, `RenderDevice.h` lines 352–353:
```cpp
VIRTUAL void METHOD(CreateDeferredContext)(THIS_ IDeviceContext** ppContext) PURE;
```

`ImmediateContextCreateInfo` (`GraphicsTypes.h`, lines 3438–3470) — what to supply per-immediate-
context if more than the single default is wanted: `Name`, `QueueId` (default `DEFAULT_QUEUE_ID`),
`Priority` (a `QUEUE_PRIORITY` enum, default `QUEUE_PRIORITY_MEDIUM`; doc notes "Vulkan backend:
all contexts with the same QueueId must use the same priority").

`DeviceContextDesc` (`DeviceContext.h`, lines 70–148), what `IDeviceContext::GetDesc()` returns for
any context, immediate or deferred: `Name`, `QueueType` (a `COMMAND_QUEUE_TYPE` bitmask — exact
values in `GraphicsTypes.h` lines 2628–2650: `COMMAND_QUEUE_TYPE_UNKNOWN=0`, `_TRANSFER=1<<0`,
`_COMPUTE=(1<<1)|_TRANSFER`, `_GRAPHICS=(1<<2)|_COMPUTE` — i.e. graphics implies compute implies
transfer, so a plain bitmask test like `QueueType & COMMAND_QUEUE_TYPE_COMPUTE` is meaningful),
`IsDeferred` (Bool), `ContextId` (Uint8 — index into the `ppContexts` array), `QueueId` (Uint8 —
"Vulkan backend: same as queue family index" per the doc comment, line 100),
`TextureCopyGranularity[3]`.

### Recording → submission lifecycle, exact methods

**File:** `DeviceContext.h`.

1. **Begin recording** (only meaningful on a deferred context), lines 2621–2632:
```cpp
/// This method must be called before any command in the deferred context may be recorded.
/// \param [in] ImmediateContextId - the ID of the immediate context where commands from this
///                                  deferred context will be executed, see DeviceContextDesc::ContextId.
/// \warning Command list recorded by the context must not be submitted to any other immediate
///          context other than one identified by ImmediateContextId.
VIRTUAL void METHOD(Begin)(THIS_ Uint32 ImmediateContextId) PURE;
```
2. **Record** — ordinary draw/state calls on the deferred `IDeviceContext*`, exactly as on an
   immediate one.
3. **Finish recording**, lines 3200–3204:
```cpp
/// Finishes recording commands and generates a command list.
VIRTUAL void METHOD(FinishCommandList)(THIS_ ICommandList** ppCommandList) PURE;
```
4. **Submit, on the target *immediate* context** — lines 3207–3214:
```cpp
/// Submits an array of recorded command lists for execution.
/// \remarks After a command list is executed, it is no longer valid and must be released.
VIRTUAL void METHOD(ExecuteCommandLists)(THIS_
                                         Uint32               NumCommandLists,
                                         ICommandList* const* ppCommandLists) PURE;
```
5. **Per-frame lifecycle requirement**, lines 3478–3493:
```cpp
/// Finishes the current frame and releases dynamic resources allocated by the context.
/// For immediate context, this method is called automatically by ISwapChain::Present() of the primary
/// swap chain, but can also be called explicitly. For deferred contexts, the method must be called by the
/// application to release dynamic resources. ...
/// For deferred contexts, this method must be called after all command lists referencing dynamic resources
/// have been executed through immediate context.
/// The method does not Flush() the context.
VIRTUAL void METHOD(FinishFrame)(THIS) PURE;
```
So: **every deferred context needs an explicit, application-driven `FinishFrame()` call every
frame, after its command list(s) have been executed via the immediate context's
`ExecuteCommandLists`** — unlike the single primary immediate context, whose `FinishFrame()` is
invoked automatically inside `ISwapChain::Present()`. Called out twice (once on
`EngineCreateInfo::NumDeferredContexts`'s doc comment, once on `FinishFrame()` itself) — a
deliberate, load-bearing requirement. The same requirement extends to *additional immediate
contexts* beyond the first.

**Command-queue access is immediate-context-only**, confirmed by the same `LockCommandQueue()` doc
already quoted in Task 4: "Only immediate device contexts have associated command queues." — a
deferred context has no `VkQueue` of its own; it only ever reaches the GPU through whichever
immediate context's `ExecuteCommandLists` consumes its `ICommandList`.

**Not yet needed but confirmed for the future milestone:** `ICommandList` itself
(`Graphics\GraphicsEngine\interface\CommandList.h`) is a plain opaque `IObject`-derived handle
passed between `FinishCommandList` and `ExecuteCommandLists` — no Vulkan-specific `ICommandListVk`
exists (no such file in the `GraphicsEngineVulkan\interface\` listing), so there is no native
`VkCommandBuffer` extraction point for a not-yet-submitted deferred command list; the Tracy
command-buffer hookup from Task 4 only applies to the *currently recording* buffer of an
*immediate* context via `IDeviceContextVk::GetVkCommandBuffer()`.

---

## Summary of files touched (all absolute, under the confirmed source root)

- `DiligentCore\Graphics\GraphicsEngine\interface\ShaderResourceVariable.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\ShaderResourceBinding.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\PipelineState.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\BlendState.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\RasterizerState.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\DepthStencilState.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\InputLayout.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\Shader.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\RenderDevice.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\DeviceObject.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\DeviceContext.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\GraphicsTypes.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\SwapChain.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\CommandQueue.h`
- `DiligentCore\Graphics\GraphicsEngine\interface\Constants.h`
- `DiligentCore\Graphics\GraphicsTools\interface\RenderStateCache.h`
- `DiligentCore\Graphics\GraphicsTools\src\RenderStateCacheImpl.cpp`
- `DiligentCore\Graphics\GraphicsTools\include\RenderStateCacheImpl.hpp`
- `DiligentCore\Graphics\GraphicsEngineVulkan\interface\RenderDeviceVk.h`
- `DiligentCore\Graphics\GraphicsEngineVulkan\interface\DeviceContextVk.h`
- `DiligentCore\Graphics\GraphicsEngineVulkan\interface\CommandQueueVk.h`
- `DiligentCore\Graphics\GraphicsEngineVulkan\interface\EngineFactoryVk.h`
- `DiligentCore\Graphics\GraphicsEngineVulkan\src\DeviceContextVkImpl.cpp`
- `DiligentCore\Graphics\GraphicsEngineVulkan\src\BufferVkImpl.cpp`
- `DiligentCore\Graphics\GraphicsEngineVulkan\src\PipelineStateVkImpl.cpp`
- `DiligentCore\Graphics\GraphicsEngineVulkan\src\VulkanUtilities\Debug.cpp`
- `DiligentCore\Graphics\GraphicsEngineVulkan\src\VulkanUtilities\LogicalDevice.cpp`
- `DiligentCore\Primitives\interface\Object.h`
- `DiligentCore\Primitives\interface\CommonDefinitions.h`

### Explicitly flagged gaps (searched, not found — do not infer these exist)
- No `IShader::BindResources` method exists anywhere in `Shader.h`.
- No Tracy reference/example anywhere in this pinned source tree.
- No `ICommandListVk` / native handle for a not-yet-submitted deferred command list.
- No `IPhysicalDeviceVk`-style separate interface — physical-device handle lives directly on
  `IRenderDeviceVk`.
