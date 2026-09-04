// Fullscreen triangle from SV_VertexID (no vertex buffer): the standard 3-vertex trick -- one
// oversized triangle covering the viewport, UVs derived from clip position.

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

void main(in uint VertexID : SV_VertexID, out PSInput PSIn)
{
    // (0,0), (2,0), (0,2) in UV space -> covers [0,1]^2 with one triangle.
    const float2 uv = float2(uint2(VertexID << 1u, VertexID) & 2u);
    PSIn.UV  = uv;
    PSIn.Pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
