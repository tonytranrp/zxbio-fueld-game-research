// Post-process composite (goals.md goals 22/24): samples the HDR scene (already bloom-composited
// by DiligentFX's final upsample pass when bloom is on) and applies the tone curve into the LDR
// swap chain. CompositeConstantsCpu in post_process.cpp mirrors the cbuffer -- update together.

Texture2D    g_SourceColor;
SamplerState g_SourceColor_sampler;

cbuffer CompositeConstants
{
    float4 g_Params; // x: tonemap enabled (1/0); yzw unused
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

// Soft-knee shoulder (goal 24): IDENTITY below the knee, tanh rolloff above it. Chosen over a
// full filmic curve (ACES was tried first and visibly regraded the whole scene -- mids washed out
// ~30% in the viewed capture) because this scene's authored range tops out around 0.9 and only
// bloom overshoot exceeds it; the curve's one job is keeping those highlights from clipping to
// flat white, not relighting terrain that already looks right. C1-continuous at the knee.
float3 SoftKnee(float3 x)
{
    const float knee = 0.75;
    const float3 above = knee + (1.0 - knee) * tanh((x - knee) / (1.0 - knee));
    return x < knee ? x : above; // componentwise select
}

void main(in PSInput PSIn, out PSOutput PSOut)
{
    const float3 hdr = g_SourceColor.Sample(g_SourceColor_sampler, PSIn.UV).rgb;
    PSOut.Color = float4(g_Params.x > 0.5 ? SoftKnee(hdr) : saturate(hdr), 1.0);
}
