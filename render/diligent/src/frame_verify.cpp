#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "render/diligent/frame_verify.hpp"

#include "detail/render_context_impl.hpp"

#include "Common/interface/DataBlobImpl.hpp"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "TextureLoader/interface/PNGCodec.h"

namespace render::diligent {

namespace {

// Shared back-buffer readback for the verifier and the frame dumper: staging copy + full GPU sync
// (WaitForIdle) + map. One-shot debug cost by design -- never on a hot path.
struct BackbufferReadback {
    Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
    Diligent::MappedTextureSubresource mapped{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    RenderContext::Impl* rc = nullptr;

    ~BackbufferReadback() {
        if (rc != nullptr && staging) {
            rc->context->UnmapTextureSubresource(staging, 0, 0);
        }
    }
};

void read_back_buffer(RenderContext::Impl& rc, BackbufferReadback& out) {
    using namespace Diligent;

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

    rc.device->CreateTexture(stagingDesc, nullptr, &out.staging);
    if (!out.staging) {
        throw std::runtime_error("frame readback: staging texture creation failed");
    }

    CopyTextureAttribs copy;
    copy.pSrcTexture = backBuffer;
    copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    copy.pDstTexture = out.staging;
    copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    rc.context->CopyTexture(copy);
    rc.context->WaitForIdle(); // flushes and blocks until the copy lands -- fine for a one-shot check

    // DO_NOT_WAIT is required, not an optimization: Diligent's Vulkan backend warns that mapping
    // a staging texture for read never GPU-waits by itself -- the WaitForIdle above is the
    // synchronization, and this flag states that explicitly.
    rc.context->MapTextureSubresource(out.staging, 0, 0, Diligent::MAP_READ,
                                      Diligent::MAP_FLAG_DO_NOT_WAIT, nullptr, out.mapped);
    if (out.mapped.pData == nullptr) {
        out.staging.Release(); // nothing to unmap
        throw std::runtime_error("frame readback: staging map failed");
    }
    out.width = bbDesc.Width;
    out.height = bbDesc.Height;
    out.rc = &rc;
}

// Repacks the padded 4-byte-per-pixel readback rows into tightly-packed RGB. All supported
// swap-chain color formats here are 4 bytes/pixel (RGBA8/BGRA8 variants). This machine's
// swapchains are RGBA-order (checked against the known clear color); a BGRA swapchain would swap
// red/blue -- cosmetic for a debug capture.
std::vector<std::uint8_t> repack_rgb(const BackbufferReadback& frame) {
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(frame.width) * frame.height * 3u);
    const auto* base = static_cast<const std::uint8_t*>(frame.mapped.pData);
    for (std::uint32_t y = 0; y < frame.height; ++y) {
        const std::uint8_t* row = base + static_cast<std::size_t>(y) * frame.mapped.Stride;
        std::uint8_t* dst = rgb.data() + static_cast<std::size_t>(y) * frame.width * 3u;
        for (std::uint32_t x = 0; x < frame.width; ++x) {
            const std::uint8_t* px = row + static_cast<std::size_t>(x) * 4u;
            dst[static_cast<std::size_t>(x) * 3u + 0] = px[0];
            dst[static_cast<std::size_t>(x) * 3u + 1] = px[1];
            dst[static_cast<std::size_t>(x) * 3u + 2] = px[2];
        }
    }
    return rgb;
}

bool write_file(const char* path, const void* data, std::size_t size, const char* header = nullptr) {
    std::FILE* f = nullptr;
#if defined(_MSC_VER)
    (void)fopen_s(&f, path, "wb"); // fopen itself is C4996 under /W4 /WX
#else
    f = std::fopen(path, "wb");
#endif
    if (f == nullptr) {
        return false;
    }
    if (header != nullptr) {
        std::fputs(header, f);
    }
    const bool ok = std::fwrite(data, 1, size, f) == size;
    std::fclose(f);
    return ok;
}

bool ends_with_ppm(const char* path) {
    const std::size_t len = std::strlen(path);
    return len >= 4 && std::strcmp(path + len - 4, ".ppm") == 0;
}

bool write_capture(const BackbufferReadback& frame, const char* path) {
    const std::vector<std::uint8_t> rgb = repack_rgb(frame);
    if (ends_with_ppm(path)) {
        char header[64];
        std::snprintf(header, sizeof(header), "P6\n%u %u\n255\n", frame.width, frame.height);
        return write_file(path, rgb.data(), rgb.size(), header);
    }
    // PNG via DiligentTools' bundled libpng (goals.md goal 6's "smallest reasonable option": the
    // encoder is already in the dependency tree -- no new dependency at all). The bare `2` is
    // libpng's PNG_COLOR_TYPE_RGB; png.h itself is deliberately not on this module's include path.
    auto pngBits = Diligent::DataBlobImpl::Create();
    const auto encoded = Diligent::EncodePng(rgb.data(), frame.width, frame.height,
                                             frame.width * 3u, /*PNG_COLOR_TYPE_RGB*/ 2,
                                             pngBits.RawPtr());
    if (encoded != Diligent::ENCODE_PNG_RESULT_OK) {
        return false;
    }
    return write_file(path, pngBits->GetConstDataPtr(), pngBits->GetSize());
}

} // namespace

float sample_non_reference_pixel_fraction(RenderContext& context) {
    auto& rc = context.impl();
    BackbufferReadback frame;
    read_back_buffer(rc, frame);

    const auto* base = static_cast<const std::uint8_t*>(frame.mapped.pData);

    // LOCAL-CONTRAST metric (third iteration, and the durable one). History: the original
    // bytewise compare-to-top-left died when Stage 2's bloom made 97.7% of sky pixels "differ";
    // a tolerance band fixed that, then Group L's analytic gradient sky (a 100+/255 vertical
    // sweep) broke any single-reference-pixel scheme for good. What actually distinguishes
    // "terrain visible" from "sky only" is spatial frequency: terrain carries terracing, AO,
    // albedo mottle, trees, and shorelines (high local contrast), while sky/fog/bloom are smooth
    // by construction -- an analytic gradient's neighbor-to-neighbor delta is < 1/255. A pixel
    // counts when it differs from its LEFT or UP neighbor by >4 on any channel. Calibrated
    // against a real frame pair at the standard verify pose: terrain-visible measures 12.3%, a
    // forced-empty frame (VOXEL_ONLY_CHUNK_Y=99) 0.9% (that floor is the overlay panel's own
    // text) -- 13.7x separation; the caller's 6% threshold sits between. A ribbon-class
    // regression (thin sliver bands) lands near the empty floor, far under the bar.
    constexpr int kChannelTolerance = 4;
    std::uint64_t differing = 0;
    for (std::uint32_t y = 1; y < frame.height; ++y) {
        const std::uint8_t* row = base + static_cast<std::size_t>(y) * frame.mapped.Stride;
        const std::uint8_t* rowUp = base + static_cast<std::size_t>(y - 1) * frame.mapped.Stride;
        for (std::uint32_t x = 1; x < frame.width; ++x) {
            const std::uint8_t* px = row + static_cast<std::size_t>(x) * 4u;
            const std::uint8_t* pxLeft = row + static_cast<std::size_t>(x - 1) * 4u;
            const std::uint8_t* pxUp = rowUp + static_cast<std::size_t>(x) * 4u;
            bool differs = false;
            for (int c = 0; c < 3 && !differs; ++c) {
                differs = std::abs(int(px[c]) - int(pxLeft[c])) > kChannelTolerance ||
                          std::abs(int(px[c]) - int(pxUp[c])) > kChannelTolerance;
            }
            differing += differs ? 1u : 0u;
        }
    }

    // Group Q's lesson, kept: the fraction alone cannot distinguish "more ribbon" from "actually
    // continuous terrain" -- when VOXEL_DUMP_FRAME names a file, write the frame for actual visual
    // review (PNG by default now, .ppm still honored).
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
        (void)write_capture(frame, dumpPath); // best-effort: the verify fraction is the contract here
    }

    const std::uint64_t total = static_cast<std::uint64_t>(frame.width) * frame.height;
    return total > 0 ? static_cast<float>(static_cast<double>(differing) / static_cast<double>(total)) : 0.0f;
}

bool dump_frame(RenderContext& context, const char* path) {
    auto& rc = context.impl();
    BackbufferReadback frame;
    read_back_buffer(rc, frame);
    return write_capture(frame, path);
}

} // namespace render::diligent
