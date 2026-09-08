// Sparse-brick-octree ray marcher (docs/goals.md Group X, research/micro-voxel-pivot-log.md):
// one fullscreen pass, one primary ray per pixel through the flat node/brick arrays that
// world/svo/brick_tree.hpp uploads verbatim. TraceRay() below is a statement-for-statement port of
// world/svo/src/ray_trace.cpp -- the CPU reference the brute-force oracle tests validate and
// tools/svo_render renders frames with. Keep the two in lockstep: a traversal change lands in
// ray_trace.cpp, passes its oracle, and is then mirrored here.
//
// Writes SV_Depth from the hit position so the existing post chain / overlay see a real depth
// buffer, and the hit distance into a second target for the temporal pass (svo_taa.psh.hlsl);
// misses shade the analytic sky at far depth. Shading (Group Z, research/lin-look-log.md): the
// terrain pass's model (sun + hemisphere ambient + albedo mottle + exp2 height fog + the fresnel
// water path) with the normal blended from the hit cube's face toward the tree's own averaged
// surface normal as cubes shrink toward a pixel (the anti-moire), per-cube brightness grain that
// fades the same way, a traced sun-shadow ray and a short hemisphere AO -- both judged for LOD by
// distance from THEIR OWN origin, never the eye's (goal 164: the shadow rings).

#include "sky_common.fxh"
#include "wind.fxh"

cbuffer MarchConstants
{
    column_major float4x4 g_InvViewProj;
    column_major float4x4 g_ViewProj;
    float4 g_CameraPosWorld;   // xyz camera position; w = elapsed seconds (water ripple phase)
    float4 g_TreeOrigin;       // xyz world-space min corner of the root; w = root edge (meters)
    float4 g_TreeParams;       // x = lod pixel angle (radians/pixel, quality-scaled), y = shadow lod
                               // multiplier, z = finest voxel edge (meters), w = AO radius (pixels)
    uint4  g_TreeInts;         // x = voxel bits V, y = max brick level, z = root node offset,
                               // w = flags: 1 shadows, 2 lod march, 4 AO, 8 tree present, 16 sky,
                               //     32 grain; bits 8..11 = debug view (kView*)
    float4 g_ShadeParams;      // x = smooth-normal span (pixels), y = grain amplitude,
                               // z = AO lod multiplier, w = raw pixel angle (radians, unscaled)
    float4 g_Jitter;           // xy = sub-pixel jitter (pixels), zw = 1 / viewport size
    // The ONE wind field's per-run tuning (world/wind). The wave SHAPE is compiled in as WIND_*
    // macros by detail/wind_macros.hpp, straight from world/wind's own header -- these are only the
    // numbers --wind-speed / --no-wind move without a recompile.
    float4 g_WindDirSpeed;     // xy = horizontal wind direction, z = base speed, w = gust amplitude
    float4 g_WindGustFlutter;  // x = gust frequency, y = gust scroll, z = flutter Hz, w = flutter freq
    // The Gerstner wave field (world/water), DERIVED on the CPU and only summed here: xy =
    // direction, z = amplitude, w = wavenumber k. WAVE_COUNT is a macro from world/water's own
    // kWaveCount, so the array cannot get out of step with the C++ that fills it.
    float4 g_Waves[WAVE_COUNT];
    float4 g_WaveParams;       // x = steepness Q (shared, so the field's total stays in budget),
                               // y = goal 266's beam tile size (0 = no seed)
    // Goal 256's cell grid. xyz = grid dimensions in cells, w = cell edge in metres; 0 dims means
    // the grid is off and the whole region is marched as one tree, which MakeWholeCell() expresses
    // as a grid of exactly one cell.
    float4 g_GridDims;
    float4 g_GridOrigin;       // xyz = world min corner of cell (0,0,0); w = unused
    // Goal 261: x = the frame index written into g_CellUsage, y != 0 enables the marking at all
    // (it is a knob so its cost can be measured against zero), zw spare.
    //
    // ORDER MATTERS AND THE static_assert DOES NOT CATCH IT. This field was first added HERE in the
    // C++ but BETWEEN g_GridDims and g_GridOrigin in the HLSL. The struct sizes still matched, so
    // the assert passed, and every field after the insertion point read the previous one's bytes --
    // the grid origin became garbage and the whole world rendered as an empty frame. A cbuffer
    // mirror is an ORDERED contract, not just a sized one.
    float4 g_MarkParams;
    // Goal 263: the proxy's geometry. xyz = world min corner, w = root edge in metres; a w of 0
    // means there is no proxy and an absent cell is passed through, which is the pre-263 behaviour
    // kept so the two can be compared.
    float4 g_ProxyOrigin;
    uint4 g_ProxyInts; // x = root node offset, y = voxel bits V, zw spare
    // Prompt 007 goal 327. x = extinction coefficient sigma at sea level (1/m), derived on the CPU
    // from an authored meteorological visibility; y = atmospheric scale height (m); zw spare.
    float4 g_FogParams;
    // One record per material (render/diligent/detail/material_macros.hpp's material_record):
    // rgb = linear albedo, w = shading model. MATERIAL_COUNT and MAT_SHADING_* are macros the C++
    // side passes at shader creation from the material registry -- no material literal lives here.
    float4 g_Materials[MATERIAL_COUNT];
};

// Goal 285: this material's stipple amplitude, 0..1, from the fractional part above.
float MaterialStipple(uint material)
{
    const float w = g_Materials[min(material, MATERIAL_COUNT - 1u)].w;
    return w - floor(w);
}

float3 MaterialAlbedo(uint material)
{
    return g_Materials[min(material, MATERIAL_COUNT - 1u)].rgb;
}

uint MaterialShading(uint material)
{
    // floor, NOT round: goal 285 packs the material's stipple amplitude into this float's
    // FRACTIONAL part (see MaterialStipple below), and `+ 0.5` would have rounded an amplitude of
    // 0.5 or more into the next shading model. Packed rather than given its own array because a new
    // cbuffer field is a new chance at the field-ORDER mismatch Prompt 004 lost hours to.
    return uint(floor(g_Materials[min(material, MATERIAL_COUNT - 1u)].w));
}

StructuredBuffer<uint> g_Nodes;
StructuredBuffer<uint> g_Bricks;
// Goal 266: one conservative start distance per screen tile, from svo_beam.psh.hlsl. Read
// with Load() at an explicitly computed tile index rather than sampled by UV -- a filtered or
// rounded fetch could land on a NEIGHBOURING tile, whose bound is not conservative for this
// pixel, and the failure would be a hole in the terrain rather than an obvious error.
Texture2D<float> g_BeamStart;

// Prompt 004 goal 256: the cell grid. One uint4 per cell -- node base, brick base, root offset,
// flags -- mirroring `world::svo::FlatCell` word for word. `g_Nodes` and `g_Bricks` hold every
// cell's words concatenated, and a cell's internal offsets stay exactly as the builder produced
// them, so `tree_layout.hpp` is untouched and the CPU oracle still guards this encoding.
StructuredBuffer<uint4> g_Cells;

// Prompt 004 goal 263: the always-resident COARSE PROXY -- one tree over the whole region at a
// coarse voxel size. A ray stepping into a cell that is not resident answers from this instead of
// passing through, which is GigaVoxels' "if LOD not available -> pick next higher available level".
// Its own buffers because it is built once and never churns, unlike the pools.
StructuredBuffer<uint> g_ProxyNodes;
StructuredBuffer<uint> g_ProxyBricks;

