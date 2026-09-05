// Sparse-brick-octree ray marcher (docs/goals.md Group X, research/micro-voxel-pivot-log.md):
// one fullscreen pass, one primary ray per pixel through the flat node/brick arrays that
// world/svo/brick_tree.hpp uploads verbatim. TraceRay() below is a statement-for-statement port of
// world/svo/src/ray_trace.cpp -- the CPU reference the brute-force oracle tests validate and
// tools/svo_render renders frames with. Keep the two in lockstep: a traversal change lands in
// ray_trace.cpp, passes its oracle, and is then mirrored here.
//
// Writes SV_Depth from the hit position so the existing post chain / overlay see a real depth
// buffer; misses shade the analytic sky at far depth. Shading is the terrain pass's model (sun +
// hemisphere ambient + albedo mottle + exp2 height fog + the fresnel water path) plus what only a
// ray marcher gets cheaply: a traced sun-shadow ray and a short-ray hemisphere AO.

#include "sky_common.fxh"

cbuffer MarchConstants
{
    column_major float4x4 g_InvViewProj;
    column_major float4x4 g_ViewProj;
    float4 g_CameraPosWorld;   // xyz camera position; w = elapsed seconds (water ripple phase)
    float4 g_TreeOrigin;       // xyz world-space min corner of the root; w = root edge (meters)
    float4 g_TreeParams;       // x = lod pixel angle (radians/pixel), y = shadow lod multiplier,
                               // z = finest voxel edge (meters), w = AO ray length (meters)
    uint4  g_TreeInts;         // x = voxel bits V, y = max brick level, z = root node offset,
                               // w = flags: 1 shadows, 2 lod march, 4 AO, 8 tree present, 16 sky
    float4 g_MaterialColors[8];
};

StructuredBuffer<uint> g_Nodes;
StructuredBuffer<uint> g_Bricks;

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

struct PSOutput
{
    float4 Color : SV_TARGET;
    float  Depth : SV_Depth;
};

static const uint kFlagShadows = 1u;
static const uint kFlagLodMarch = 2u;
static const uint kFlagAO = 4u;
static const uint kFlagTree = 8u;
static const uint kFlagSky = 16u;
static const uint kBrickWords = 144u;
static const uint kBrickMaskWords = 16u;
static const uint kMaxIterations = 2048u;

struct Hit
{
    bool  hit;
    float t;
    uint  material;
    int3  normal;
    int   level;
    bool  lodCube;
    uint  steps;
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
    m.steps = 0u;
    return m;
}

