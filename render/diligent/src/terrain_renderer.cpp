#include <format>
#include <limits>
#include <optional>
#include <stdexcept>

#include "detail/terrain_renderer_impl.hpp"

#include "render/diligent/frustum.hpp"

#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
// GPU zones (task 28): the Tracy Vulkan context lives type-erased in RenderContext::Impl
// (created by attach_gpu_profiler); the command buffer is re-fetched from Diligent immediately
// before every use, per the IDeviceContextVk::GetVkCommandBuffer contract -- never cached.
#include <vulkan/vulkan.h>

#include "Graphics/GraphicsEngineVulkan/interface/DeviceContextVk.h"

#include <tracy/TracyVulkan.hpp>
#else
#define ZoneScoped
#define ZoneScopedN(name)
#endif

namespace render::diligent {

namespace {

using namespace Diligent;

constexpr float kChunkSizeF = static_cast<float>(world::chunk::kChunkSize);

} // namespace

TerrainRenderer::TerrainRenderer(RenderContext& context) : impl_(std::make_unique<Impl>()) {
    impl_->context = &context;
    create_terrain_pipeline(*impl_);
}

TerrainRenderer::~TerrainRenderer() = default;
TerrainRenderer::TerrainRenderer(TerrainRenderer&&) noexcept = default;
TerrainRenderer& TerrainRenderer::operator=(TerrainRenderer&&) noexcept = default;

void TerrainRenderer::upload_chunk_mesh(world::chunk::ChunkCoord coord, const world::meshing::MeshData& mesh) {
    ZoneScopedN("chunk upload");
    remove_chunk_mesh(coord);
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return;
    }

    IRenderDevice* device = impl_->context->impl().device;
    detail::ChunkGpuMesh gpu;

    const Uint64 vbSize = mesh.vertices.size() * sizeof(world::meshing::Vertex);
    const Uint64 ibSize = mesh.indices.size() * sizeof(std::uint32_t);

    // Named per PHASE_1_BRIEF.md §4 -- these are the strings RenderDoc/Nsight show instead of hex
    // handles. Diligent copies the name at creation, so the temporaries are fine.
    const std::string vbName = std::format("ChunkVB[{},{},{}]", coord.x, coord.y, coord.z);
    const std::string ibName = std::format("ChunkIB[{},{},{}]", coord.x, coord.y, coord.z);

    {
        BufferDesc desc;
        desc.Name = vbName.c_str();
        desc.Size = vbSize;
        desc.Usage = USAGE_IMMUTABLE; // meshes are replaced wholesale on re-mesh, never patched
        desc.BindFlags = BIND_VERTEX_BUFFER;
        BufferData initial{mesh.vertices.data(), vbSize};
        device->CreateBuffer(desc, &initial, &gpu.vertexBuffer);
    }
    {
        BufferDesc desc;
        desc.Name = ibName.c_str();
        desc.Size = ibSize;
        desc.Usage = USAGE_IMMUTABLE;
        desc.BindFlags = BIND_INDEX_BUFFER;
        BufferData initial{mesh.indices.data(), ibSize};
        device->CreateBuffer(desc, &initial, &gpu.indexBuffer);
    }
    if (!gpu.vertexBuffer || !gpu.indexBuffer) {
        throw std::runtime_error(std::format("chunk mesh buffer creation failed at [{},{},{}]", coord.x, coord.y,
                                             coord.z));
    }

    // Tight world-space AABB from the actual vertices (chunk-local positions span [-1, 32] -- the
    // -1 boundary-layer cells are part of this chunk's own mesh, so a plain [0,32] box would cull
    // visible boundary geometry at screen edges).
    glm::vec3 localMin{std::numeric_limits<float>::max()};
    glm::vec3 localMax{std::numeric_limits<float>::lowest()};
    for (const world::meshing::Vertex& v : mesh.vertices) {
        localMin = glm::min(localMin, v.position);
        localMax = glm::max(localMax, v.position);
    }
    const glm::vec3 origin{static_cast<float>(coord.x) * kChunkSizeF, static_cast<float>(coord.y) * kChunkSizeF,
                           static_cast<float>(coord.z) * kChunkSizeF};
    gpu.aabbMin = origin + localMin;
    gpu.aabbMax = origin + localMax;

    gpu.indexCount = static_cast<std::uint32_t>(mesh.indices.size());
    gpu.gpuBytes = vbSize + ibSize;
    impl_->tracker.on_allocate(gpu.gpuBytes); // task 13's allocation-tracking hook (§2.3)

    impl_->chunks.emplace(coord, std::move(gpu));
}

