// Sky vertex stage: same SV_VertexID fullscreen triangle as fullscreen.vsh.hlsl, but emitted at
// depth 1.0 (the far plane) so LESS_EQUAL depth testing draws sky ONLY where no terrain wrote
// depth -- zero overdraw behind mountains.

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

void main(in uint VertexID : SV_VertexID, out PSInput PSIn)
{
    const float2 uv = float2(uint2(VertexID << 1u, VertexID) & 2u);
    PSIn.UV  = uv;
    PSIn.Pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
}