// Mirror of world::svo::trace_ray (ray_trace.cpp). t is in units of |rayDir| (meters for a unit
// direction). lodPixelAngle == 0 disables the LOD early-out; tOffset is the distance already
// travelled from the camera for secondary rays.
Hit TraceRay(float3 rayOrigin, float3 rayDir, float lodPixelAngle, float tOffset, float maxT)
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

    uint stack[22];
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
                Hit h;
                h.hit = true;
                h.t = t;
                h.material = (header >> 16) & 0xFFu;
                h.normal = NormalFrom(lastAxis, step, d);
                h.level = level;
                h.lodCube = false;
                h.steps = iteration;
                return h;
            }
            if (kind == 1u)
            {
                const int shift = V - level - 3;
                const int cellShift = V - level;
                const uint brickBase = g_Nodes[node + 1u] * kBrickWords;
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
                        Hit h;
                        h.hit = true;
                        h.t = t;
                        h.material = (g_Bricks[brickBase + kBrickMaskWords + (index >> 2)] >> ((index & 3u) * 8u)) & 0xFFu;
                        h.normal = NormalFrom(lastAxis, step, d);
                        h.level = level;
                        h.lodCube = false;
                        h.steps = iteration;
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
                if (childEdgeWorld < (t + tOffset) * lodPixelAngle)
                {
                    Hit h;
                    h.hit = true;
                    h.t = t;
                    h.material = (header >> 16) & 0xFFu;
                    h.normal = NormalFrom(lastAxis, step, d);
                    h.level = level;
                    h.lodCube = true;
                    h.steps = iteration;
                    return h;
                }
            }
            const int sh = V - childLevel;
            const int octant = ((c.x >> sh) & 1) | (((c.y >> sh) & 1) << 1) | (((c.z >> sh) & 1) << 2);
            const uint mask = header & 0xFFu;
            if ((mask & (1u << uint(octant))) != 0u)
            {
                const uint below = mask & ((1u << uint(octant)) - 1u);
                stack[childLevel] = g_Nodes[node + 1u + countbits(below)];
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

// ---- shading (terrain.psh.hlsl's model) ---------------------------------------------------------

float Hash2(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
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

float3 ShadeWater(float3 worldPos, float3 viewDir, float timeSeconds)
{
    const float2 p = worldPos.xz;
    const float2 d1 = normalize(float2(1.0, 0.35));
    const float2 d2 = normalize(float2(-0.42, 1.0));
    float3 n = float3(0.0, 1.0, 0.0);
    n.xz += 0.040 * cos(dot(p, d1) * 0.90 + timeSeconds * 1.7) * d1;
    n.xz += 0.025 * cos(dot(p, d2) * 1.70 + timeSeconds * 2.6) * d2;
    const float3 rippleN = normalize(n);
    const float3 body = float3(0.06, 0.22, 0.36);
    const float cosTheta = saturate(dot(viewDir, rippleN));
    const float fresnel = 0.02 + 0.98 * pow(1.0 - cosTheta, 5.0);
    const float3 reflection = SkyRadiance(reflect(-viewDir, rippleN));
    const float3 halfVec = normalize(viewDir - kSunDirection);
    const float3 glint = kSunColor * (2.5 * pow(saturate(dot(rippleN, halfVec)), 256.0));
    return lerp(body, reflection, fresnel) + glint;
}

// Short-ray hemisphere AO: four fixed directions around the normal, rotated per pixel by a hash so
// the pattern dithers instead of banding. Coarse LOD and a short max distance keep it cheap.
float AmbientOcclusion(float3 p, float3 n, float2 pixel, float tOffset)
{
    const float rayLength = g_TreeParams.w;
    const float lod = g_TreeParams.x * 4.0;
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
        const Hit h = TraceRay(p, dir, lod, tOffset, rayLength);
        if (h.hit)
            occluded += 1.0 - saturate(h.t / rayLength);
    }
    return 1.0 - 0.6 * (occluded * 0.25);
}

void main(in PSInput PSIn, out PSOutput PSOut)
{
    const float2 ndc = float2(PSIn.UV.x * 2.0 - 1.0, 1.0 - PSIn.UV.y * 2.0);
    const float4 farWorld = mul(g_InvViewProj, float4(ndc, 1.0, 1.0));
    const float3 camera = g_CameraPosWorld.xyz;
    const float3 dir = normalize(farWorld.xyz / farWorld.w - camera);
    const uint flags = g_TreeInts.w;

    const float lodAngle = (flags & kFlagLodMarch) != 0u ? g_TreeParams.x : 0.0;
    const Hit hit = TraceRay(camera, dir, lodAngle, 0.0, 1.0e30);
    if (!hit.hit)
    {
        PSOut.Color = float4((flags & kFlagSky) != 0u ? SkyRadiance(dir) : float3(0.25, 0.5, 0.8), 1.0);
        PSOut.Depth = 1.0;
        return;
    }

    const float3 p = camera + dir * hit.t;
    const float3 normal = float3(hit.normal);
    const float3 albedoBase = g_MaterialColors[min(hit.material, 7u)].rgb;
    const float n1 = ValueNoise(p.xz * (1.0 / 24.0));
    const float n2 = ValueNoise(p.xz * (1.0 / 7.0) + 17.31);
    const float mottle = 0.90 + 0.20 * (0.65 * n1 + 0.35 * n2);
    const float3 albedo = albedoBase * mottle;

    const float diffuse = saturate(dot(normal, -kSunDirection));
    // Offset the secondary-ray origin off the face by half a finest voxel (plus a distance-scaled
    // epsilon) so the ray does not re-hit the surface it starts on.
    const float3 offsetOrigin = p + normal * (g_TreeParams.z * 0.5 + hit.t * 1.0e-4);
    float lit = 1.0;
    if ((flags & kFlagShadows) != 0u && diffuse > 0.0)
    {
        const Hit shadow = TraceRay(offsetOrigin, -kSunDirection, g_TreeParams.x * g_TreeParams.y, hit.t, 1.0e30);
        lit = shadow.hit ? 0.0 : 1.0;
    }
    float ao = 1.0;
    if ((flags & kFlagAO) != 0u)
    {
        ao = AmbientOcclusion(offsetOrigin, normal, PSIn.Pos.xy, hit.t);
    }

    const float3 skyAmbient    = float3(0.34, 0.33, 0.30);
    const float3 groundAmbient = float3(0.14, 0.15, 0.19);
    const float3 ambient = lerp(groundAmbient, skyAmbient, normal.y * 0.5 + 0.5);
    float3 color = albedo * (ambient * ao + kSunColor * diffuse * lit);
    if (hit.material == 3u)
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

    const float4 clip = mul(g_ViewProj, float4(p, 1.0));
    PSOut.Color = float4(color, 1.0);
    PSOut.Depth = clip.z / clip.w;
}
