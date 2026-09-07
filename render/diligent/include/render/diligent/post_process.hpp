#pragma once

#include <cstdint>
#include <memory>

#include "render/diligent/render_context.hpp"

namespace render::diligent {

// Post-process chain (docs/goals.md Group D): scene renders into an offscreen HDR color target
// (registered on RenderContext's impl, where TerrainRenderer picks it up automatically), then
// DiligentFX's Bloom runs over it and a composite pass tone-maps the result to the swap chain.
//
// Deliberately wired WITHOUT PostFXContext::Execute: Bloom only consumes the context's frame
// desc / device features / compile flags / transition alpha (verified by reading Bloom.cpp at the
// pinned commit), so the depth + motion-vector inputs PostFXContext::Execute demands are never
// needed -- exactly the "no G-buffer prematurely" boundary goal 22 draws. SSAO (Group F) is what
// would change this, and it is explicitly gated on goal 41's written go/no-go.
//
// Same PIMPL discipline as the rest of render/diligent: no Diligent (or DiligentFX) types here.
class PostProcessor {
public:
    // Creates the offscreen target lazily on the first execute() (it needs live swap-chain
    // dimensions, which resize anyway).
    explicit PostProcessor(RenderContext& context);
    ~PostProcessor();

    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;

    // Per-pass kill switches (goal 52): isolate a regression to one pass without reverting code.
    void set_bloom_enabled(bool enabled) noexcept;
    void set_tonemap_enabled(bool enabled) noexcept;
    // Goal 237: the auto-exposure multiplier, applied BEFORE the tone curve. 1.0 disables it.
    void set_exposure(float exposure) noexcept;
    // Goal 238: the adapted log2 luminance from goal 237's metering. Bloom behaves like VEILING
    // LUMINANCE rather than like a fixed threshold -- what blooms is what is bright RELATIVE TO
    // WHAT THE EYE IS ADAPTED TO, and the kernel widens as adaptation drops (a dark-adapted pupil
    // is dilated and its point-spread function is wider), so a torch in a cave blooms like the sun
    // outdoors. Pass NaN, or never call this, to keep the fixed-threshold behaviour.
    void set_adaptation_log2(float adaptedLog2) noexcept;

    // Runs bloom + tonemap-composite for the frame the renderer just drew into the scene target.
    // Call after TerrainRenderer::render and before overlay/present.
    void execute(std::uint32_t frameIndex);

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace render::diligent