// Prompt 004 goal 261: what the marcher records about the cells it steps into. One word per cell --
// the frame index in the low 31 bits, bit 31 set when a ray wanted the cell and it was not resident.
//
// NO ATOMIC, and the reason is the encoding rather than a guarantee about the hardware. Every ray in
// this dispatch writes the SAME word for the same cell: the frame index is a per-frame constant, and
// whether a cell is resident is a property of the structure and not of the ray. So this is a set of
// concurrent stores of one identical value to one address, which cannot disagree whatever order they
// land in. An InterlockedOr, a counter, or "how many rays wanted it" would each be a
// read-modify-write and would put the atomic straight back -- which is why none of them is here.
// world/svo/cell_marks.hpp carries the argument in full and the CPU mirror it is checked against.
//
// COMPILED OUT WHEN THE GRID IS OFF, and that is not tidiness. A bound pixel-shader UAV costs this
// march ~30% on vk even when nothing writes to it -- measured, because the goal 273 regression gate
// caught it on the grid-OFF path where the marking never runs. The mechanism is the same family as
// the SV_Depth finding in research section 11: a UAV on a pixel shader disables ROP and early-Z
// optimisations whether or not it is used. Two PSOs, one macro, and the legacy path pays nothing.
#if SVO_MARK_USAGE
RWStructuredBuffer<uint> g_CellUsage;
#endif

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

struct PSOutput
{
    float4 Color : SV_TARGET0;
    float  Dist  : SV_TARGET1; // hit distance in meters (sky: kSkyDistance)
    // Goal 267: NO SV_Depth. Writing it cost 17% of the march on vk (4.03 -> 3.35 ms) and 10% on
    // d3d12 (4.39 -> 3.95), because a shader-written depth forces the ROP to export in submission
    // order -- the exact mechanism research section 9.1 identifies as the one real argument for
    // moving this pass to compute, and here it is measurable without moving anything.
    //
    // The old comment said the write was "for the overlay". It was not: every pass after the march
    // on this path (TAA resolve, composite, ImGui) has DepthEnable = False, so nothing ever read
    // it. Verified by capturing a frame WITH the overlay and crosshair on, both configurations --
    // identical but for the overlay's own changing digits.
};

static const uint kFlagShadows = 1u;
static const uint kFlagLodMarch = 2u;
static const uint kFlagAO = 4u;
static const uint kFlagTree = 8u;
static const uint kFlagSky = 16u;
static const uint kFlagGrain = 32u;
// Goal 278: band-limit the albedo toward the hit node's area-weighted average. A FLAG rather than a
// new cbuffer field on purpose -- Prompt 004 lost hours to a cbuffer field-ORDER mismatch that a
// size static_assert cannot catch, and a spare bit in a word that already exists cannot reorder.
static const uint kFlagFilterAlbedo = 64u;
// Goal 285: the deliberate directional stipple. Separate from kFlagGrain, which is the per-CUBE
// brightness hash from Group Z -- that one is keyed to the voxel lattice and measured at exactly
// zero contribution to the moire (log section 2), while this one is a chosen pattern at a chosen
// frequency and is the half of the owner's request that AL-A's filtering spent.
static const uint kFlagStipple = 128u;

// The hatch direction, in world space. MEASURED, not chosen: a radial FFT of the target capture's
// stone region puts 641x more energy at 30-45 degrees than at the quietest orientation, so the
// reference stipple is strongly directional rather than isotropic -- and a world-space direction is
// what gives a coherent hatch across a whole slope, the way a pencil hatch reads.
static const float3 kHatchDir = normalize(float3(0.80, 0.35, 0.49));

// One octave of the hatch: a 1D wave along kHatchDir, so its iso-lines are parallel planes and a
// surface cutting them sees parallel stripes.
float HatchOctave(float3 p, float scale)
{
    return sin(dot(p, kHatchDir) * scale * 6.2831853);
}

// BENARD, BOUSSEAU & THOLLOT (I3D 2009), the fractal-octave construction, and the reason it is not
// simpler than this. They prove the three properties a stipple must have are mutually
// contradictory: constant size and density IN THE IMAGE, following the motion of the 3D surface
// (or you get the shower-door effect), and temporal continuity. Their resolution is a weighted sum
// of n octaves whose scales are tied to a ZOOM CYCLE -- one cycle per doubling of apparent size --
// with weights that sum to 1 across the cycle so no octave ever pops in. Their finding, verbatim:
// "Empirically we observed that n = 4 octaves is enough to deceive human perception."
//
// s is the fractional part of log2(distance); the four weights are theirs exactly.
float Hatch(float3 p, float distance, float worldScale)
{
    const float lz = log2(max(distance, 1.0e-4));
    const float s = lz - floor(lz);
    const float base = worldScale / exp2(floor(lz));
    const float a1 = s / 2.0;
    const float a2 = 0.5 - s / 6.0;
    const float a3 = 1.0 / 3.0 - s / 6.0;
    const float a4 = 1.0 / 6.0 - s / 6.0;
    return a1 * HatchOctave(p, base * 8.0) + a2 * HatchOctave(p, base * 4.0) +
           a3 * HatchOctave(p, base * 2.0) + a4 * HatchOctave(p, base);
}
static const uint kViewShift = 8u;
static const uint kViewNone = 0u;
static const uint kViewLit = 1u;
static const uint kViewAO = 2u;
static const uint kViewNormal = 3u;
static const uint kViewFaceNormal = 4u;
static const uint kViewLevel = 5u;
static const uint kViewSteps = 6u;
static const uint kViewCoverage = 7u;
static const uint kViewCubePixels = 8u;
static const uint kViewSmoothNormal = 9u;
static const uint kViewLodCube = 10u;
static const uint kViewMaterial = 11u;
static const uint kViewDistance = 12u;
// Goal 258: the brick is palette-compressed -- 16 mask words, 52 index words holding ten 3-bit
// palette indices each, then a 2-word 8-entry palette. Mirrors world/svo/brick.hpp; the two change
// together and the 7,000-ray oracle is what proves they agree.
static const uint kBrickWords = 70u;
static const uint kBrickMaskWords = 16u;
static const uint kBrickIndexWord0 = 16u;
static const uint kBrickIndicesPerWord = 10u;
static const uint kBrickPaletteWord0 = 68u;
// A ceiling on the grid walk, the same kind of safety bound kMaxIterations is for the octree.
// 16x16x16 cells is 48 steps along an axis and at most ~90 on a diagonal; 256 is generous.
static const uint kMaxGridSteps = 256u;
static const uint kMaxIterations = 2048u;
static const uint kMaxLevels = 22u;
static const float kSkyDistance = 1.0e6;
// Secondary rays descend LOD nodes under this coverage instead of hitting them (goal 171).
static const float kSecondaryCoverage = 0.35;

// Node layout v2 (world/svo/tree_layout.hpp): internal = [header][attributes][children...],
// brick = [header][brick index][attributes], solid = [header].
static const uint kAttrSlotInternal = 1u;
static const uint kBrickIndexSlot = 1u;
static const uint kAttrSlotBrick = 2u;
static const uint kFirstChildSlot = 2u;

struct Hit
{
    bool   hit;
    float  t;
    uint   material;
    int3   normal;
    int    level;
    bool   lodCube;
    bool   solidLeaf;    // the hit is a solid leaf's own face (no attributes of its own)
    uint   steps;
    float  cubeEdge;     // world meters: the voxel / LOD cube / solid cube that was hit
    float3 smoothNormal; // averaged normal of the node the smoothing rule picked (0 = none)
    float  coverage;
    int    smoothLevel;
    // Goal 278: the hit node's own AREA-WEIGHTED AVERAGE albedo, unpacked from the header's free
    // bits (world/svo/tree_layout.hpp packs it R4 G6 B4). Filtered at the HIT NODE, not at the
    // normal's smoothing ancestor -- two quantities, two correct scales. Reading it from the 6 px
    // normal ancestor blurred the terrain flat (local contrast 5.7% against a 6% floor).
    float3 nodeAlbedo;
    bool   hasNodeAlbedo;
};

// Mirrors node_albedo() / node_has_albedo() in tree_layout.hpp -- change both together.
float3 NodeAlbedo(uint header)
{
    return float3(float((header >> 24) & 0xFu) / 15.0,
                  float((header >> 10) & 0x3Fu) / 63.0,
                  float((header >> 28) & 0xFu) / 15.0);
}
bool NodeHasAlbedo(uint header)
{
    return (header & ((0xFu << 24) | (0x3Fu << 10) | (0xFu << 28))) != 0u;
}

