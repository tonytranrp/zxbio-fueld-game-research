#pragma once

// Prompt 006 Group AM-A: the baked macro field the whole pipeline writes into and `height_at`
// reads out of.
//
// WHY A MACRO FIELD AND NOT A TILE PER REGION. The playable region is 512 m across = 0.262 km².
// The research's channel-head threshold is A_c = 0.1-5 km² (Part 2 §11), so the entire playable
// world holds **2.6 channel-head areas at the smallest threshold and 0.05 at the largest**. It
// cannot contain a drainage network at all. Running priority-flood and flow accumulation on the
// region would compute a river network for a patch smaller than one river's headwater catchment
// and produce either nothing or noise gullies -- which is §9.4's failure mode in both directions.
// So the erosion runs on a field FIFTEEN TIMES WIDER than the region and the region is a window
// into it. Full arithmetic: research/earth-terrain-pipeline-log.md §1.
//
// THE SIZE, from three research numbers rather than by feel:
//   * A_c = 0.1 km² must be many cells, or the constant-drop test (§9.6) is measuring noise.
//   * §9's statistics want "a 512²-or-larger patch".
//   * §9.4's drainage-density band is quoted at 30 m-equivalent resolution.
// 8 km at 16 m cells (500 x 500) gives 390 cells per channel head, 15x the region's width, and
// ~8 MB for eight planes. The simulation runs at 16 m; the drainage-density ACCEPTANCE TEST
// measures at 30 m-equivalent by downsampling, which is the honest way to use a band quoted at a
// stated resolution rather than degrading the whole simulation to match it.
//
// LAYOUT IS SoA, per memory-and-performance.md: every stage of the pipeline sweeps one or two
// planes over the whole grid, so a struct-of-eleven-floats per cell would touch eleven cache lines
// to read one quantity. Parallel `std::vector<float>` planes are the right shape here and the
// stages that need two planes read two.

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "engine/core/math.hpp"

namespace world::generation::field {

/// The quantities the pipeline produces. Each is one plane over the same grid.
///
/// Ordered by the stage that writes it, so the enum reads as the pipeline's own dependency chain:
/// nothing later than `Elevation` can be computed before it, `FlowAccum` needs the filled surface,
/// and the biome index needs both climate planes.
enum class Plane : std::uint8_t {
    Elevation,     ///< metres above sea level; the only plane `height_at` reads directly
    Precipitation, ///< m/yr, from the orographic stage
    Temperature,   ///< °C, from latitude + lapse rate
    FlowAccum,     ///< upstream contributing cells (multiply by cell area for m²)
    Lithology,     ///< index into the lithology table; drives erodibility K and the cliff look
    Biome,         ///< index into the biome table
    IceSnow,       ///< 0..1 coverage; the glacial stencil's own mask, and the shading's
    Sediment,      ///< metres of transportable material above bedrock
    Count
};

[[nodiscard]] constexpr const char* plane_name(Plane p) noexcept {
    switch (p) {
    case Plane::Elevation:
        return "elevation";
    case Plane::Precipitation:
        return "precipitation";
    case Plane::Temperature:
        return "temperature";
    case Plane::FlowAccum:
        return "flow_accum";
    case Plane::Lithology:
        return "lithology";
    case Plane::Biome:
        return "biome";
    case Plane::IceSnow:
        return "ice_snow";
    case Plane::Sediment:
        return "sediment";
    case Plane::Count:
        break;
    }
    return "?";
}

/// Where the field sits in the world, and how finely it is sampled.
struct FieldGeometry {
    float origin_x = -4096.0f; ///< world x of cell (0,0)'s CENTRE
    float origin_z = -4096.0f;
    float cell_size = 16.0f; ///< metres per cell
    std::int32_t cells = 500;

    [[nodiscard]] constexpr float extent() const noexcept { return static_cast<float>(cells) * cell_size; }
    /// Continuous cell coordinates of a world position. Fractional; the reader interpolates.
    [[nodiscard]] constexpr glm::vec2 to_cell(float x, float z) const noexcept {
        return glm::vec2{(x - origin_x) / cell_size, (z - origin_z) / cell_size};
    }
    [[nodiscard]] constexpr glm::vec2 to_world(std::int32_t cx, std::int32_t cz) const noexcept {
        return glm::vec2{origin_x + static_cast<float>(cx) * cell_size,
                         origin_z + static_cast<float>(cz) * cell_size};
    }
    /// Cell area in m², which is what turns a flow-accumulation count into a drainage area.
    [[nodiscard]] constexpr float cell_area() const noexcept { return cell_size * cell_size; }
};

/// The baked field: `Plane::Count` parallel planes over one grid.
///
/// Deliberately a plain owner of vectors rather than a class with a query API. The stages that
/// write it want raw spans (a priority-flood is a loop over an array, not a sequence of accessor
/// calls), and the ONE reader that needs interpolation is `FieldSampler`, which is a separate type
/// so that a stage cannot accidentally read a half-written plane through a convenient accessor.
class TerrainField {
public:
    TerrainField() = default;
    explicit TerrainField(FieldGeometry geometry);

    [[nodiscard]] const FieldGeometry& geometry() const noexcept { return geometry_; }
    [[nodiscard]] std::int32_t cells() const noexcept { return geometry_.cells; }
    [[nodiscard]] std::size_t cell_count() const noexcept {
        return static_cast<std::size_t>(geometry_.cells) * static_cast<std::size_t>(geometry_.cells);
    }

    [[nodiscard]] std::span<float> plane(Plane p) noexcept;
    [[nodiscard]] std::span<const float> plane(Plane p) const noexcept;

    /// Row-major, X innermost -- the same convention `generate_column_heights` already uses, so a
    /// reader that walks a row of the world walks a contiguous run here too.
    [[nodiscard]] std::size_t index(std::int32_t cx, std::int32_t cz) const noexcept {
        return static_cast<std::size_t>(cz) * static_cast<std::size_t>(geometry_.cells) +
               static_cast<std::size_t>(cx);
    }
    [[nodiscard]] bool in_bounds(std::int32_t cx, std::int32_t cz) const noexcept {
        return cx >= 0 && cz >= 0 && cx < geometry_.cells && cz < geometry_.cells;
    }

    /// Total bytes the planes occupy -- goal 296's Check reports this rather than estimating it.
    [[nodiscard]] std::uint64_t bytes() const noexcept {
        return static_cast<std::uint64_t>(planes_.size()) * sizeof(float);
    }

private:
    FieldGeometry geometry_{};
    std::vector<float> planes_; ///< Plane::Count consecutive blocks of cell_count() floats
};

} // namespace world::generation::field
