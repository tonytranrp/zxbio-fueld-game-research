#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace world::meshing {

// Chunk-local position quantization for the compressed GPU vertex (ENGINE_HARDENING_BRIEF.md
// Group K task 24; full derivation in research/engine-hardening-log.md). Positions span
// [-1, 32] per axis (32-cell chunk + 1-cell boundary layer). Quantized to 16-bit fixed point at
// a DYADIC step of 1/1024 voxel: q = round((p + 1) * 1024), so q_max = 33 * 1024 = 33792 with
// headroom in uint16. The dyadic step is the load-bearing choice, not a rounding of "1/1986":
// a 32-voxel chunk offset is then EXACTLY 32768 steps, so two neighboring chunks quantizing the
// same shared-boundary vertex land on the same world lattice (any residual disagreement is
// bounded by the one step their input floats already differed by -- ~0.001 voxel, far below
// visibility and below Surface Nets' own placement error).
//
// Decoded GPU-side by fixed-function UNORM vertex fetch + one mad (no shader bit manipulation --
// the whole point, given the cross-backend intrinsic risks Subagent 3 documented):
//   p = unorm * (65535.0 / 1024.0) - 1.0     // 65535/1024 is exactly representable in float32
inline constexpr float kPositionQuantScale = 1024.0f;
inline constexpr float kPositionQuantBias = 1.0f; // shifts [-1, ...] to [0, ...] before scaling

[[nodiscard]] inline std::uint16_t quantize_position_16(float p) noexcept {
    const float scaled = (p + kPositionQuantBias) * kPositionQuantScale;
    const long q = std::lround(std::clamp(scaled, 0.0f, 65535.0f));
    return static_cast<std::uint16_t>(q);
}

[[nodiscard]] inline float dequantize_position_16(std::uint16_t q) noexcept {
    // Mirrors the GPU exactly: UNORM decode (q/65535) then the shader's mad.
    return static_cast<float>(q) * (1.0f / 65535.0f) * (65535.0f / kPositionQuantScale) - kPositionQuantBias;
}

} // namespace world::meshing
