#pragma once

// Module-internal (render/diligent/src/ only): TerrainRenderer's GPU-side state, shared between
// pso_terrain.cpp (pipeline + constant-buffer creation) and terrain_renderer.cpp (upload/draw).

#include <cstdint>
#include <unordered_map>

#include "render/diligent/terrain_renderer.hpp"

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
    std::unordered_map<world::chunk::ChunkCoord, detail::ChunkGpuMesh> chunks;
    std::size_t lastVisible = 0;
};

// Implemented in pso_terrain.cpp: shaders (from VOXEL_TERRAIN_SHADER_DIR), input layout matching
// world::meshing::Vertex, the terrain PSO, the three constant buffers, and the SRB. Throws
// std::runtime_error on any creation failure.
void create_terrain_pipeline(TerrainRenderer::Impl& impl);

} // namespace render::diligent
