// Auto-exposure metering, pass 2 (goal 237): an 8x8 box reduction of the (weighted log-luminance,
// weight) pairs meter.psh.hlsl produced. Run once, 64x64 -> 8x8; the last 64 values are summed on
// the CPU, which is where the adaptation lives anyway (see auto_exposure.cpp for why).

Texture2D    g_SourceColor;
SamplerState g_SourceColor_sampler;

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

struct PSOutput
{
    float2 Value : SV_TARGET;
};

void main(in PSInput PSIn, out PSOutput PSOut)
{
    // The output texel's footprint in source UV, taken from the rasterizer rather than a constant:
    // one fewer number that can disagree with the render-target size.
    const float2 texel = float2(ddx(PSIn.UV.x), ddy(PSIn.UV.y));
    float2 total = float2(0.0, 0.0);
    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            const float2 uv = PSIn.UV + (float2(x, y) - 3.5) * 0.125 * texel;
            // A SUM, not a mean: the weights have to survive the reduction so the CPU can divide
            // once at the end. Averaging here would divide by a constant 64 and lose the mask.
            total += g_SourceColor.Sample(g_SourceColor_sampler, uv).rg;
        }
    }
    PSOut.Value = total;
}
