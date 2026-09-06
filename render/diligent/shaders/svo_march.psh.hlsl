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
    // One record per material (render/diligent/detail/material_macros.hpp's material_record):
    // rgb = linear albedo, w = shading model. MATERIAL_COUNT and MAT_SHADING_* are macros the C++
    // side passes at shader creation from the material registry -- no material literal lives here.
    float4 g_Materials[MATERIAL_COUNT];
};

float3 MaterialAlbedo(uint material)
{
    return g_Materials[min(material, MATERIAL_COUNT - 1u)].rgb;
}

uint MaterialShading(uint material)
{
    return uint(g_Materials[min(material, MATERIAL_COUNT - 1u)].w + 0.5);
}

StructuredBuffer<uint> g_Nodes;
StructuredBuffer<uint> g_Bricks;

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

struct PSOutput
{
    float4 Color : SV_TARGET0;
    float  Dist  : SV_TARGET1; // hit distance in meters (sky: kSkyDistance)
    float  Depth : SV_Depth;
};

static const uint kFlagShadows = 1u;
static const uint kFlagLodMarch = 2u;
static const uint kFlagAO = 4u;
static const uint kFlagTree = 8u;
static const uint kFlagSky = 16u;
static const uint kFlagGrain = 32u;
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
static const uint kBrickWords = 144u;
static const uint kBrickMaskWords = 16u;
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
};

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
    return m;
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
void ReadAttributes(inout Hit h, uint stack[kMaxLevels], int attrLevel, float smoothPixelAngle)
{
    int L = attrLevel;
    if (smoothPixelAngle > 0.0)
    {
        const float wanted = h.t * smoothPixelAngle;
        [loop]
        while (L > 0 && g_TreeOrigin.w * exp2(-float(L)) < wanted)
            --L;
    }
    if (L >= 0)
    {
        const uint node = stack[L];
        const uint kind = (g_Nodes[node] >> 8) & 3u;
        if (kind != 2u)
        {
            const uint attr = g_Nodes[node + (kind == 0u ? kAttrSlotInternal : kAttrSlotBrick)];
            h.smoothNormal = AttrNormal(attr);
            h.coverage = AttrCoverage(attr);
            h.smoothLevel = L;
        }
    }
}

