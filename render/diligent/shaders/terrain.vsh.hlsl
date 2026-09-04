// Terrain vertex stage. Cbuffer mirrors live in render/diligent/detail/terrain_renderer_impl.hpp
// (FrameConstantsCpu / ChunkConstantsCpu) -- update both sides together. column_major is explicit
// so the raw GLM (column-major) upload is byte-identical under every backend compiler.

cbuffer FrameConstants
{
    column_major float4x4 g_ViewProj;
};

cbuffer ChunkConstants
{
    float4 g_ChunkOriginWorld; // xyz: chunk origin in world voxel units; w unused padding
};

struct VSInput
{
    float3 Pos      : ATTRIB0; // chunk-local voxel-space position, spans [-1, 32]
    float3 Normal   : ATTRIB1; // area-weighted Surface Nets normal, already unit length
    uint   Material : ATTRIB2; // world::chunk::MaterialID
};

struct PSInput
{
    float4 Pos      : SV_POSITION;
    float3 Normal   : NORMAL0;
    nointerpolation uint Material : TEXCOORD0; // integer varying -- must be flat
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    const float3 worldPos = VSIn.Pos + g_ChunkOriginWorld.xyz;
    PSIn.Pos      = mul(g_ViewProj, float4(worldPos, 1.0));
    PSIn.Normal   = VSIn.Normal;
    PSIn.Material = VSIn.Material;
}
