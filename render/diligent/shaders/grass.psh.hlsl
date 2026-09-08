// Prompt 007 goal 339's pixel half: shade a blade, and compose it against a ray-marched world that
// DOES NOT WRITE A DEPTH BUFFER.
//
// THAT IS THE WHOLE PROBLEM, and it is worth stating because the research's recommended technique
// assumes otherwise. `research/grass-rendering-research.md` §3's hybrid is "raster blades composed
// against the marcher's depth buffer" -- but goal 267 REMOVED the march's SV_Depth export after
// measuring it at 17% of the march on vk and 10% on d3d12, because nothing read it. Re-enabling it
// to hang grass off would be paying that 17% back for one feature.
//
// The engine already exports what is needed under a different name: the march's second render
// target is the HIT DISTANCE, written for the temporal resolve. A blade compares its own view
// distance against that texture at its own pixel and discards when the marched world is nearer.
// That is depth composition with no depth buffer, no change to the march, and no cost on any frame
// that has no grass in it.
//
// Grass-against-GRASS still needs real depth, and gets it: the overlay tests and writes the bound
// DSV, which nothing else on this path touches. So blades sort correctly among themselves and are
// occluded correctly by the world, from two different mechanisms, each doing the half it can.

Texture2D<float> g_HitDistance;
SamplerState     g_HitDistance_sampler;

cbuffer GrassPixelConstants
{
    float4 g_Albedo;      // rgb linear, w = the ambient-occlusion depth at the blade's root
    float4 g_SunDir;      // xyz, w unused
    float4 g_SunColor;    // rgb, w unused
    float4 g_Ambient;     // rgb sky ambient, w = 1 / viewport width (for the distance fetch)
    float4 g_Viewport;    // xy = 1/size, zw unused
};

struct PSIn
{
    float4 Pos      : SV_POSITION;
    float3 World    : WORLD;
    float  Along    : ALONG;
    float  Side     : SIDE;
    float  Distance : DIST;
};

struct PSOut
{
    float4 Color    : SV_TARGET0;
    float  HitDist  : SV_TARGET1;
};

PSOut main(PSIn PSIn)
{
    // The depth composition. A tiny bias so a blade standing exactly ON the ground is not eaten by
    // the ground it stands on: two centimetres, which is under one 7.8 mm voxel of slop and far
    // below anything a player can see.
    const float2 uv = PSIn.Pos.xy * g_Viewport.xy;
    const float worldDistance = g_HitDistance.SampleLevel(g_HitDistance_sampler, uv, 0);
    if (worldDistance > 0.0 && PSIn.Distance > worldDistance + 0.02)
    {
        discard;
    }

    // A blade's normal: mostly up, bowed across its width, so a field catches the light in bands
    // rather than reading as one flat colour.
    const float3 normal = normalize(float3(PSIn.Side * 0.75, 1.0, 0.0));
    const float diffuse = saturate(dot(normal, -g_SunDir.xyz));
    // Occlusion along the blade: a root sits in the sward and is dark, a tip is in the open. This is
    // the single largest thing that makes raster grass read as grass rather than as green needles.
    const float ao = lerp(g_Albedo.w, 1.0, PSIn.Along * PSIn.Along);

    PSOut o;
    o.Color = float4(g_Albedo.rgb * (g_Ambient.rgb * ao + g_SunColor.rgb * diffuse * ao), 1.0);
    // The blade's own distance goes into the hit-distance target too, so the temporal resolve
    // reprojects grass with the same rule it uses for the world. An overlay that left this alone
    // would be reprojected against the distance of whatever is BEHIND it, which is the classic way
    // an overlay ghosts -- the thing this goal's Check says to verify explicitly.
    o.HitDist = PSIn.Distance;
    return o;
}