void TerrainRenderer::remove_chunk_mesh(world::chunk::ChunkCoord coord) {
    const auto it = impl_->chunks.find(coord);
    if (it == impl_->chunks.end()) {
        return;
    }
    impl_->tracker.on_free(it->second.gpuBytes);
    impl_->chunks.erase(it); // RefCntAutoPtr release is the GPU-side free
}

void TerrainRenderer::render(const render::interface::Camera& camera) {
    ZoneScopedN("terrain render");
    auto& rc = impl_->context->impl();
    IDeviceContext* ctx = rc.context;

#if defined(TRACY_ENABLE)
    // Fetched fresh for the whole pass; any Diligent call may submit and swap the underlying
    // command buffer, so scope the GPU zone/collect tightly around this render() body only.
    tracy::VkCtx* tracyCtx = static_cast<tracy::VkCtx*>(rc.tracyVkCtx);
    RefCntAutoPtr<IDeviceContextVk> ctxVk;
    if (tracyCtx != nullptr) {
        ctx->QueryInterface(IID_DeviceContextVk, reinterpret_cast<IObject**>(static_cast<IDeviceContextVk**>(&ctxVk)));
    }
#endif

    ITextureView* rtv = rc.swapchain->GetCurrentBackBufferRTV();
    ITextureView* dsv = rc.swapchain->GetDepthBufferDSV();
    ctx->SetRenderTargets(1, &rtv, dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->ClearRenderTarget(rtv, kClearColor.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->ClearDepthStencil(dsv, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (impl_->chunks.empty()) {
        impl_->lastVisible = 0;
        return;
    }

    const SwapChainDesc& scDesc = rc.swapchain->GetDesc();
    const float aspect =
        scDesc.Height > 0 ? static_cast<float>(scDesc.Width) / static_cast<float>(scDesc.Height) : 1.0f;
    const glm::mat4 viewProj = projection_matrix(camera, aspect) * view_matrix(camera);

    {
        MapHelper<detail::FrameConstantsCpu> frame(ctx, impl_->frameConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        frame->viewProj = viewProj;
    }

    ctx->SetPipelineState(impl_->pso);
    ctx->CommitShaderResources(impl_->srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const Frustum frustum = extract_frustum(viewProj);
    std::size_t visible = 0;
#if defined(TRACY_ENABLE)
    std::optional<tracy::VkCtxScope> gpuZone;
    if (tracyCtx != nullptr && ctxVk) {
        static constexpr tracy::SourceLocationData kDrawLoc{"terrain draw", TracyFunction, TracyFile, __LINE__, 0};
        gpuZone.emplace(tracyCtx, &kDrawLoc, ctxVk->GetVkCommandBuffer(), true);
    }
#endif
    for (const auto& [coord, mesh] : impl_->chunks) {
        if (!intersects(frustum, Aabb{mesh.aabbMin, mesh.aabbMax})) {
            continue; // task 14: skip the draw call entirely
        }
        ++visible;

        {
            MapHelper<detail::ChunkConstantsCpu> chunk(ctx, impl_->chunkConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            chunk->chunkOriginWorld = glm::vec4(static_cast<float>(coord.x) * kChunkSizeF,
                                                static_cast<float>(coord.y) * kChunkSizeF,
                                                static_cast<float>(coord.z) * kChunkSizeF, 0.0f);
        }

        IBuffer* vertexBuffers[] = {mesh.vertexBuffer.RawPtr()};
        const Uint64 offsets[] = {0};
        ctx->SetVertexBuffers(0, 1, vertexBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                              SET_VERTEX_BUFFERS_FLAG_RESET);
        ctx->SetIndexBuffer(mesh.indexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs draw;
        draw.NumIndices = mesh.indexCount;
        draw.IndexType = VT_UINT32;
        draw.Flags = DRAW_FLAG_VERIFY_ALL;
        ctx->DrawIndexed(draw);
    }
#if defined(TRACY_ENABLE)
    if (tracyCtx != nullptr && ctxVk) {
        gpuZone.reset(); // end the zone in the same command buffer state before collecting
        TracyVkCollect(tracyCtx, ctxVk->GetVkCommandBuffer());
    }
#endif
    impl_->lastVisible = visible;
}

std::size_t TerrainRenderer::chunk_count() const noexcept {
    return impl_->chunks.size();
}

std::size_t TerrainRenderer::last_visible_count() const noexcept {
    return impl_->lastVisible;
}

const GpuAllocationTracker& TerrainRenderer::gpu_memory() const noexcept {
    return impl_->tracker;
}

} // namespace render::diligent
