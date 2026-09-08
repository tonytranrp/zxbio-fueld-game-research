// Prompt 007 goal 339 (= docs/goals.md goal 194): the instanced raster grass overlay.
//
// NO VERTEX BUFFER AND NO INPUT LAYOUT. A blade is generated from SV_VertexID and its instance is
// read from a structured buffer by SV_InstanceID. That is not cleverness for its own sake: an input
// layout is a third place (after the C++ struct and the shader) where the per-blade format has to
// agree, and this engine already has one format-agreement bug per prompt without inventing more.
//
// PER-BLADE DATA IS 32 BYTES -- two float4s. `research/grass-rendering-research.md` §4 measures Ghost
// of Tsushima at 16 floats (64 B) for ~83,000 blades in 2.5 ms, Acerola at 7 floats (28 B), and
// Project-GrassFlow at 32 B. So this sits exactly on GrassFlow's figure and at half of GoT's, and
// the reason it can is that everything derivable is derived: the blade's shape comes from the
// vertex id, its wind from the shared field by world position, and its identity from a hash the CPU
// already computed for the voxel tier (goal 340).

#include "wind.fxh"

cbuffer GrassConstants
{
    float4x4 g_ViewProj;
    float4   g_CameraPos;     // xyz + animation time
    float4   g_WindDirSpeed;  // xy direction, z base speed, w gust amplitude
    float4   g_WindGustFlutter;
    float4   g_Params;        // x = segments, y = width scale, z = bend gain, w = 1/far distance
    float4   g_PlayerBend;    // xyz player position, w bend radius (0 = off)
};

StructuredBuffer<float4> g_Blades; // 2 float4s per blade: (base.xyz, height), (lean.xy, width, phase)

struct VSOut
{
    float4 Pos      : SV_POSITION;
    float3 World    : WORLD;
    float  Along    : ALONG;   // 0 at the root, 1 at the tip -- the ambient-occlusion ramp
    float  Side     : SIDE;    // -1..1 across the blade, for the round-ish normal
    float  Distance : DIST;    // view distance, for the depth composition in the PS
};

VSOut main(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    const float4 a = g_Blades[iid * 2u + 0u];
    const float4 b = g_Blades[iid * 2u + 1u];
    const float3 base = a.xyz;
    const float height = a.w;
    const float2 lean = b.xy;
    const float width = b.z * g_Params.y;
    const float phase = b.w;

    const uint segments = uint(g_Params.x);
    // Six vertices per segment, as a triangle list. A strip would be fewer, but a list draws every
    // blade in one call with no restart index and no topology that differs between backends.
    const uint seg = min(vid / 6u, segments - 1u);
    const uint corner = vid % 6u;
    const float t0 = float(seg) / float(segments);
    const float t1 = float(seg + 1u) / float(segments);
    // 0,1,2, 2,1,3 over (t0,-1) (t0,+1) (t1,-1) (t1,+1).
    const bool upper = (corner == 2u) || (corner == 4u) || (corner == 5u);
    const bool right = (corner == 1u) || (corner == 3u) || (corner == 5u);
    const float t = upper ? t1 : t0;
    const float side = right ? 1.0 : -1.0;

    // The blade tapers to a point, so the top segment's two vertices coincide and the quad becomes
    // a triangle -- no separate tip primitive, no special case.
    const float taper = saturate(1.0 - t * t);

    // WIND. The same field everything else reads, sampled at the blade's ROOT so a whole tuft moves
    // together, with the blade's own phase offsetting it so neighbours are not in lockstep.
    float3 offset = float3(0.0, 0.0, 0.0);
    if (g_WindDirSpeed.z > 0.0)
    {
        const float time = g_CameraPos.w + phase;
        const float gust = WindGust(base, g_WindDirSpeed.xy, time, g_WindGustFlutter.x, g_WindGustFlutter.y);
        const float flutter = WindFlutter(base, time, g_WindGustFlutter.z, g_WindGustFlutter.w);
        const float2 along = g_WindDirSpeed.xy;
        const float2 across = float2(-g_WindDirSpeed.y, g_WindDirSpeed.x);
        const float strength = saturate(g_WindDirSpeed.z / 6.0) * g_Params.z;
        // t*t, not t: a blade is a cantilever and its deflection grows as the square of the
        // distance from the root. A linear bend reads as the whole blade sliding sideways.
        const float2 sway = (along * (0.55 + 0.45 * gust) + across * (0.30 * flutter)) * strength;
        offset.xz += sway * height * t * t;
    }

    // PLAYER BEND, the cheapest of the three families the research documents (§2.2): a sphere mask
    // that pushes blades radially away from the body. No trail, no texture, no history -- and it is
    // the one that needs no state at all, which is what makes it right for a first version.
    if (g_PlayerBend.w > 0.0)
    {
        const float3 away = base - g_PlayerBend.xyz;
        const float2 flat = float2(away.x, away.z);
        const float d = length(flat);
        if (d < g_PlayerBend.w && d > 1e-4)
        {
            const float push = (1.0 - d / g_PlayerBend.w);
            offset.xz += normalize(flat) * push * push * height * 0.8 * t * t;
        }
    }

    // The blade's own lean, also cantilever-shaped, so a still field is not a bed of spikes.
    offset.xz += lean * height * t * t;

    // A width axis square to the view, so a blade is never edge-on and invisible. The research's
    // orientation cull removes those; this engine draws so few that turning them instead is
    // cheaper than culling them.
    const float3 toCam = g_CameraPos.xyz - base;
    const float3 flatCam = normalize(float3(toCam.x, 0.0, toCam.z) + float3(1e-5, 0.0, 0.0));
    const float3 widthAxis = normalize(cross(float3(0.0, 1.0, 0.0), flatCam));

    float3 world = base;
    world.y += height * t;
    world += offset;
    world += widthAxis * (side * width * 0.5 * taper);

    VSOut o;
    // mul(M, v), the convention terrain.vsh.hlsl uses -- this project stores GLM matrices
    // column-major and does not set PackMatrixRowMajor. The other order compiles and silently
    // transposes, which puts every blade off-screen and looks exactly like nothing drawing.
    o.Pos = mul(g_ViewProj, float4(world, 1.0));
    o.World = world;
    o.Along = t;
    o.Side = side * taper;
    o.Distance = length(world - g_CameraPos.xyz);
    return o;
}
