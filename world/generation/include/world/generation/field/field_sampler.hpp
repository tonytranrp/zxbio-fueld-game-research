#pragma once

// Prompt 006 goal 297: reading the baked field back, and the derivative everything else needs.
//
// WHY BICUBIC AND NOT BILINEAR. `height_at`'s SLOPE is load-bearing, not incidental: the material
// banding asks it (`grass_max_slope` decides grass vs rock), tree placement masks on it, and the
// collider's walkability limit is a slope test. Bilinear interpolation has a DISCONTINUOUS
// derivative at every cell boundary, so a bilinear field would put a grid-aligned seam through
// every one of those decisions -- a 16 m checkerboard of grass and rock, which is exactly the kind
// of artefact that gets diagnosed as a shading bug three passes later.
//
// Catmull-Rom is the choice: C¹ everywhere, interpolating (it passes through the samples, so the
// baked field is reproduced exactly at cell centres rather than smoothed), and its derivative is a
// closed form rather than a finite difference -- which is what makes `slope_at` exact instead of
// epsilon-dependent.
//
// AND `slope_at` EXISTS SO THE EPSILONS STOP DIVERGING. Several callers already finite-difference
// `height_at` with their own step -- `TerrainSampler` at 1 m, the collider at its own -- so "the
// slope" is currently three slightly different quantities depending on who asks. One function, one
// answer, and it is the analytic derivative of the same reconstruction the height came from.

#include <cmath>
#include <cstdint>

#include "engine/core/math.hpp"
#include "world/generation/field/terrain_field.hpp"

namespace world::generation::field {

/// Reads one plane of a baked field with a C¹ reconstruction.
///
/// Holds a reference, not a copy: the field is ~8 MB and a sampler is created per query site.
class FieldSampler {
public:
    FieldSampler(const TerrainField& field, Plane plane) noexcept
        : field_(&field), plane_(field.plane(plane)) {}

    /// The reconstructed value at a world position. Clamped at the field's edge rather than
    /// wrapped: the macro field is 15x wider than the playable region, so a query outside it is a
    /// tool or a test looking past the world's own edge, and a clamp is the answer that keeps
    /// `height_at` total.
    [[nodiscard]] float value_at(float x, float z) const noexcept {
        const glm::vec2 c = field_->geometry().to_cell(x, z);
        const auto cx = static_cast<std::int32_t>(std::floor(c.x));
        const auto cz = static_cast<std::int32_t>(std::floor(c.y));
        const float fx = c.x - static_cast<float>(cx);
        const float fz = c.y - static_cast<float>(cz);
        float rows[4];
        for (int j = 0; j < 4; ++j) {
            const std::int32_t sz = cz - 1 + j;
            rows[j] = catmull_rom(at(cx - 1, sz), at(cx, sz), at(cx + 1, sz), at(cx + 2, sz), fx);
        }
        return catmull_rom(rows[0], rows[1], rows[2], rows[3], fz);
    }

    /// The analytic gradient (d/dx, d/dz) in metres per metre -- the derivative of the SAME
    /// reconstruction `value_at` uses, so slope and height can never disagree about a surface.
    [[nodiscard]] glm::vec2 gradient_at(float x, float z) const noexcept {
        const glm::vec2 c = field_->geometry().to_cell(x, z);
        const auto cx = static_cast<std::int32_t>(std::floor(c.x));
        const auto cz = static_cast<std::int32_t>(std::floor(c.y));
        const float fx = c.x - static_cast<float>(cx);
        const float fz = c.y - static_cast<float>(cz);
        float rows[4];
        float dRows[4];
        for (int j = 0; j < 4; ++j) {
            const std::int32_t sz = cz - 1 + j;
            const float p0 = at(cx - 1, sz);
            const float p1 = at(cx, sz);
            const float p2 = at(cx + 1, sz);
            const float p3 = at(cx + 2, sz);
            rows[j] = catmull_rom(p0, p1, p2, p3, fx);
            dRows[j] = catmull_rom_derivative(p0, p1, p2, p3, fx);
        }
        const float inv = 1.0f / field_->geometry().cell_size;
        return glm::vec2{catmull_rom(dRows[0], dRows[1], dRows[2], dRows[3], fz) * inv,
                         catmull_rom_derivative(rows[0], rows[1], rows[2], rows[3], fz) * inv};
    }

private:
    [[nodiscard]] float at(std::int32_t cx, std::int32_t cz) const noexcept {
        const std::int32_t n = field_->geometry().cells;
        const std::int32_t qx = cx < 0 ? 0 : (cx >= n ? n - 1 : cx);
        const std::int32_t qz = cz < 0 ? 0 : (cz >= n ? n - 1 : cz);
        return plane_[field_->index(qx, qz)];
    }

    /// Catmull-Rom through p1 and p2 (tension 0.5). t in [0,1] between p1 and p2.
    [[nodiscard]] static constexpr float catmull_rom(float p0, float p1, float p2, float p3,
                                                     float t) noexcept {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                       (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    }

    /// d/dt of the above -- in units of "per cell", which the caller scales by 1/cell_size.
    [[nodiscard]] static constexpr float catmull_rom_derivative(float p0, float p1, float p2, float p3,
                                                                float t) noexcept {
        const float t2 = t * t;
        return 0.5f * ((-p0 + p2) + 2.0f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t +
                       3.0f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t2);
    }

    const TerrainField* field_;
    std::span<const float> plane_;
};

} // namespace world::generation::field
