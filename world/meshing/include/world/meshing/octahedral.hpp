#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "engine/core/math.hpp"

namespace world::meshing {

// Octahedral unit-vector encoding (Cigolle et al., "A Survey of Efficient Representations for
// Independent Unit Vectors", JCGT 2014; the branchless fold is Narkowicz's widely-used variant).
// Production precedent for the 16-bit total width: Bentley iTwin.js ships normals as a 16-bit
// OctEncodedNormal; Godot/Unity/Blender ship the same family of encodings (ENGINE_HARDENING_
// BRIEF.md §6). CPU side of Group K task 25 -- the HLSL decode mirror lands with task 26 and the
// two must stay bit-compatible (same UNORM rounding, same fold).
//
// Encoding: project the unit sphere onto the octahedron |x|+|y|+|z|=1, unfold the lower
// hemisphere into the outer corners of the [-1,1]^2 square, then quantize each component to
// 8-bit UNORM (u = round((c*0.5+0.5)*255)). 2 bytes total vs 12 for float3 -- the 6x field
// reduction §6's arithmetic banks on.

struct OctNormal16 {
    std::uint8_t u = 0;
    std::uint8_t v = 0;
    friend bool operator==(const OctNormal16&, const OctNormal16&) = default;
};

// n must be non-zero; it is normalized here so callers can pass raw area-weighted sums.
[[nodiscard]] inline OctNormal16 encode_octahedral_16(glm::vec3 n) noexcept {
    n = glm::normalize(n);
    const float invL1 = 1.0f / (std::abs(n.x) + std::abs(n.y) + std::abs(n.z));
    float ox = n.x * invL1;
    float oy = n.y * invL1;
    if (n.z < 0.0f) {
        // Fold the lower hemisphere outward: (1-|y|)*sign(x), (1-|x|)*sign(y).
        const float fx = (1.0f - std::abs(oy)) * (ox >= 0.0f ? 1.0f : -1.0f);
        const float fy = (1.0f - std::abs(ox)) * (oy >= 0.0f ? 1.0f : -1.0f);
        ox = fx;
        oy = fy;
    }
    const auto quantize = [](float c) noexcept -> std::uint8_t {
        const float unorm = std::clamp(c * 0.5f + 0.5f, 0.0f, 1.0f);
        return static_cast<std::uint8_t>(std::lround(unorm * 255.0f));
    };
    return OctNormal16{quantize(ox), quantize(oy)};
}

// Exact mirror of the HLSL decode (task 26): UNORM byte -> [-1,1], refold, normalize.
[[nodiscard]] inline glm::vec3 decode_octahedral_16(OctNormal16 packed) noexcept {
    const float ox = static_cast<float>(packed.u) * (1.0f / 255.0f) * 2.0f - 1.0f;
    const float oy = static_cast<float>(packed.v) * (1.0f / 255.0f) * 2.0f - 1.0f;
    glm::vec3 n{ox, oy, 1.0f - std::abs(ox) - std::abs(oy)};
    if (n.z < 0.0f) {
        const float fx = (1.0f - std::abs(oy)) * (ox >= 0.0f ? 1.0f : -1.0f);
        const float fy = (1.0f - std::abs(ox)) * (oy >= 0.0f ? 1.0f : -1.0f);
        n.x = fx;
        n.y = fy;
    }
    return glm::normalize(n);
}

} // namespace world::meshing
