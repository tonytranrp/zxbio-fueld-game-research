// Auto-exposure metering, pass 1 (Prompt 003 goal 237): the HDR scene reduced to a small grid of
// (weighted log-luminance, weight) pairs, with the weight coming from a CROSSHAIR-CENTRED MASK.
//
// Why a mask rather than a frame average: `research/eye-camera-and-rendering.md` 5.7(b) --
// the adaptation state that matters perceptually is pooled over roughly 6 degrees around fixation
// (Vangorp et al. 2015), so a bright sky at the top of the screen must not crush a dark valley the
// player is looking into. This is UE's Exposure Metering Mask pattern with the pooling radius taken
// from the actual FOV and viewport rather than a hardcoded pixel count.
//
// MeterConstantsCpu in auto_exposure.cpp mirrors the cbuffer -- update both together.

Texture2D    g_SourceColor;
SamplerState g_SourceColor_sampler;

cbuffer MeterConstants
{
    // x: 1/cos of the half pooling angle is not what we need -- store tan(theta0) directly.
    // x: tan(pooling half-angle), y: tan(vertical half-FOV), z: aspect (w/h), w: 1 = crosshair
    // metering, 0 = flat global average (the A/B build goal 237's check asks for).
    float4 g_Meter;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

struct PSOutput
{
    float2 Value : SV_TARGET; // (sum of weight * log2(luminance), sum of weight)
};

// Rec. 709 luminance. The scene target is linear HDR, so this is a real photometric quantity up to
// the renderer's own arbitrary scale -- which is fine, because exposure only ever uses RATIOS.
float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

void main(in PSInput PSIn, out PSOutput PSOut)
{
    // 4x4 taps per output texel. The source is much larger than this target, so a single tap would
    // alias badly on a scene of 7.8 mm cubes -- and an exposure that flickers with sub-pixel detail
    // is worse than no auto-exposure at all.
    const float2 texel = float2(ddx(PSIn.UV.x), ddy(PSIn.UV.y));
    float logSum = 0.0;
    float weightSum = 0.0;

    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            const float2 uv = PSIn.UV + (float2(x, y) - 1.5) * 0.25 * texel;

            // Angle from the crosshair. The screen is a plane at distance 1 with half-height
            // tan(vFOV/2), so a point at NDC (nx, ny) sits at angle atan(|offset|) from the axis --
            // and the mask has to be computed this way, not in pixels, or it changes meaning the
            // moment the FOV or the window does.
            const float2 ndc = uv * 2.0 - 1.0;
            const float2 offset = float2(ndc.x * g_Meter.z, ndc.y) * g_Meter.y;
            const float tanTheta = length(offset);

            // Gaussian in the tangent, with the pooling half-angle as the 1/e point. Smooth on
            // purpose: a hard-edged mask makes the exposure jump when a bright object crosses the
            // boundary, which reads as the scene flickering rather than the eye adapting.
            const float r = tanTheta / max(g_Meter.x, 1e-5);
            const float weight = g_Meter.w > 0.5 ? exp(-r * r) : 1.0;

            const float3 hdr = g_SourceColor.Sample(g_SourceColor_sampler, uv).rgb;
            // The epsilon keeps log2 finite on a black pixel AND sets the floor of the metered
            // range: 1e-4 is about -13 EV, far below anything this renderer writes, so it never
            // biases a real measurement.
            logSum += weight * log2(max(Luminance(hdr), 1e-4));
            weightSum += weight;
        }
    }

    PSOut.Value = float2(logSum, weightSum);
}
