// Terrain pixel stage: material-palette lookup + one fixed directional light, simple Lambertian
// (PHASE_1_BRIEF.md §2.6 -- no textures in this phase; the palette is the slot a texture array
// upgrades into later). g_MaterialColors is sized/ordered by world::chunk::MaterialID and
// mirrored by kMaterialColors in render/diligent/src/pso_terrain.cpp -- update both together.

cbuffer MaterialPalette
{
    float4 g_MaterialColors[4];
};

struct PSInput
{
    float4 Pos      : SV_POSITION;
    float3 Normal   : NORMAL0;
    nointerpolation uint Material : TEXCOORD0;
};

struct PSOutput
{
    float4 Color : SV_TARGET;
};

void main(in PSInput PSIn, out PSOutput PSOut)
{
    const float3 lightDir = normalize(float3(0.4, -1.0, 0.25)); // sun, slightly off-axis so all face orientations shade distinctly
    const float3 normal   = normalize(PSIn.Normal);
    const float  diffuse  = saturate(dot(normal, -lightDir));
    const float3 albedo   = g_MaterialColors[min(PSIn.Material, 3u)].rgb;
    const float3 lit      = albedo * (0.25 + 0.75 * diffuse); // 0.25 ambient floor keeps unlit faces readable
    PSOut.Color = float4(lit, 1.0);
}
