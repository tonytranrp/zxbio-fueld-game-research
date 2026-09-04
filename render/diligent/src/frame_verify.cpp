#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "render/diligent/frame_verify.hpp"

#include "detail/render_context_impl.hpp"

#include "Graphics/GraphicsEngine/interface/Texture.h"

namespace render::diligent {

float sample_non_reference_pixel_fraction(RenderContext& context) {
    using namespace Diligent;
    auto& rc = context.impl();

    ITexture* backBuffer = rc.swapchain->GetCurrentBackBufferRTV()->GetTexture();
    const TextureDesc& bbDesc = backBuffer->GetDesc();

    TextureDesc stagingDesc;
    stagingDesc.Name = "FrameVerify staging";
    stagingDesc.Type = RESOURCE_DIM_TEX_2D;
    stagingDesc.Width = bbDesc.Width;
    stagingDesc.Height = bbDesc.Height;
    stagingDesc.Format = bbDesc.Format;
    stagingDesc.MipLevels = 1;
    stagingDesc.Usage = USAGE_STAGING;
    stagingDesc.BindFlags = BIND_NONE;
    stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;

    RefCntAutoPtr<ITexture> staging;
    rc.device->CreateTexture(stagingDesc, nullptr, &staging);
    if (!staging) {
        throw std::runtime_error("frame verification: staging texture creation failed");
    }

    CopyTextureAttribs copy;
    copy.pSrcTexture = backBuffer;
    copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    copy.pDstTexture = staging;
    copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    rc.context->CopyTexture(copy);
    rc.context->WaitForIdle(); // flushes and blocks until the copy lands -- fine for a one-shot check

    // DO_NOT_WAIT is required, not an optimization: Diligent's Vulkan backend warns that mapping
    // a staging texture for read never GPU-waits by itself -- the WaitForIdle above is the
    // synchronization, and this flag states that explicitly.
    MappedTextureSubresource mapped;
    rc.context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
    if (mapped.pData == nullptr) {
        throw std::runtime_error("frame verification: staging map failed");
    }

    // All supported swap-chain color formats here are 4 bytes/pixel (RGBA8/BGRA8 variants);
    // rows may be padded, hence the per-row stride walk.
    const auto* base = static_cast<const std::uint8_t*>(mapped.pData);
    std::uint32_t reference = 0;
    std::memcpy(&reference, base, sizeof(reference));

    std::uint64_t differing = 0;
    for (std::uint32_t y = 0; y < bbDesc.Height; ++y) {
        const std::uint8_t* row = base + static_cast<std::size_t>(y) * mapped.Stride;
        for (std::uint32_t x = 0; x < bbDesc.Width; ++x) {
            std::uint32_t pixel = 0;
            std::memcpy(&pixel, row + static_cast<std::size_t>(x) * 4u, sizeof(pixel));
            differing += pixel != reference ? 1u : 0u;
        }
    }

    // TERRAIN_FIXES_BRIEF Group Q task 5: the fraction alone cannot distinguish "more ribbon"
    // from "actually continuous terrain" -- when VOXEL_DUMP_FRAME names a file, write the frame
    // as a binary PPM for actual visual review. Channel order is written as-is (RGBA vs BGRA is
    // irrelevant to judging continuity).
#if defined(_MSC_VER)
    char dumpPathBuffer[512] = {};
    std::size_t dumpPathLen = 0;
    const char* dumpPath =
        getenv_s(&dumpPathLen, dumpPathBuffer, sizeof(dumpPathBuffer), "VOXEL_DUMP_FRAME") == 0 && dumpPathLen > 0
            ? dumpPathBuffer
            : nullptr; // getenv itself is C4996 under /W4 /WX
#else
    const char* dumpPath = std::getenv("VOXEL_DUMP_FRAME");
#endif
    if (dumpPath != nullptr) {
        std::FILE* ppm = nullptr;
#if defined(_MSC_VER)
        (void)fopen_s(&ppm, dumpPath, "wb");
#else
        ppm = std::fopen(dumpPath, "wb");
#endif
        if (ppm != nullptr) {
            std::fprintf(ppm, "P6\n%u %u\n255\n", bbDesc.Width, bbDesc.Height);
            for (std::uint32_t y = 0; y < bbDesc.Height; ++y) {
                const std::uint8_t* row = base + static_cast<std::size_t>(y) * mapped.Stride;
                for (std::uint32_t x = 0; x < bbDesc.Width; ++x) {
                    const std::uint8_t* px = row + static_cast<std::size_t>(x) * 4u;
                    // This machine's swapchains are RGBA-order (checked against the known clear
                    // color); a BGRA swapchain would swap red/blue -- cosmetic for this tool.
                    const std::uint8_t rgb[3] = {px[0], px[1], px[2]};
                    std::fwrite(rgb, 1, 3, ppm);
                }
            }
            std::fclose(ppm);
        }
    }
    rc.context->UnmapTextureSubresource(staging, 0, 0);

    const std::uint64_t total = static_cast<std::uint64_t>(bbDesc.Width) * bbDesc.Height;
    return total > 0 ? static_cast<float>(static_cast<double>(differing) / static_cast<double>(total)) : 0.0f;
}

} // namespace render::diligent
