// Analytic gradient sky + sun disc (goals.md Group L). Replaces the flat clear color: per-pixel
// world view direction reconstructed from the inverse view-projection, fed to the shared
// SkyRadiance model (sky_common.fxh -- the same palette terrain fog reads, goal 91).

#include "sky_common.fxh"

cbuffer SkyConstants
{
    column_major float4x4 g_InvViewProj;
    float4 g_CameraPosWorld; // xyz used
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

struct PSOutput
{
    float4 Color : SV_TARGET;
};

void main(in PSInput PSIn, out PSOutput PSOut)
{
    // UV -> NDC -> far-plane world point -> direction.
    const float2 ndc = float2(PSIn.UV.x * 2.0 - 1.0, 1.0 - PSIn.UV.y * 2.0);
    const float4 farWorld = mul(g_InvViewProj, float4(ndc, 1.0, 1.0));
    const float3 dir = normalize(farWorld.xyz / farWorld.w - g_CameraPosWorld.xyz);
    PSOut.Color = float4(SkyRadiance(dir), 1.0);
}
