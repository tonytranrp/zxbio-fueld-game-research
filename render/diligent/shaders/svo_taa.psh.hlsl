// Temporal anti-aliasing resolve for the micro-voxel march (docs/goals.md goal 168,
// research/lin-look-log.md §4). The march pass jitters every primary ray by a sub-pixel Halton
// offset and writes each pixel's hit distance; this pass rebuilds the pixel's world position from
// that distance, reprojects it through the PREVIOUS frame's view-projection (the world is static,
// so camera motion is the only motion), and blends the previous resolved color in when it is
// still valid: on screen, the previous frame saw about the same distance there (a disocclusion
// test that needs no depth buffer read-back), and the color lies within the current frame's 3x3
// neighborhood (Playdead-style clamping so a bad reprojection fades instead of smearing).
//
// Two outputs: the resolved color for the post chain, and the same color plus this frame's hit
// distance in alpha as the next frame's history.

cbuffer TaaConstants
{
    column_major float4x4 g_InvViewProj;  // this frame, unjittered
    column_major float4x4 g_PrevViewProj; // last frame, unjittered
    float4 g_CameraPos;     // xyz this frame; w = blend weight of the new frame
    float4 g_PrevCameraPos; // xyz last frame; w = 1 when the history is valid
    float4 g_Jitter;        // xy = this frame's sub-pixel jitter (pixels), zw = 1 / viewport size
    float4 g_Params;        // x = relative distance tolerance, y = absolute tolerance (m)
};

Texture2D g_RawColor;
Texture2D g_Distance;
Texture2D g_History;
SamplerState g_History_sampler;

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

struct PSOutput
{
    float4 Color   : SV_TARGET0;
    float4 History : SV_TARGET1;
};

static const float kSkyDistance = 1.0e6;

void main(in PSInput PSIn, out PSOutput PSOut)
{
    const int2 pixel = int2(PSIn.Pos.xy);
    const float3 raw = g_RawColor.Load(int3(pixel, 0)).rgb;
    const float t = g_Distance.Load(int3(pixel, 0)).r;

    // The world position this pixel's (jittered) ray hit.
    const float2 uv = PSIn.UV + g_Jitter.xy * g_Jitter.zw;
    const float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    const float4 farWorld = mul(g_InvViewProj, float4(ndc, 1.0, 1.0));
    const float3 dir = normalize(farWorld.xyz / farWorld.w - g_CameraPos.xyz);
    const float3 worldPos = g_CameraPos.xyz + dir * min(t, kSkyDistance);

    // Where the previous frame saw it.
    const float4 prevClip = mul(g_PrevViewProj, float4(worldPos, 1.0));
    bool valid = g_PrevCameraPos.w > 0.5 && prevClip.w > 0.0;
    float2 prevUV = float2(0.0, 0.0);
    if (valid)
    {
        const float2 prevNdc = prevClip.xy / prevClip.w;
        prevUV = float2(prevNdc.x * 0.5 + 0.5, 0.5 - prevNdc.y * 0.5);
        valid = all(prevUV >= 0.0) && all(prevUV <= 1.0);
    }

    float3 color = raw;
    if (valid)
    {
        const float4 history = g_History.Sample(g_History_sampler, prevUV);
        // Disocclusion: the previous frame's hit distance there must match the distance from the
        // previous camera to this world position (sky reprojects against itself at kSkyDistance).
        const float expected = min(length(worldPos - g_PrevCameraPos.xyz), kSkyDistance);
        const float previous = min(history.a, kSkyDistance);
        if (abs(previous - expected) <= g_Params.x * expected + g_Params.y)
        {
            // Neighborhood clamp against the current frame's 3x3.
            float3 lo = raw;
            float3 hi = raw;
            [unroll]
            for (int dy = -1; dy <= 1; ++dy)
            {
                [unroll]
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const float3 c = g_RawColor.Load(int3(pixel + int2(dx, dy), 0)).rgb;
                    lo = min(lo, c);
                    hi = max(hi, c);
                }
            }
            const float3 clamped = clamp(history.rgb, lo, hi);
            color = lerp(clamped, raw, g_CameraPos.w);
        }
    }

    PSOut.Color = float4(color, 1.0);
    PSOut.History = float4(color, t);
}
