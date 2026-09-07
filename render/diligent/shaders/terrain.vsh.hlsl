// Terrain vertex stage. Cbuffer mirrors live in render/diligent/detail/terrain_renderer_impl.hpp
// (FrameConstantsCpu / ChunkConstantsCpu) -- update both sides together. column_major is explicit
// so the raw GLM (column-major) upload is byte-identical under every backend compiler.
//
// Inputs are the COMPRESSED 12-byte vertex (detail::GpuVertexCompressed): fixed-function
// normalized fetch delivers Pos/OctNormal as floats in [0,1], so the decode below is pure
// arithmetic -- deliberately no bit-manipulation intrinsics, which do not translate reliably
// across every Diligent shader-conversion path (engine-hardening brief Group K task 26).
//
// MATERIAL_COUNT and MAT_SHADING_* are macros the C++ side passes at shader creation from the
// material registry (world/materials, render/diligent/detail/material_macros.hpp): this shader
// carries no material literal.

#include "wind.fxh"

cbuffer FrameConstants
{
    column_major float4x4 g_ViewProj;
    float4 g_TimeAndPad;       // x = elapsed seconds (foliage sway); yzw unused
    // The ONE wind field (world/wind), same tuning the marcher gets. Shape constants arrive as
    // WIND_* macros from world/wind's own header, so the two paths cannot disagree.
    float4 g_WindDirSpeed;     // xy = horizontal direction, z = base speed, w = gust amplitude
    float4 g_WindGustFlutter;  // x = gust frequency, y = gust scroll, z = flutter Hz, w = flutter freq
};

cbuffer ChunkConstants
{
    float4 g_ChunkOriginWorld; // xyz: chunk origin in world voxel units; w unused padding
};

// One record per material (detail::material_record): rgb = albedo, w = shading model.
cbuffer MaterialPalette
{
    float4 g_Materials[MATERIAL_COUNT];
};

uint MaterialShading(uint material)
{
    return uint(g_Materials[min(material, MATERIAL_COUNT - 1u)].w + 0.5);
}

struct VSInput
{
    float4 Pos      : ATTRIB0; // UNORM16 x4: chunk-local position, fixed point at 1/1024 voxel (w unused -- DXGI has no 3x16 format)
    float2 Oct      : ATTRIB1; // UNORM8 x2: 16-bit octahedral normal
    uint   Material : ATTRIB2; // world::chunk::MaterialID
    float  AO       : ATTRIB3; // UNORM8: baked per-vertex concavity AO (1 = fully open)
};

struct PSInput
{
    float4 Pos      : SV_POSITION;
    float3 Normal   : NORMAL0;
    float3 WorldPos : TEXCOORD1; // for albedo variation now; distance fog reuses it later
    float  AO       : TEXCOORD2; // interpolates across triangles like the smooth normal does
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
    float3 worldPos = localPos + g_ChunkOriginWorld.xyz;

    // Goal 39: wind sway on tree CANOPIES only -- whatever material the registry shades as
    // foliage; trunks (Wood) and terrain get zero offset, so trunks visibly stay still while
    // canopies move. Group AC: the material record carries the shading model, so the old
    // "material 5 is Leaves" literal is gone -- the CPU/GPU boundary is crossed by the record.
    //
    // Prompt 001 C7: this used to be two hand-picked sine waves of its own. It now reads the ONE
    // wind field (world/wind), so the mesh path's canopies lean the way the svo path's shimmer and
    // the sea's waves do -- same direction, same gusts, same --wind-speed. That is the whole point
    // of Group B: nothing invents its own wind again.
    if (MaterialShading(VSIn.Material) == MAT_SHADING_FOLIAGE && g_WindDirSpeed.z > 0.0)
    {
        const float t = g_TimeAndPad.x;
        const float speed = WindSpeed(worldPos, g_WindDirSpeed.xy, t, g_WindDirSpeed.z,
                                      g_WindDirSpeed.w, g_WindGustFlutter.x, g_WindGustFlutter.y);
        const float flutter = WindFlutter(worldPos, t, g_WindGustFlutter.z, g_WindGustFlutter.w);
        // Lean downwind in proportion to the local wind, plus a small cross-wind flutter so the
        // canopy does not read as one rigid lean. 0.03 m per m/s puts a 6 m/s breeze at ~18 cm --
        // about where the old hand-tuned 0.15 m sat, so the mesh world's look is preserved.
        const float lean = speed * 0.03;
        worldPos.xz += g_WindDirSpeed.xy * lean;
        worldPos.xz += float2(-g_WindDirSpeed.y, g_WindDirSpeed.x) * (flutter * 0.05);
    }

    PSIn.Pos      = mul(g_ViewProj, float4(worldPos, 1.0));
    PSIn.Normal   = DecodeOctahedral(VSIn.Oct);
    PSIn.WorldPos = worldPos;
    PSIn.AO       = VSIn.AO;
    PSIn.Material = VSIn.Material;
}