int ArgMin3(float3 v)
{
    if (v.x <= v.y && v.x <= v.z) return 0;
    return v.y <= v.z ? 1 : 2;
}

// Runtime-indexed component access. FXC (the D3D12 path) rejects a dynamically indexed vector
// component as an l-value (X3500), so every "v[axis] = ..." in the CPU reference becomes a masked
// vector write here, and reads go through these selects for symmetry.
float Comp(float3 v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
int   CompI(int3 v, int i)  { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
int3  AxisMask(int i)       { return int3(i == 0 ? 1 : 0, i == 1 ? 1 : 0, i == 2 ? 1 : 0); }

int3 NormalFrom(int axis, int3 step, float3 d)
{
    if (axis < 0)
    {
        // Ray started inside the hit cell: face against the dominant direction component.
        const float3 a = abs(d);
        const int dominant = (a.x >= a.y && a.x >= a.z) ? 0 : (a.y >= a.z ? 1 : 2);
        return AxisMask(dominant) * (Comp(d, dominant) > 0.0 ? -1 : 1);
    }
    return AxisMask(axis) * (-CompI(step, axis));
}

Hit MakeMiss()
{
    Hit m;
    m.hit = false;
    m.t = 0.0;
    m.material = 0u;
    m.normal = int3(0, 0, 0);
    m.level = -1;
    m.lodCube = false;
    m.solidLeaf = false;
    m.steps = 0u;
    m.cubeEdge = 0.0;
    m.smoothNormal = float3(0.0, 0.0, 0.0);
    m.coverage = 0.0;
    m.smoothLevel = -1;
    m.nodeAlbedo = float3(0.0, 0.0, 0.0);
    m.hasNodeAlbedo = false;
    return m;
}

// Prompt 004 goal 256: WHICH TREE a traversal is walking.
//
// The marcher used to read the one tree straight out of the constant buffer. With the cell grid
// (world/svo/cell_grid.hpp) there are many trees in ONE pair of arrays, each with a base offset, so
// every traversal has to be told which. This struct is the mirror of `world::svo::TreeView`, and
// `MakeWholeCell()` below turns the old single-tree globals into a grid of exactly one cell -- which
// is why the pre-grid path is not a separate code path at all, and cannot drift.
struct Cell
{
    uint  nodeBase;  // word offset of this cell's first node inside g_Nodes
    uint  brickBase; // BRICK index (not word offset) of its first brick inside g_Bricks
    uint  root;      // root header offset, RELATIVE to nodeBase
    float3 origin;   // world-space min corner
    float edge;      // cell edge, metres
    int   V;         // voxel bits: log2 of voxels along one cell edge
    bool  present;
    bool  isProxy;   // goal 263: read g_ProxyNodes/g_ProxyBricks instead of the pools
};

Cell MakeAbsentCell()
{
    Cell c;
    c.nodeBase = 0u;
    c.brickBase = 0u;
    c.root = 0u;
    c.origin = float3(0.0, 0.0, 0.0);
    c.edge = 0.0;
    c.V = 0;
    c.present = false;
    c.isProxy = false;
    return c;
}

// Goal 263: the coarse proxy as a Cell covering the whole region.
Cell MakeProxyCell()
{
    Cell c = MakeAbsentCell();
    c.nodeBase = 0u;
    c.brickBase = 0u;
    c.root = g_ProxyInts.x;
    c.origin = g_ProxyOrigin.xyz;
    c.edge = g_ProxyOrigin.w;
    c.V = int(g_ProxyInts.y);
    c.present = g_ProxyOrigin.w > 0.0;
    c.isProxy = true;
    return c;
}

// The whole region as one cell -- the shipping, pre-grid arrangement.
Cell MakeWholeCell()
{
    Cell c = MakeAbsentCell();
    c.root = g_TreeInts.z;
    c.origin = g_TreeOrigin.xyz;
    c.edge = g_TreeOrigin.w;
    c.V = int(g_TreeInts.x);
    c.present = (g_TreeInts.w & kFlagTree) != 0u;
    return c;
}

// One node word, from whichever array this cell lives in. The branch is uniform across a warp for
// every ray in a cell, so it costs a predicted branch and not divergence.
uint CellNode(Cell cell, uint offset)
{
    return g_Nodes[cell.nodeBase + offset];
}
uint CellBrick(Cell cell, uint offset)
{
    return g_Bricks[offset];
}

// Decodes the attribute word (three int8 snorm normal components + a uint8 coverage).
float3 AttrNormal(uint attr)
{
    return float3(float(int(attr << 24) >> 24), float(int(attr << 16) >> 24), float(int(attr << 8) >> 24)) / 127.0;
}
float AttrCoverage(uint attr) { return float(attr >> 24) / 255.0; }

// Mirror of ray_trace.cpp's make_hit attribute rule: start at `attrLevel` (the deepest stack
// entry carrying attributes for this hit) and walk up while that ancestor spans less than
// t * smoothPixelAngle.
void ReadAttributes(inout Hit h, uint stack[kMaxLevels], int attrLevel, float smoothPixelAngle, Cell cell)
{
    // Goal 278: the albedo comes from attrLevel -- the node actually hit -- before any smoothing
    // walk. See the comment on Hit::nodeAlbedo for why this scale and not the normal's.
    if (attrLevel >= 0)
    {
        const uint hitHeader = CellNode(cell, stack[attrLevel]);
        h.nodeAlbedo = NodeAlbedo(hitHeader);
        h.hasNodeAlbedo = NodeHasAlbedo(hitHeader);
    }
    int L = attrLevel;
    if (smoothPixelAngle > 0.0)
    {
        const float wanted = h.t * smoothPixelAngle;
        [loop]
        while (L > 0 && cell.edge * exp2(-float(L)) < wanted)
            --L;
    }
    if (L >= 0)
    {
        const uint node = stack[L];
        const uint kind = (CellNode(cell, node) >> 8) & 3u;
        if (kind != 2u)
        {
            const uint attr = CellNode(cell, node + (kind == 0u ? kAttrSlotInternal : kAttrSlotBrick));
            h.smoothNormal = AttrNormal(attr);
            h.coverage = AttrCoverage(attr);
            h.smoothLevel = L;
        }
    }
}

// Defined below: HLSL needs the declaration before TraceGrid can call it, and TraceCell is the
// larger of the two so it reads better after the walk that drives it.
Hit TraceCell(Cell cell, float3 rayOrigin, float3 rayDir, float lodPixelAngle, float tOffset, float maxT,
              float smoothPixelAngle, float coverageThreshold, float tStart);

// Prompt 004 goal 256: the grid DDA, mirroring `trace_grid` in world/svo/src/cell_grid.cpp.
//
// Cells are visited in ray order by an Amanatides-Woo walk over a FLAT ARRAY -- no pointer chasing
// between cells at all -- and each present one is traced with the ordinary octree descent. Because
// `TraceCell` returns the nearest hit WITHIN a cell and cells are visited near to far, the first hit
// found is the globally nearest. There is nothing else to prove.
//
// Measured on the CPU reference over 20,000 real-terrain rays: 21.7 octree steps per ray as one
// 512 m tree becomes 10.5 steps plus 2.6 of these flat DDA steps -- a 52% cut in the pointer-chasing
// half of the work.
// The raw flag word, so the walk can tell NOT RESIDENT from RESIDENT-AND-EMPTY.
uint FetchCellFlags(int3 coord)
{
    const int3 dims = int3(g_GridDims.xyz);
    if (any(coord < int3(0, 0, 0)) || any(coord >= dims))
        return 0u;
    return g_Cells[uint(coord.x + dims.x * (coord.y + dims.y * coord.z))].w;
}

Cell FetchCell(int3 coord)
{
    const int3 dims = int3(g_GridDims.xyz);
    if (any(coord < int3(0, 0, 0)) || any(coord >= dims))
        return MakeAbsentCell();
    const uint index = uint(coord.x + dims.x * (coord.y + dims.y * coord.z));
    const uint4 record = g_Cells[index];

    Cell c;
    c.nodeBase = record.x;
    c.brickBase = record.y;
    c.root = record.z;
    c.origin = g_GridOrigin.xyz + float3(coord) * g_GridDims.w;
    c.edge = g_GridDims.w;
    c.V = int(g_TreeInts.x);
    // Bit 0 present, bit 1 resident-and-empty: an empty cell has nothing to trace but is NOT a
    // candidate for the proxy fallback (world/svo/cell_grid.hpp's kFlatCellEmpty).
    c.present = (record.w & 1u) != 0u && (record.w & 2u) == 0u;
    c.isProxy = false;
    return c;
}

Hit TraceGrid(float3 rayOrigin, float3 rayDir, float lodPixelAngle, float tOffset, float maxT,
              float smoothPixelAngle, float coverageThreshold, float tStart)
{
    // Grid off: one cell, the whole region, exactly the pre-grid behaviour.
    if (g_GridDims.x <= 0.0)
        return TraceCell(MakeWholeCell(), rayOrigin, rayDir, lodPixelAngle, tOffset, maxT, smoothPixelAngle,
                         coverageThreshold, tStart);

    Hit miss = MakeMiss();
    const int3 dims = int3(g_GridDims.xyz);
    const float edge = g_GridDims.w;
    const float3 o = (rayOrigin - g_GridOrigin.xyz) / edge;
    const float3 d = rayDir / edge;

    float3 invd;
    [unroll]
    for (int a = 0; a < 3; ++a)
        invd[a] = abs(d[a]) > 1.0e-20 ? 1.0 / d[a] : (d[a] >= 0.0 ? 1.0e30 : -1.0e30);

    // Slab test against the whole grid, in cell units, so a ray that meets nothing costs one test.
    //
    // NO `continue` AND NO `return` INSIDE THE UNROLLED LOOPS HERE, and that is not style. FXC
    // refuses to unroll a loop containing them ("X3511: forced to unroll loop, but unrolling
    // failed"), and once the loop is not unrolled the vector component writes below become
    // RUNTIME-INDEXED, which FXC also rejects ("X3500: array reference cannot be used as an
    // l-value"). Vulkan's compiler accepts both, so this shader compiled and ran correctly on vk
    // and failed outright on d3d12 -- exactly the trap CLAUDE.md documents. Both branches are
    // therefore expressed as values, and the early-outs happen after the loop.
    float tEnter = max(tStart, 0.0);
    float tExit = maxT;
    bool outsideOnParallelAxis = false;
    [unroll]
    for (int b = 0; b < 3; ++b)
    {
        const bool degenerate = abs(d[b]) < 1.0e-20;
        // Parallel to this axis: constrained only by already being inside the slab.
        outsideOnParallelAxis =
            outsideOnParallelAxis || (degenerate && (o[b] < 0.0 || o[b] >= float(dims[b])));
        const float t0 = (0.0 - o[b]) * invd[b];
        const float t1 = (float(dims[b]) - o[b]) * invd[b];
        tEnter = degenerate ? tEnter : max(tEnter, min(t0, t1));
        tExit = degenerate ? tExit : min(tExit, max(t0, t1));
    }
    if (outsideOnParallelAxis || tExit < tEnter)
        return miss;

    int3 cellCoord;
    int3 stepDir;
    float3 tMax;
    float3 tDelta;
    const float3 entry = o + tEnter * d;
    [unroll]
    for (int c = 0; c < 3; ++c)
    {
        // Clamped: a ray entering exactly on a face can land one cell out through float rounding,
        // and clamping is cheaper and more robust than making the arithmetic exact.
        cellCoord[c] = clamp(int(floor(entry[c])), 0, dims[c] - 1);
        // Branch-free for the FXC reason above: an axis the ray does not travel along gets a step
        // of 0 and a boundary at infinity, which the walk below never selects.
        const bool degenerate = abs(d[c]) < 1.0e-20;
        stepDir[c] = degenerate ? 0 : (d[c] > 0.0 ? 1 : -1);
        const float boundary = float(cellCoord[c] + (stepDir[c] > 0 ? 1 : 0));
        tMax[c] = degenerate ? 1.0e30 : (boundary - o[c]) * invd[c];
        tDelta[c] = degenerate ? 1.0e30 : abs(invd[c]);
    }

    float cellEnter = tEnter;
    [loop]
    for (uint walked = 0u; walked < kMaxGridSteps; ++walked)
    {
        const Cell cell = FetchCell(cellCoord);
        const uint cellFlags = FetchCellFlags(cellCoord);
#if SVO_MARK_USAGE
        if (g_MarkParams.y != 0.0)
        {
            // A plain store, once per cell entered. See the note on g_CellUsage above for why this
            // needs no atomic and why it deliberately accumulates nothing.
            const int3 dims = int3(g_GridDims.xyz);
            const uint index = uint(cellCoord.x + dims.x * (cellCoord.y + dims.y * cellCoord.z));
            g_CellUsage[index] = (uint(g_MarkParams.x) & 0x7FFFFFFFu) | (cell.present ? 0u : 0x80000000u);
        }
#endif
        if (cell.present)
        {
            const Hit hit = TraceCell(cell, rayOrigin, rayDir, lodPixelAngle, tOffset, maxT,
                                      smoothPixelAngle, coverageThreshold, tStart);
            if (hit.hit)
                return hit;
        }
        // Goal 263: NOT RESIDENT (flag bit 0 clear) -- answer from the coarse proxy over this
        // cell's own span. A cell that is resident and EMPTY (bit 1) is skipped instead: the fine
        // build looked there and found nothing, and the proxy's coarser voxels must not overrule
        // that. The two look identical to the marcher and are opposites here.
        else if ((cellFlags & 1u) == 0u && g_ProxyOrigin.w > 0.0)
        {
            const int axisNow = tMax.x < tMax.y ? (tMax.x < tMax.z ? 0 : 2) : (tMax.y < tMax.z ? 1 : 2);
            const float cellExit = min(tMax[axisNow], tExit);
            const Hit coarse = TraceCell(MakeProxyCell(), rayOrigin, rayDir, lodPixelAngle, tOffset,
                                         min(maxT, cellExit), smoothPixelAngle, coverageThreshold,
                                         max(tStart, cellEnter));
            if (coarse.hit)
                return coarse;
        }

        const int axis = tMax.x < tMax.y ? (tMax.x < tMax.z ? 0 : 2) : (tMax.y < tMax.z ? 1 : 2);
        if (tMax[axis] > tExit)
            return miss;
        // X3500: a runtime-indexed vector component cannot be WRITTEN under FXC, so both of these
        // go through masked whole-vector writes (see AxisMask above, and the note at the top).
        const int3 m = AxisMask(axis);
        cellCoord += m * stepDir;
        if (any(cellCoord < int3(0, 0, 0)) || any(cellCoord >= dims))
            return miss;
        cellEnter = tMax[axis];
        tMax += float3(m) * tDelta;
    }
    return miss;
}

// Mirror of world::svo::trace_ray (ray_trace.cpp). t is in units of |rayDir| (meters for a unit
// direction). lodPixelAngle == 0 disables the LOD early-out; tOffset is added to the distance
// before the LOD test (0 for every ray this shader casts -- see TraceParams::t_offset);
// coverageThreshold: an early-out node under this volume coverage is descended instead of hit
// (TraceParams::lod_coverage_threshold).
Hit TraceCell(Cell cell, float3 rayOrigin, float3 rayDir, float lodPixelAngle, float tOffset, float maxT,
              float smoothPixelAngle, float coverageThreshold, float tStart)
{
    Hit miss = MakeMiss();
    if (!cell.present)
        return miss;

    const int   V = cell.V;
    const float cells = exp2(float(V));
    const float rootEdge = cell.edge;
    const float3 o = (rayOrigin - cell.origin) / rootEdge;
    const float3 d = rayDir / rootEdge;
    float3 invd;
    int3 step;
    [unroll]
    for (int a = 0; a < 3; ++a)
    {
        step[a] = d[a] >= 0.0 ? 1 : -1;
        invd[a] = abs(d[a]) > 1.0e-20 ? 1.0 / d[a] : (d[a] >= 0.0 ? 1.0e30 : -1.0e30);
    }

    // Slab test against the root [0,1]^3.
    float tEnter = 0.0;
    float tExit = maxT;
    int enterAxis = -1;
    [unroll]
    for (int b = 0; b < 3; ++b)
    {
        const float t0 = (0.0 - o[b]) * invd[b];
        const float t1 = (1.0 - o[b]) * invd[b];
        const float tNear = min(t0, t1);
        const float tFar = max(t0, t1);
        if (tNear > tEnter)
        {
            tEnter = tNear;
            enterAxis = b;
        }
        tExit = min(tExit, tFar);
    }
    if (tExit < tEnter)
        return miss;

    const bool inside = all(o >= 0.0) && all(o < 1.0);
    float t = inside ? 0.0 : tEnter;
    int lastAxis = inside ? -1 : enterAxis;
    // Goal 266: a beam seed jumps the ray past the root entry. Once it has, the ray no longer
    // starts ON a root face, so the entry-axis snap below would put the cell in the wrong place --
    // the coordinate has to come from the seeded position alone. lastAxis goes to -1 for the same
    // reason, which is exactly how a ray starting inside the root is already handled. Mirrors
    // world/svo/src/ray_trace.cpp's TraceParams::t_start.
    const bool seeded = tStart > t;
    if (seeded)
    {
        t = tStart;
        if (t > tExit)
            return miss;
    }
    lastAxis = seeded ? -1 : lastAxis;
    const int n = int(cells);
    int3 c;
    {
        const float3 p = o + t * d;
        c = clamp(int3(floor(p * cells)), int3(0, 0, 0), int3(n - 1, n - 1, n - 1));
        if (!inside && !seeded)
        {
            const int3 m = AxisMask(enterAxis);
            c = c * (int3(1, 1, 1) - m) + m * (CompI(step, enterAxis) > 0 ? 0 : n - 1);
        }
    }

    uint stack[kMaxLevels];
    stack[0] = cell.root;
    int level = 0;

    [loop]
    for (uint iteration = 1u; iteration <= kMaxIterations; ++iteration)
    {
        if (t > maxT)
        {
            miss.steps = iteration;
            return miss;
        }
        int3 cellMin = int3(0, 0, 0);
        int3 cellMax = int3(0, 0, 0);
        int exitAxis = 0;
        float tOut = 0.0;

        // Descend from the current level to the deepest node containing cell c.
        [loop]
        for (;;)
        {
            const uint node = stack[level];
            const uint header = CellNode(cell, node);
            const uint kind = (header >> 8) & 3u;
            if (kind == 2u)
            {
                Hit h = MakeMiss();
                h.hit = true;
                h.t = t;
                h.material = (header >> 16) & 0xFFu;
                h.normal = NormalFrom(lastAxis, step, d);
                h.level = level;
                h.lodCube = false;
                h.solidLeaf = true;
                h.steps = iteration;
                h.cubeEdge = rootEdge * exp2(-float(level));
                // A solid leaf has only a header, and that header carries its albedo -- which is
                // what lets goal 278 reach the 804,157 solid leaves that have no attribute word.
                h.nodeAlbedo = NodeAlbedo(header);
                h.hasNodeAlbedo = NodeHasAlbedo(header);
                ReadAttributes(h, stack, level - 1, smoothPixelAngle, cell);
                return h;
            }
            if (kind == 1u)
            {
                const int shift = V - level - 3;
                const int cellShift = V - level;
                const uint brickBase = (cell.brickBase + CellNode(cell, node + kBrickIndexSlot)) * kBrickWords;
                int3 v = (c >> shift) & 7;
                const int3 brickCell = (c >> cellShift) << cellShift;
                const float voxelEdge = exp2(-float(level + 3));
                const float3 brickOrigin = float3(brickCell) / cells;
                float3 tMax;
                float3 tDelta;
                [unroll]
                for (int e = 0; e < 3; ++e)
                {
                    const float boundary = brickOrigin[e] + float(v[e] + (step[e] > 0 ? 1 : 0)) * voxelEdge;
                    tMax[e] = (boundary - o[e]) * invd[e];
                    tDelta[e] = voxelEdge * abs(invd[e]);
                }
                [loop]
                for (;;)
                {
                    const uint index = uint(v.x + 8 * v.y + 64 * v.z);
                    if (((CellBrick(cell, brickBase + (index >> 5)) >> (index & 31u)) & 1u) != 0u)
                    {
                        Hit h = MakeMiss();
                        h.hit = true;
                        h.t = t;
                        // Two dependent loads instead of one: the voxel's 3-bit palette index,
                        // then the palette entry. Only ever executed on a HIT, which is why the
                        // measured march cost of this change is far below the 2.06x it saves in
                        // brick bandwidth -- see the log's section on goal 258.
                        const uint iw = index / kBrickIndicesPerWord;
                        const uint slot = (CellBrick(cell, brickBase + kBrickIndexWord0 + iw) >>
                                           ((index - iw * kBrickIndicesPerWord) * 3u)) & 7u;
                        h.material = (CellBrick(cell, brickBase + kBrickPaletteWord0 + (slot >> 2)) >>
                                      ((slot & 3u) * 8u)) & 0xFFu;
                        h.normal = NormalFrom(lastAxis, step, d);
                        h.level = level;
                        h.lodCube = false;
                        h.steps = iteration;
                        h.cubeEdge = rootEdge * voxelEdge;
                        ReadAttributes(h, stack, level, smoothPixelAngle, cell);
                        return h;
                    }
                    const int axis = ArgMin3(tMax);
                    t = Comp(tMax, axis);
                    const int3 m = AxisMask(axis);
                    tMax += tDelta * float3(m);
                    v += step * m;
                    lastAxis = axis;
                    const int va = CompI(v, axis);
                    if (va < 0 || va > 7)
                        break;
                }
                cellMin = brickCell;
                cellMax = brickCell + ((1 << cellShift) - 1);
                exitAxis = lastAxis;
                tOut = t;
                break;
            }

            // Internal node.
            const int childLevel = level + 1;
            if (lodPixelAngle > 0.0)
            {
                const float childEdgeWorld = rootEdge * exp2(-float(childLevel));
                if (childEdgeWorld < (t + tOffset) * lodPixelAngle &&
                    (coverageThreshold <= 0.0 || AttrCoverage(CellNode(cell, node + kAttrSlotInternal)) >= coverageThreshold))
                {
                    Hit h = MakeMiss();
                    h.hit = true;
                    h.t = t;
                    h.material = (header >> 16) & 0xFFu;
                    h.normal = NormalFrom(lastAxis, step, d);
                    h.level = level;
                    h.lodCube = true;
                    h.steps = iteration;
                    h.cubeEdge = rootEdge * exp2(-float(level));
                    ReadAttributes(h, stack, level, smoothPixelAngle, cell);
                    return h;
                }
            }
            const int sh = V - childLevel;
            const int octant = ((c.x >> sh) & 1) | (((c.y >> sh) & 1) << 1) | (((c.z >> sh) & 1) << 2);
            const uint mask = header & 0xFFu;
            if ((mask & (1u << uint(octant))) != 0u)
            {
                const uint below = mask & ((1u << uint(octant)) - 1u);
                stack[childLevel] = CellNode(cell, node + kFirstChildSlot + countbits(below));
                level = childLevel;
                continue;
            }
            // Empty child cell: leave it.
            cellMin = (c >> sh) << sh;
            cellMax = cellMin + ((1 << sh) - 1);
            float3 tE;
            [unroll]
            for (int f = 0; f < 3; ++f)
            {
                const float boundary = float(step[f] > 0 ? cellMax[f] + 1 : cellMin[f]) / cells;
                tE[f] = (boundary - o[f]) * invd[f];
            }
            exitAxis = ArgMin3(tE);
            tOut = Comp(tE, exitAxis);
            lastAxis = exitAxis;
            break;
        }

        // Step into the neighboring cell and pop to the deepest common ancestor.
        const int3 cOld = c;
        t = tOut;
        const float3 p = o + tOut * d;
        {
            const int3 exitMask = AxisMask(exitAxis);
            const int3 stepped = step * 0 + (step > 0 ? cellMax + 1 : cellMin - 1);
            const int3 clamped = clamp(int3(floor(p * cells)), cellMin, cellMax);
            c = exitMask * stepped + (int3(1, 1, 1) - exitMask) * clamped;
        }
        if (any(c < 0) || any(c >= n))
        {
            miss.steps = iteration;
            return miss;
        }
        const uint diff = uint((cOld.x ^ c.x) | (cOld.y ^ c.y) | (cOld.z ^ c.z));
        const int common = V - (diff == 0u ? 0 : (int(firstbithigh(diff)) + 1));
        level = min(level, common);
    }
    miss.steps = kMaxIterations;
    return miss;
}

// ---- shading (terrain.psh.hlsl's model + Group Z) ----------------------------------------------

float Hash2(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float Hash3(float3 p)
{
    return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453);
}

float ValueNoise(float2 p)
{
    const float2 cell = floor(p);
    const float2 f    = frac(p);
    const float2 u    = f * f * (3.0 - 2.0 * f);
    const float a = Hash2(cell);
    const float b = Hash2(cell + float2(1.0, 0.0));
    const float c = Hash2(cell + float2(0.0, 1.0));
    const float d = Hash2(cell + float2(1.0, 1.0));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// Two directional wave trains plus value-noise wobble at two scales, a sky-gradient reflection
// and a soft sun highlight. Goal 169 (the "checkerboard on the water" in the user's screenshot):
// bisected by swapping this function's return line and sampling one pixel row, the white cells
// were the SUN GLINT alone -- with the sun high and the camera looking down, the half-vector is
// nearly vertical, so a sharp pow(., 256) highlight fired wherever the 3-7 m ripple lattice
// tilted the normal through the peak, one bright cell per lattice cell. A broad, dim highlight
// plus a half-meter noise component turns that into the smooth glitter band real water has.
// The Gerstner surface normal at a column (Prompt 001 E1). MIRROR of world/water's wave_surface(),
// under the usual rule: the CPU version is the reference, this follows it, and the field it sums
// was derived there -- the shader never decides what a wind speed means.
//
// The normal is the analytic derivative of the same sum, not a finite difference of the height:
// Gerstner displaces points horizontally as well as vertically, so a height difference disagrees
// with the crests at exactly the steepnesses that make crests worth having.
float3 GerstnerNormal(float2 p, float timeSeconds, float fade)
{
    float nx = 0.0;
    float nz = 0.0;
    float ny = 1.0;
    const float q = g_WaveParams.x;
    [unroll]
    for (int i = 0; i < WAVE_COUNT; ++i)
    {
        const float2 dir = g_Waves[i].xy;
        const float amplitude = g_Waves[i].z * fade;
        const float k = g_Waves[i].w;
        if (amplitude <= 0.0)
        {
            continue;
        }
        // Deep-water dispersion omega = sqrt(g*k): longer waves travel faster, so a swell outruns
        // the chop instead of the whole field sliding as one sheet.
        const float omega = sqrt(WAVE_GRAVITY * k);
        const float phase = k * dot(dir, p) - omega * timeSeconds;
        const float s = sin(phase);
        const float c = cos(phase);
        const float wa = k * amplitude;
        nx -= dir.x * wa * c;
        nz -= dir.y * wa * c;
        ny -= q * wa * s;
    }
    return normalize(float3(nx, max(ny, 0.05), nz));
}

float3 ShadeWater(float3 worldPos, float3 viewDir, float timeSeconds, float fade)
{
    const float2 p = worldPos.xz;
    // Gerstner crests carry the surface; the fine noise stays because a real sea has capillary
    // detail far below the shortest gravity wave, and because it is what breaks the sun glint into
    // a glitter band instead of one mirror cell per lattice cell (the water-checkerboard lesson in
    // research/lin-look-log.md -- do not remove it without re-reading that).
    float3 n = GerstnerNormal(p, timeSeconds, fade);
    const float f1 = ValueNoise(p * 2.1 + float2(timeSeconds * 0.9, -timeSeconds * 0.6));
    const float f2 = ValueNoise(p * 2.1 + float2(57.3 + timeSeconds * 0.5, timeSeconds * 0.8));
    n.xz += fade * 0.05 * float2(f1 - 0.5, f2 - 0.5);
    const float3 rippleN = normalize(n);
    const float3 body = float3(0.06, 0.22, 0.36);
    const float cosTheta = saturate(dot(viewDir, rippleN));
    const float fresnel = 0.02 + 0.98 * pow(1.0 - cosTheta, 5.0);
    const float3 reflection = SkyGradient(reflect(-viewDir, rippleN));
    const float3 halfVec = normalize(viewDir - kSunDirection);
    const float3 glint = kSunColor * (0.18 * pow(saturate(dot(rippleN, halfVec)), 24.0));
    return lerp(body, reflection, fresnel) + glint;
}

// Short-ray hemisphere AO: four fixed directions around the normal, rotated per pixel by a hash so
// the pattern dithers instead of banding. The ray length is a screen-space radius (g_TreeParams.w
// pixels at the hit's distance, never under 0.15 m) so the darkening reads the same at every
// distance, and LOD is judged from the ray's own origin (goal 164).
float AmbientOcclusion(float3 p, float3 n, float2 pixel, float hitDistance)
{
    // Prompt 004 goal 268. Two changes here, both measured, and together they took the marcher
    // from 4.89 to 4.02 ms on vk and 5.10 to 4.42 on d3d12 -- an 18% / 13% cut of the WHOLE march
    // for a mean image difference of 0.097/255 over 0.11% of pixels (viewed:
    // research/captures/ak_ao_cheaper.png, and that is with --no-taa, the harsher case).
    //
    // (1) The ray length is CLAMPED. It used to grow without bound with hit distance, so distant
    // pixels paid the most for the AO that mattered least -- a contact-shadow term whose feature
    // size is sub-pixel out there. 2 m is a couple of the terrain cubes this look is built from.
    const float rayLength = clamp(g_TreeParams.w * hitDistance * g_ShadeParams.w, 0.15, 2.0);
    const float lod = g_TreeParams.x * g_ShadeParams.z;
    // Tangent frame around n.
    const float3 helper = abs(n.y) < 0.9 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    const float3 tangent = normalize(cross(helper, n));
    const float3 bitangent = cross(n, tangent);
    const float rot = Hash2(pixel) * 6.2831853;
    float occluded = 0.0;
    [unroll]
    // (2) TWO rays, not four. Measured separately: 4.89 -> 4.31 ms on its own, which is 13% of the
    // march, for 0.084/255 mean difference. Goal 246 priced all four AO rays at 1.81 ms (37% of
    // the marcher) and this recovers half of that. The hemisphere is sampled at two opposed
    // azimuths instead of four quadrants, with the per-pixel `rot` jitter and TAA carrying the
    // rest -- which is what the four were really buying.
    for (int i = 0; i < 2; ++i)
    {
        const float phi = rot + float(i) * 3.1415927;
        // ~45 degrees off the normal: cheap, and where occlusion actually lives for cube worlds.
        const float3 dir = normalize(n * 0.75 + (tangent * cos(phi) + bitangent * sin(phi)) * 0.66);
        const Hit h = TraceGrid(p, dir, lod, 0.0, rayLength, 0.0, kSecondaryCoverage, 0.0);
        if (h.hit)
            occluded += 1.0 - saturate(h.t / rayLength);
    }
    return 1.0 - 0.6 * (occluded * 0.5);
}

float3 LevelColor(int level)
{
    // A repeating 6-hue ramp so adjacent levels contrast.
    const float h = frac(float(level) / 6.0) * 6.0;
    const float3 c = saturate(float3(abs(h - 3.0) - 1.0, 2.0 - abs(h - 2.0), 2.0 - abs(h - 4.0)));
    return c * (0.55 + 0.45 * frac(float(level) / 2.0) * 2.0);
}

void main(in PSInput PSIn, out PSOutput PSOut)
{
    // Sub-pixel jitter (TAA) shifts every primary ray by the same fraction of a pixel; the
    // temporal pass knows the offset and re-centers.
    const float2 uv = PSIn.UV + g_Jitter.xy * g_Jitter.zw;
    const float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    const float4 farWorld = mul(g_InvViewProj, float4(ndc, 1.0, 1.0));
    const float3 camera = g_CameraPosWorld.xyz;
    const float3 dir = normalize(farWorld.xyz / farWorld.w - camera);
    const uint flags = g_TreeInts.w;
    const uint view = (flags >> kViewShift) & 0xFu;

    const float lodAngle = (flags & kFlagLodMarch) != 0u ? g_TreeParams.x : 0.0;
    const float smoothAngle = g_ShadeParams.x * g_ShadeParams.w;
    // Goal 266: the tile's conservative start distance. Load() at an explicitly computed tile
    // index, never a filtered or UV-rounded fetch -- a neighbouring tile's bound is NOT conservative
    // for this pixel, and getting it wrong shows up as a hole in the terrain, far from the cause.
    // g_WaveParams.y is the tile size, 0 when the pre-pass is off (its target is cleared to zero
    // then, so this reads 0 either way and the march needs no branch).
    //
    // PSIn.Pos is the UNJITTERED pixel centre, which is the right tile to index. The jittered RAY
    // can lean up to half a pixel out of that tile, and the cone is widened by a whole pixel
    // (world::svo::tile_tan_half_angle's margin) precisely so it still contains it.
    const int beamTile = int(g_WaveParams.y);
    const float tSeed =
        beamTile > 0 ? g_BeamStart.Load(int3(int2(PSIn.Pos.xy) / beamTile, 0)) : 0.0;
    const Hit hit = TraceGrid(camera, dir, lodAngle, 0.0, 1.0e30, smoothAngle, 0.0, tSeed);
    if (!hit.hit)
    {
        PSOut.Color = float4((flags & kFlagSky) != 0u ? SkyRadiance(dir) : float3(0.25, 0.5, 0.8), 1.0);
        if (view == kViewSteps)
            PSOut.Color = float4(saturate(float(hit.steps) / 256.0).xxx, 1.0);
        else if (view != kViewNone)
            PSOut.Color = float4(0.0, 0.0, 0.0, 1.0);
        PSOut.Dist = kSkyDistance;
        return;
    }

    const float3 p = camera + dir * hit.t;
    const float3 faceNormal = float3(hit.normal);
    // How many pixels the hit cube spans: the blend between the cube's own face (large on screen:
    // the John Lin close-up look, cubes visibly cubes) and the tree's averaged surface normal
    // (cubes near pixel size: the staircase must not shade as a staircase, or it moires).
    const float cubePixels = hit.cubeEdge / max(hit.t * g_ShadeParams.w, 1.0e-6);
    const float faceWeight = saturate((cubePixels - 1.5) / 3.0);
    const bool haveSmooth = !hit.solidLeaf && dot(hit.smoothNormal, hit.smoothNormal) > 0.01;
    const float3 smoothNormal = haveSmooth ? normalize(hit.smoothNormal) : faceNormal;
    const float3 normal = normalize(lerp(smoothNormal, faceNormal, faceWeight));

    // Goal 278: band-limit the albedo toward the hit node's average as the cube approaches pixel
    // size. `faceWeight` is 1 when the cube is large on screen (shade it as itself -- the John Lin
    // close-up) and 0 when it is sub-pixel. Colours interpolate; material IDs do not, which is why
    // this blends the resolved albedo rather than choosing between two IDs.
    float3 albedoBase = MaterialAlbedo(hit.material);
    if ((flags & kFlagFilterAlbedo) != 0u && hit.hasNodeAlbedo)
        albedoBase = lerp(hit.nodeAlbedo, albedoBase, faceWeight);

    // Goal 285: the deliberate stipple, applied to the ALBEDO so it is lit like the surface it
    // sits on rather than added afterwards as a screen effect.
    float stipple = 0.0;
    if ((flags & kFlagStipple) != 0u)
    {
        const float amp = MaterialStipple(hit.material) * g_WaveParams.z;
        if (amp > 0.0)
        {
            // The world scale is a CONSTANT, and it took a measurement to get that right: the
            // first version passed a distance-dependent scale AND divided by the zoom cycle, which
            // compensates for distance twice and left the pattern an octave-and-a-half too coarse
            // to see (moire 1.028 against 1.024 without it -- i.e. no effect at all).
            //
            // Benard's construction IS the distance compensation. A fixed world frequency with
            // octave scales tied to 2^floor(log2 t) has a screen period that is constant in t --
            // that is the property being bought. The constant is chosen so the heaviest octave
            // (a2, scale 4x base) lands on the wanted screen period.
            // THE 4.0 IS THE DERIVATION, NOT A CALIBRATION, and that distinction is the honest
            // state of this constant. It puts the heaviest octave (a2, scale 4x base) on the
            // wanted screen period. An attempt to calibrate it against a measured render was
            // ABANDONED because the instrument was wrong, not the number: a landscape pose spans
            // many distances at once, so "the delivered screen period" is not a single quantity
            // there, and successive measurements read 25.6 / 32.0 / 18.3 / 32.0 px for monotonically
            // increasing requests -- noise. The right instrument is a fixed camera at four
            // distances from ONE slope, which is goal 285's own Check and is not yet built
            // (goal 285a). Until it is, --stipple-period is a relative knob, not an absolute one.
            const float worldScale = 1.0 / max(4.0 * g_WaveParams.w * g_ShadeParams.w, 1.0e-9);
            // Band-limit exactly as the grain does, and for the same reason: a pattern at pixel
            // frequency is structured noise against the pixel grid and becomes its own moire. This
            // fade is what keeps the thing this pass just removed from being reintroduced.
            const float periodPixels = g_WaveParams.w;
            const float fade = saturate((periodPixels - 2.0) / 2.0);
            stipple = amp * fade * Hatch(p, max(hit.t, 1.0e-4), worldScale);
        }
    }

    const float n1 = ValueNoise(p.xz * (1.0 / 24.0));
    const float n2 = ValueNoise(p.xz * (1.0 / 7.0) + 17.31);
    const float mottle = 0.90 + 0.20 * (0.65 * n1 + 0.35 * n2);
    // Per-cube brightness grain (Binks' recipe: fade the pattern toward its mean as it approaches
    // pixel frequency). The cube's integer coordinates come from a point just inside its hit face.
    float grain = 1.0;
    if ((flags & kFlagGrain) != 0u && hit.cubeEdge > 0.0)
    {
        const float3 cell = floor((p - faceNormal * (0.5 * hit.cubeEdge)) / hit.cubeEdge);
        // Gone by 1.5 px (a per-cube hash at pixel frequency is structured noise against the
        // pixel grid -- its own moire), full from 4 px up.
        const float amplitude = g_ShadeParams.y * saturate((cubePixels - 1.5) / 2.5);
        grain = 1.0 + amplitude * (Hash3(cell) * 2.0 - 1.0);
    }
    float3 albedo = albedoBase * mottle * grain * (1.0 + stipple);

    const float diffuse = saturate(dot(normal, -kSunDirection));
    // Secondary-ray origins: half a finest voxel off the hit FACE (into the cell the primary ray
    // just crossed, so the origin is air at every resolution) plus a distance-scaled float
    // epsilon, then lifted along the averaged surface normal by a fraction of the hit cube. The
    // lift is what keeps a voxel STAIRCASE from shadowing itself: a slope of tangent s built from
    // steps of any size puts s/tan(sun elevation) of every tread in its own riser's shadow (47%
    // at 45 degrees under this sun), scale-free, so it shows at every LOD as terraced darkening
    // and, at pixel-sized steps, as moire (Gustafsson's "extreme shadow acne", research/
    // lin-look-log.md §3). One cube along the normal clears the riser; AO keeps half so contact
    // occlusion between grains survives.
    // A solid leaf (a water body's top, an underground cube) is its own flat face: no lift.
    const float liftEdge = hit.solidLeaf ? 0.0 : hit.cubeEdge;
    const float3 faceOffset = p + faceNormal * (g_TreeParams.z * 0.5 + hit.t * 1.0e-4);
    const float3 shadowOrigin = faceOffset + smoothNormal * liftEdge;
    const float3 aoOrigin = faceOffset + smoothNormal * (0.5 * liftEdge);
    float lit = 1.0;
    if ((flags & kFlagShadows) != 0u && diffuse > 0.0)
    {
        const Hit shadow = TraceGrid(shadowOrigin, -kSunDirection, g_TreeParams.x * g_TreeParams.y, 0.0, 1.0e30, 0.0,
                                    kSecondaryCoverage, 0.0);
        lit = shadow.hit ? 0.0 : 1.0;
    }
    float ao = 1.0;
    if ((flags & kFlagAO) != 0u)
    {
        ao = AmbientOcclusion(aoOrigin, normal, PSIn.Pos.xy, hit.t);
    }

    const float3 skyAmbient    = float3(0.34, 0.33, 0.30);
    const float3 groundAmbient = float3(0.14, 0.15, 0.19);
    const float3 ambient = lerp(groundAmbient, skyAmbient, normal.y * 0.5 + 0.5);
    // Wind, shading domain (Prompt 001 C6.1 / D3). Foliage does not MOVE here -- the voxels it is
    // made of are stored, and nothing published animates stored ray-marched geometry -- but its
    // brightness does, at the flutter band that reads as individual leaves catching the light
    // (research/tree-motion-growth-and-appearance.md �6.2). Gust and flutter together: the gust
    // sweeps whole canopies light and dark as it crosses them, the flutter shimmers their surface.
    // At distance, with TAA, this is what makes a valley read as alive at zero geometry cost.
    if (MaterialShading(hit.material) == MAT_SHADING_FOLIAGE && g_WindDirSpeed.z > 0.0)
    {
        const float t = g_CameraPosWorld.w;
        const float gust = WindGust(p, g_WindDirSpeed.xy, t, g_WindGustFlutter.x, g_WindGustFlutter.y);
        const float flutter = WindFlutter(p, t, g_WindGustFlutter.z, g_WindGustFlutter.w);
        // Scaled by the wind's own strength, so --wind-speed 0.5 is genuinely calmer and
        // --no-wind (base speed 0) is bit-for-bit the pre-wind image.
        const float strength = saturate(g_WindDirSpeed.z / 6.0);
        albedo *= 1.0 + strength * (0.10 * flutter + 0.06 * gust);
    }

    float3 color = albedo * (ambient * ao + kSunColor * diffuse * lit);
    if (MaterialShading(hit.material) == MAT_SHADING_WATER)
    {
        // E3's shore fade is 1.0 here, deliberately. A wave cannot orbit in water thinner than
        // half its wavelength, and world/water::shore_fade implements exactly that -- but the
        // SHADER has no depth to feed it: a probe ray straight down from the surface hits the
        // water column's own voxels immediately, so it would need a water-skipping traversal
        // variant, which is a change to the marcher the 7,000-ray oracle guards. The CPU side
        // (the swimmer's surface) does apply it. Note also that this implementation displaces
        // NORMALS, not geometry, so the artefact E3 exists to prevent -- crests clipping through
        // the sand -- cannot occur here; what is missing is only that shallow water should look
        // calmer. Recorded as a follow-up goal rather than guessed at.
        color = ShadeWater(p, -dir, g_CameraPosWorld.w, 1.0) * lerp(0.6, 1.0, lit);
    }

    // PROMPT 007 GOAL 327: Koschmieder's law, with the rate constant the physics fixes rather than
    // an artist-chosen number.
    //
    // What was here: `density = 0.0030 * (0.80 + 0.20*exp2(-y*0.012))` and a falloff SQUARED in
    // distance. Three things were wrong with it. The 0.0030 was a bare constant; the squared
    // distance is a Gaussian, not the exponential Koschmieder's law actually is; and the height
    // term was an unexplained 1/58 m e-fold, which is nothing like an atmosphere.
    //
    // What is here now:
    //   * `sigma` comes from an AUTHORED meteorological visibility V. Eye research §8.2 and
    //     aesthetics §9.6: contrast decays as e^(-sigma*d), and V is the distance at which it
    //     reaches the convention's threshold. The CPU derives sigma = k/V and passes it; "how far
    //     can you see" is a number a person can hold and 0.0030 was not.
    //   * The height term is the BAROMETRIC form, exp(-y/H) with H the atmospheric scale height,
    //     because extinction is height-dependent for the reason air density is. Over this world's
    //     ~112 m of relief that varies sigma by 1.3% -- physically right, practically negligible,
    //     and correct in advance if the world ever gets mountains. It is evaluated at the HIT
    //     point rather than integrated along the ray, which is the same approximation the previous
    //     code made and is exact for a horizontal view.
    //   * The lerp toward `SkyGradient(dir)` STAYS, and is not merely an aesthetic choice: it is
    //     Narasimhan & Nayar's two-term model, `color*T + L_inf*(1-T)`, with the airlight radiance
    //     L_inf being the sky in that direction. It is also the decided-against-flat-fog constraint
    //     from docs/progress.md -- flat fog made fogged ridges vanish while their darker trees
    //     lingered as floating dashes.
    const float dist = hit.t;
    const float sigma = g_FogParams.x * exp(-max(p.y, 0.0) / max(g_FogParams.y, 1.0));
    const float transmission = exp(-sigma * dist);
    const float fogAmount = saturate(1.0 - transmission);
    color = lerp(color, SkyGradient(dir), fogAmount);

    if (view != kViewNone)
    {
        if (view == kViewLit)               color = lit.xxx;
        else if (view == kViewAO)           color = ao.xxx;
        else if (view == kViewNormal)       color = normal * 0.5 + 0.5;
        else if (view == kViewFaceNormal)   color = faceNormal * 0.5 + 0.5;
        else if (view == kViewLevel)        color = LevelColor(hit.level);
        else if (view == kViewSteps)        color = saturate(float(hit.steps) / 256.0).xxx;
        else if (view == kViewCoverage)     color = hit.coverage.xxx;
        else if (view == kViewCubePixels)   color = saturate(cubePixels / 8.0).xxx;
        else if (view == kViewSmoothNormal) color = haveSmooth ? smoothNormal * 0.5 + 0.5 : float3(1.0, 0.0, 1.0);
        else if (view == kViewLodCube)      color = hit.lodCube ? float3(1.0, 0.2, 0.1) : float3(0.1, 0.4, 1.0);
        else if (view == kViewMaterial)     color = MaterialAlbedo(hit.material);
        else if (view == kViewDistance)     color = frac(hit.t * 0.5).xxx; // 2 m bands
    }

    const float4 clip = mul(g_ViewProj, float4(p, 1.0));
    PSOut.Color = float4(color, 1.0);
    PSOut.Dist = hit.t;
}
