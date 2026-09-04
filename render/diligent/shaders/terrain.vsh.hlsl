// Terrain vertex stage. Cbuffer mirrors live in render/diligent/detail/terrain_renderer_impl.hpp
// (FrameConstantsCpu / ChunkConstantsCpu) -- update both sides together. column_major is explicit
// so the raw GLM (column-major) upload is byte-identical under every backend compiler.
//
// Inputs are the COMPRESSED 12-byte vertex (detail::GpuVertexCompressed): fixed-function
// normalized fetch delivers Pos/OctNormal as floats in [0,1], so the decode below is pure
// arithmetic -- deliberately no bit-manipulation intrinsics, which do not translate reliably
// across every Diligent shader-conversion path (engine-hardening brief Group K task 26).

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
    float4 Pos      : ATTRIB0; // UNORM16 x4: chunk-local position, fixed point at 1/1024 voxel (w unused -- DXGI has no 3x16 format)
    float2 Oct      : ATTRIB1; // UNORM8 x2: 16-bit octahedral normal
    uint   Material : ATTRIB2; // world::chunk::MaterialID
};

struct PSInput
{
    float4 Pos      : SV_POSITION;
    float3 Normal   : NORMAL0;
    nointerpolation uint Material : TEXCOORD0; // integer varying -- must be flat
};

// Mirror of world::meshing::decode_octahedral_16 (keep bit-compatible with the CPU encoder).
float3 DecodeOctahedral(float2 unorm8Pair)
{
    float2 e = unorm8Pair * 2.0 - 1.0;
    float3 n = float3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0)
    {
        float2 folded;
        folded.x = (1.0 - abs(e.y)) * (e.x >= 0.0 ? 1.0 : -1.0);
        folded.y = (1.0 - abs(e.x)) * (e.y >= 0.0 ? 1.0 : -1.0);
        n.xy = folded;
    }
    return normalize(n);
}

void main(in VSInput VSIn, out PSInput PSIn)
{
    // Mirror of world::meshing::dequantize_position_16: UNORM in [0,1] -> [-1, 32] voxel space.
    // 65535/1024 = 63.9990234375 is exactly representable in float32.
    const float3 localPos = VSIn.Pos.xyz * (65535.0 / 1024.0) - 1.0;
    const float3 worldPos = localPos + g_ChunkOriginWorld.xyz;
    PSIn.Pos      = mul(g_ViewProj, float4(worldPos, 1.0));
    PSIn.Normal   = DecodeOctahedral(VSIn.Oct);
    PSIn.Material = VSIn.Material;
}
