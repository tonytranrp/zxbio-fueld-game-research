#pragma once

#include "render/diligent/render_context.hpp"

namespace render::diligent {

// M1.4 smoke-verification hook, not a rendering feature: reads the current back buffer back to
// the CPU (staging copy + full GPU sync -- call it once, never per frame) and returns the
// fraction [0,1] of pixels that differ bytewise from the top-left pixel. With the spectator
// camera tilted down at terrain, the top-left pixel is clear-color sky, so a zero return means
// "nothing but the clear color rendered" -- the failure this exists to catch mechanically instead
// of by a human squinting at a window ("terrain actually visible", Phase 1 brief §8 M1.4).
// Bytewise-vs-reference deliberately sidesteps sRGB encoding and channel-order differences
// between backends.
[[nodiscard]] float sample_non_reference_pixel_fraction(RenderContext& context);

} // namespace render::diligent
