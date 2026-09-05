#include "world/svo/height_field.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace world::svo {

namespace {

// Per-cell margin: a fraction of the cell's OWN corner range plus an absolute floor. Local, not
// global, on purpose -- a first version used the largest adjacent-sample delta anywhere in the
// field (3.6 m on this terrain's cliffs), which made every flat meadow's boxes "Mixed" through a
// 7 m band and had the builder sampling ~8x more bricks than it kept. The cell's own range is a
// sound local bound for a function whose finest feature (this noise's 4th octave, ~25 m period)
// is far larger than the cell: within a 0.5 m cell the surface is nearly linear, so interior
// values stay inside the corner range up to curvature, which the factor + floor cover. VERIFIED
// by test_height_field.cpp against dense re-sampling, not assumed.
constexpr float kMarginFactor = 0.5f;
constexpr float kMarginFloorPerMeterOfCell = 0.2f; // 0.10 m at the 0.5 m coarse cell, 0.0125 m at 1/16 m

} // namespace

HeightField::HeightField(const world::generation::HeightmapGenerator& heightmap, float xMin, float zMin,
                         float extent, float cellSize)
    : xMin_(xMin), zMin_(zMin), cellSize_(cellSize) {
    cells_ = std::max<std::int32_t>(1, static_cast<std::int32_t>(std::ceil(extent / cellSize)));
    const std::int32_t corners = cells_ + 1;
    std::vector<float> h(static_cast<std::size_t>(corners) * static_cast<std::size_t>(corners));
    heightmap.generate_column_heights_spaced(xMin, zMin, corners, corners, cellSize, h.data());
    const auto at = [&](std::int32_t i, std::int32_t j) {
        return h[static_cast<std::size_t>(j) * static_cast<std::size_t>(corners) +
                 static_cast<std::size_t>(i)];
    };

    Level base;
    base.size = cells_;
    base.min.resize(static_cast<std::size_t>(cells_) * static_cast<std::size_t>(cells_));
    base.max.resize(base.min.size());
    float maxMargin = 0.0f;
    for (std::int32_t j = 0; j < cells_; ++j) {
        for (std::int32_t i = 0; i < cells_; ++i) {
            const float a = at(i, j);
            const float b = at(i + 1, j);
            const float c = at(i, j + 1);
            const float d = at(i + 1, j + 1);
            const std::size_t idx =
                static_cast<std::size_t>(j) * static_cast<std::size_t>(cells_) + static_cast<std::size_t>(i);
            const float lo = std::min({a, b, c, d});
            const float hi = std::max({a, b, c, d});
            const float margin = (hi - lo) * kMarginFactor + kMarginFloorPerMeterOfCell * cellSize;
            base.min[idx] = lo - margin;
            base.max[idx] = hi + margin;
            maxMargin = std::max(maxMargin, margin);
        }
    }
    margin_ = maxMargin;
    levels_.push_back(std::move(base));

    // Per-corner 1 m-baseline slope: +-k corners where k*cellSize == 1 m (rounded); clamped at
    // the edges so the outermost meter is approximate rather than undefined.
    const std::int32_t k = std::max<std::int32_t>(1, static_cast<std::int32_t>(std::lround(1.0f / cellSize)));
    slopes_.resize(h.size());
    for (std::int32_t j = 0; j < corners; ++j) {
        for (std::int32_t i = 0; i < corners; ++i) {
            const std::int32_t ip = std::min(i + k, corners - 1);
            const std::int32_t im = std::max(i - k, 0);
            const std::int32_t jp = std::min(j + k, corners - 1);
            const std::int32_t jm = std::max(j - k, 0);
            const float sx = std::abs(at(ip, j) - at(im, j)) * 0.5f;
            const float sz = std::abs(at(i, jp) - at(i, jm)) * 0.5f;
            slopes_[static_cast<std::size_t>(j) * static_cast<std::size_t>(corners) +
                    static_cast<std::size_t>(i)] = std::max(sx, sz);
        }
    }

    while (levels_.back().size > 1) {
        const Level& fine = levels_.back();
        Level coarse;
        coarse.size = (fine.size + 1) / 2;
        coarse.min.assign(static_cast<std::size_t>(coarse.size) * static_cast<std::size_t>(coarse.size),
                          std::numeric_limits<float>::max());
        coarse.max.assign(coarse.min.size(), std::numeric_limits<float>::lowest());
        for (std::int32_t j = 0; j < fine.size; ++j) {
            for (std::int32_t i = 0; i < fine.size; ++i) {
                const std::size_t fi = static_cast<std::size_t>(j) * static_cast<std::size_t>(fine.size) +
                                       static_cast<std::size_t>(i);
                const std::size_t ci =
                    static_cast<std::size_t>(j / 2) * static_cast<std::size_t>(coarse.size) +
                    static_cast<std::size_t>(i / 2);
                coarse.min[ci] = std::min(coarse.min[ci], fine.min[fi]);
                coarse.max[ci] = std::max(coarse.max[ci], fine.max[fi]);
            }
        }
        levels_.push_back(std::move(coarse));
    }
}

