#pragma once

// Module-internal (render/diligent/src/ only): TerrainRenderer's GPU-side state, shared between
// pso_terrain.cpp (pipeline + constant-buffer creation) and terrain_renderer.cpp (upload/draw).

#include <cstdint>

#include "render/diligent/terrain_renderer.hpp"

#include "world/chunk/coord_containers.hpp"

#include "detail/render_context_impl.hpp"

#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"

namespace render::diligent::detail {

// CPU mirrors of the cbuffers in shaders/terrain.vsh.hlsl / terrain.psh.hlsl. Both sides declare
// the matrix column_major explicitly, so the raw GLM (column-major) bytes upload verbatim -- no
// transpose anywhere.
struct FrameConstantsCpu {
    glm::mat4 viewProj; // matches: cbuffer FrameConstants { column_major float4x4 g_ViewProj; }
};
static_assert(sizeof(FrameConstantsCpu) == 64, "must match the 64-byte HLSL cbuffer exactly");

struct ChunkConstantsCpu {
    glm::vec4 chunkOriginWorld; // matches: cbuffer ChunkConstants { float4 g_ChunkOriginWorld; } -- xyz used, w padding
};
static_assert(sizeof(ChunkConstantsCpu) == 16, "must match the 16-byte HLSL cbuffer exactly");

// The compressed GPU vertex (engine-hardening brief Group K): 12 bytes vs the CPU-side
// world::meshing::Vertex's 28 -- 2.33x. Decoded entirely by fixed-function normalized vertex
// fetch + arithmetic in the VS (no shader bit manipulation; Subagent 3's cross-backend finding).
//   position: 4x uint16 UNORM, chunk-local fixed point at 1/1024 voxel (vertex_quantization.hpp).
//             FOUR components, not three, is load-bearing: DXGI has no R16G16B16_UNORM at all, so
//             a 3-component VT_UINT16 layout asserts in Diligent's D3D12 format mapping
//             ("Unsupported number of components") while working fine on Vulkan -- found by the
//             per-backend --verify-frame check, exactly the silent-divergence class Group K's
//             design review warned about. pw is zero padding the GPU ignores.
//   normal:   2x uint8 UNORM, 16-bit octahedral (octahedral.hpp; iTwin.js-width precedent)
//   material: uint8, plain integer attribute (unchanged semantics from the uncompressed layout)
//   ao:       uint8 UNORM, baked per-vertex concavity AO (research/baked-ao-design.md) -- this
//             deliberately consumed the former pad byte, so the stride is STILL 12; the layout
//             static_asserts in pso_terrain.cpp were extended for it, not loosened.
struct GpuVertexCompressed {
    std::uint16_t px = 0;
    std::uint16_t py = 0;
    std::uint16_t pz = 0;
    std::uint16_t pw = 0; // unused; exists so position is a DXGI-representable RGBA16 attribute
    std::uint8_t octU = 0;
    std::uint8_t octV = 0;
    std::uint8_t material = 0;
    std::uint8_t ao = 255; // UNORM: 255 = fully open
};
static_assert(sizeof(GpuVertexCompressed) == 12, "GPU input layout in pso_terrain.cpp assumes stride 12");

// One uploaded chunk's GPU-side mesh (task 13). gpuBytes is remembered so the allocation
// tracker's decrement on removal never has to touch a possibly-dying Diligent object.
struct ChunkGpuMesh {
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer;
    std::uint32_t indexCount = 0;
    std::uint64_t gpuBytes = 0;
    glm::vec3 aabbMin{0.0f}; // world-space, tight bounds from the actual uploaded vertices
    glm::vec3 aabbMax{0.0f};
};

} // namespace render::diligent::detail

namespace render::diligent {

struct TerrainRenderer::Impl {
    RenderContext* context = nullptr; // non-owning; TerrainRenderer's contract requires it outlive us
    GpuAllocationTracker tracker;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> frameConstants; // dynamic, mapped once per frame
    Diligent::RefCntAutoPtr<Diligent::IBuffer> chunkConstants; // dynamic, mapped once per visible chunk draw
    Diligent::RefCntAutoPtr<Diligent::IBuffer> materialPalette; // immutable
    // CoordMap (flat) matters most HERE of anywhere: this map is iterated every frame for the
    // draw loop, and flat contiguous storage is the iteration-friendly layout.
    world::chunk::CoordMap<detail::ChunkGpuMesh> chunks;
    std::size_t lastVisible = 0;
};

// Implemented in pso_terrain.cpp: shaders (from VOXEL_TERRAIN_SHADER_DIR), input layout matching
// world::meshing::Vertex, the terrain PSO, the three constant buffers, and the SRB. Throws
// std::runtime_error on any creation failure.
void create_terrain_pipeline(TerrainRenderer::Impl& impl);

} // namespace render::diligent