// Mirror of world::svo::trace_ray (ray_trace.cpp). t is in units of |rayDir| (meters for a unit
// direction). lodPixelAngle == 0 disables the LOD early-out; tOffset is added to the distance
// before the LOD test (0 for every ray this shader casts -- see TraceParams::t_offset);
// coverageThreshold: an early-out node under this volume coverage is descended instead of hit
// (TraceParams::lod_coverage_threshold).
Hit TraceRay(float3 rayOrigin, float3 rayDir, float lodPixelAngle, float tOffset, float maxT, float smoothPixelAngle,
             float coverageThreshold)
{
    Hit miss = MakeMiss();
    if ((g_TreeInts.w & kFlagTree) == 0u)
        return miss;

    const int   V = int(g_TreeInts.x);
    const float cells = exp2(float(V));
    const float rootEdge = g_TreeOrigin.w;
    const float3 o = (rayOrigin - g_TreeOrigin.xyz) / rootEdge;
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
    const int n = int(cells);
    int3 c;
    {
        const float3 p = o + t * d;
        c = clamp(int3(floor(p * cells)), int3(0, 0, 0), int3(n - 1, n - 1, n - 1));
        if (!inside)
        {
            const int3 m = AxisMask(enterAxis);
            c = c * (int3(1, 1, 1) - m) + m * (CompI(step, enterAxis) > 0 ? 0 : n - 1);
        }
    }

    uint stack[kMaxLevels];
    stack[0] = g_TreeInts.z;
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
            const uint header = g_Nodes[node];
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
                ReadAttributes(h, stack, level - 1, smoothPixelAngle);
                return h;
            }
            if (kind == 1u)
            {
                const int shift = V - level - 3;
                const int cellShift = V - level;
                const uint brickBase = g_Nodes[node + kBrickIndexSlot] * kBrickWords;
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
                    if (((g_Bricks[brickBase + (index >> 5)] >> (index & 31u)) & 1u) != 0u)
                    {
                        Hit h = MakeMiss();
                        h.hit = true;
                        h.t = t;
                        h.material = (g_Bricks[brickBase + kBrickMaskWords + (index >> 2)] >> ((index & 3u) * 8u)) & 0xFFu;
                        h.normal = NormalFrom(lastAxis, step, d);
                        h.level = level;
                        h.lodCube = false;
                        h.steps = iteration;
                        h.cubeEdge = rootEdge * voxelEdge;
                        ReadAttributes(h, stack, level, smoothPixelAngle);
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
                    (coverageThreshold <= 0.0 || AttrCoverage(g_Nodes[node + kAttrSlotInternal]) >= coverageThreshold))
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
                    ReadAttributes(h, stack, level, smoothPixelAngle);
                    return h;
                }
            }
            const int sh = V - childLevel;
            const int octant = ((c.x >> sh) & 1) | (((c.y >> sh) & 1) << 1) | (((c.z >> sh) & 1) << 2);
            const uint mask = header & 0xFFu;
            if ((mask & (1u << uint(octant))) != 0u)
            {
                const uint below = mask & ((1u << uint(octant)) - 1u);
                stack[childLevel] = g_Nodes[node + kFirstChildSlot + countbits(below)];
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
float3 ShadeWater(float3 worldPos, float3 viewDir, float timeSeconds)
{
    const float2 p = worldPos.xz;
    const float2 d1 = normalize(float2(1.0, 0.35));
    const float2 d2 = normalize(float2(-0.42, 1.0));
    float3 n = float3(0.0, 1.0, 0.0);
    n.xz += 0.030 * cos(dot(p, d1) * 0.90 + timeSeconds * 1.7) * d1;
    n.xz += 0.020 * cos(dot(p, d2) * 1.70 + timeSeconds * 2.6) * d2;
    const float w1 = ValueNoise(p * 0.35 + float2(timeSeconds * 0.21, timeSeconds * 0.13));
    const float w2 = ValueNoise(p * 0.35 + float2(31.7 - timeSeconds * 0.17, timeSeconds * 0.24));
    const float f1 = ValueNoise(p * 2.1 + float2(timeSeconds * 0.9, -timeSeconds * 0.6));
    const float f2 = ValueNoise(p * 2.1 + float2(57.3 + timeSeconds * 0.5, timeSeconds * 0.8));
    n.xz += 0.035 * float2(w1 - 0.5, w2 - 0.5) + 0.05 * float2(f1 - 0.5, f2 - 0.5);
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
    const float rayLength = max(0.15, g_TreeParams.w * hitDistance * g_ShadeParams.w);
    const float lod = g_TreeParams.x * g_ShadeParams.z;
    // Tangent frame around n.
    const float3 helper = abs(n.y) < 0.9 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    const float3 tangent = normalize(cross(helper, n));
    const float3 bitangent = cross(n, tangent);
    const float rot = Hash2(pixel) * 6.2831853;
    float occluded = 0.0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        const float phi = rot + float(i) * 1.5707963;
        // ~45 degrees off the normal: cheap, and where occlusion actually lives for cube worlds.
        const float3 dir = normalize(n * 0.75 + (tangent * cos(phi) + bitangent * sin(phi)) * 0.66);
        const Hit h = TraceRay(p, dir, lod, 0.0, rayLength, 0.0, kSecondaryCoverage);
        if (h.hit)
            occluded += 1.0 - saturate(h.t / rayLength);
    }
    return 1.0 - 0.6 * (occluded * 0.25);
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
    const Hit hit = TraceRay(camera, dir, lodAngle, 0.0, 1.0e30, smoothAngle, 0.0);
    if (!hit.hit)
    {
        PSOut.Color = float4((flags & kFlagSky) != 0u ? SkyRadiance(dir) : float3(0.25, 0.5, 0.8), 1.0);
        if (view == kViewSteps)
            PSOut.Color = float4(saturate(float(hit.steps) / 256.0).xxx, 1.0);
        else if (view != kViewNone)
            PSOut.Color = float4(0.0, 0.0, 0.0, 1.0);
        PSOut.Dist = kSkyDistance;
        PSOut.Depth = 1.0;
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

    const float3 albedoBase = MaterialAlbedo(hit.material);
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
    const float3 albedo = albedoBase * mottle * grain;

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
        const Hit shadow = TraceRay(shadowOrigin, -kSunDirection, g_TreeParams.x * g_TreeParams.y, 0.0, 1.0e30, 0.0,
                                    kSecondaryCoverage);
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
    float3 color = albedo * (ambient * ao + kSunColor * diffuse * lit);
    if (MaterialShading(hit.material) == MAT_SHADING_WATER)
    {
        color = ShadeWater(p, -dir, g_CameraPosWorld.w) * lerp(0.6, 1.0, lit);
    }

    // exp2 height fog converging on the sky gradient (terrain.psh.hlsl's formula, goals 33/34/91).
    const float dist = hit.t;
    const float heightFactor = exp2(-max(p.y, 0.0) * 0.012);
    const float density = 0.0030 * (0.80 + 0.20 * heightFactor);
    const float rawFog = 1.0 - exp2(-(dist * density) * (dist * density) * 1.442695);
    const float fogAmount = saturate(rawFog * 1.12);
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
    PSOut.Depth = clip.z / clip.w;
}