HeightField::Range HeightField::range(float x0, float z0, float x1, float z1) const noexcept {
    const auto toCell = [&](float v, float origin) {
        return static_cast<std::int32_t>(std::floor((v - origin) / cellSize_));
    };
    // Inclusive base-cell index range covering the footprint, clamped to the field.
    const std::int32_t i0 = std::clamp(toCell(x0, xMin_), 0, cells_ - 1);
    const std::int32_t i1 = std::clamp(toCell(x1, xMin_), 0, cells_ - 1);
    const std::int32_t j0 = std::clamp(toCell(z0, zMin_), 0, cells_ - 1);
    const std::int32_t j1 = std::clamp(toCell(z1, zMin_), 0, cells_ - 1);

    // Coarsest level whose cells are no larger than half the span keeps the visited count <= 25.
    const std::int32_t span = std::max(i1 - i0 + 1, j1 - j0 + 1);
    int level = std::max(0, static_cast<int>(std::bit_width(static_cast<std::uint32_t>(span))) - 2);
    level = std::min<int>(level, static_cast<int>(levels_.size()) - 1);
    const Level& lv = levels_[static_cast<std::size_t>(level)];
    const std::int32_t I0 = i0 >> level;
    const std::int32_t I1 = i1 >> level;
    const std::int32_t J0 = j0 >> level;
    const std::int32_t J1 = j1 >> level;

    Range r{std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()};
    for (std::int32_t j = J0; j <= J1; ++j) {
        for (std::int32_t i = I0; i <= I1; ++i) {
            const std::size_t idx =
                static_cast<std::size_t>(j) * static_cast<std::size_t>(lv.size) + static_cast<std::size_t>(i);
            r.min = std::min(r.min, lv.min[idx]);
            r.max = std::max(r.max, lv.max[idx]);
        }
    }
    return r; // per-cell margins are already folded into every level
}

float HeightField::slope_at(float x, float z) const noexcept {
    const std::int32_t corners = cells_ + 1;
    const float fx = std::clamp((x - xMin_) / cellSize_, 0.0f, static_cast<float>(cells_));
    const float fz = std::clamp((z - zMin_) / cellSize_, 0.0f, static_cast<float>(cells_));
    const auto i0 = static_cast<std::int32_t>(std::floor(fx));
    const auto j0 = static_cast<std::int32_t>(std::floor(fz));
    const std::int32_t i1 = std::min(i0 + 1, cells_);
    const std::int32_t j1 = std::min(j0 + 1, cells_);
    const float tx = fx - static_cast<float>(i0);
    const float tz = fz - static_cast<float>(j0);
    const auto at = [&](std::int32_t i, std::int32_t j) {
        return slopes_[static_cast<std::size_t>(j) * static_cast<std::size_t>(corners) +
                       static_cast<std::size_t>(i)];
    };
    const float a = at(i0, j0) + (at(i1, j0) - at(i0, j0)) * tx;
    const float b = at(i0, j1) + (at(i1, j1) - at(i0, j1)) * tx;
    return a + (b - a) * tz;
}

bool HeightField::covers(float x0, float z0, float x1, float z1) const noexcept {
    const float extent = static_cast<float>(cells_) * cellSize_;
    return x0 >= xMin_ && z0 >= zMin_ && x1 <= xMin_ + extent && z1 <= zMin_ + extent;
}

HeightField::Range HeightField::whole_range() const noexcept {
    const Level& top = levels_.back();
    return Range{top.min[0], top.max[0]};
}

} // namespace world::svo
