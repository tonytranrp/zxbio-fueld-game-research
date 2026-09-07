#pragma once

#include <cstdint>
#include <vector>

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

// Writes the current back buffer to `path` (goals.md Group B): PNG via DiligentTools' bundled
// libpng encoder unless the path ends in ".ppm" (the original uncompressed format, kept for
// zero-dependency debugging). Same staging-copy + WaitForIdle cost profile as the sampler above --
// a debug/verification tool, never a per-frame call in shipping paths (the --dump-every and
// screenshot-hotkey callers are explicitly debug workflows). Throws on staging/copy failure;
// returns false only if the file itself could not be written or encoded.
bool dump_frame(RenderContext& context, const char* path);

// Prompt 002 goal 217. One tightly-packed RGB frame, in memory. The golden-image comparison needs
// to READ a PNG as well as write one, and the decoder is already in the dependency tree
// (DiligentTools' bundled libpng, the same one dump_frame encodes with) -- so it is exposed here
// rather than pulled into dev/harness, which would then need libpng on its include path for no
// other reason.
struct FrameImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgb; // 3 bytes per pixel, rows top to bottom

    [[nodiscard]] bool empty() const noexcept { return rgb.empty(); }
};

// Same staging-copy + WaitForIdle cost as dump_frame; a debug/verification call, never per frame.
[[nodiscard]] FrameImage read_back_frame(RenderContext& context);
// Returns an empty image if the file is missing or is not an 8-bit RGB/RGBA PNG.
[[nodiscard]] FrameImage decode_png_file(const char* path);
[[nodiscard]] bool encode_png_file(const char* path, const FrameImage& image);

} // namespace render::diligent
