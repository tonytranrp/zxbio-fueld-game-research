#pragma once

// Prompt 006 Group AM-B: the fluvial core. Research Part 7 §10.2 stage 3, Part 2 §11 stages 2-6.
//
// EVERY CONSTANT BELOW COMES FROM PART 2 §11'S PARAMETER TABLE, or is derived from ones that do.
// The prompt's rule is that a constant this pass introduces either comes from that table or is
// justified against it, so each carries its source in a comment rather than a plausible value.

#include <cstdint>
#include <vector>

#include "world/generation/field/terrain_field.hpp"

namespace world::generation::field {

/// D8 flow structure over a depression-filled field.
struct FlowNetwork {
    /// Steepest-downslope neighbour of each cell; a cell whose receiver is itself is an outlet.
    std::vector<std::uint32_t> receiver;
    /// Cells by DECREASING filled elevation. After priority-flood every receiver is strictly lower
    /// than its donor, so this visits a cell before its receiver -- which makes accumulation one
    /// pass, and the same order reversed makes the implicit incision sweep one pass.
    std::vector<std::uint32_t> order;
};

struct FluvialParams {
    /// Stream-power exponents. Part 2 §11: start m = 0.5, n = 1, so the concavity theta = m/n
    /// lands at 0.5 -- the top of the 0.45-0.5 band the table quotes.
    float m = 0.5f;
    /// Erodibility. The table's band is wide because K absorbs lithology and climate; this is a
    /// mid value, and goal 307's drainage-density measurement is what tunes it.
    float k = 4.0e-5f;
    /// Rock uplift, m/yr. Part 1 §3 bounds mountain height by isostasy and erosion, not by U.
    float uplift = 3.0e-4f;
    /// Hillslope diffusivity, m²/yr. Part 2 §4's band for soil-mantled slopes.
    float diffusivity = 1.0e-2f;
    /// Years per step. The implicit incision is unconditionally stable at any dt; the EXPLICIT
    /// diffusion is not, and dt < dx²/(4D) = 16²/(4·0.01) = 6,400 yr is what bounds this.
    float dt = 1000.0f;
    int steps = 40;
    int diffusion_steps = 40;
    float sea_level = 0.0f;
};

/// Barnes/Lehman/Mulla priority-flood with the +epsilon variant, so flats carry an infinitesimal
/// gradient and D8 has a defined direction inside every filled basin.
///
/// Post-condition, which goal 303's Check asserts mechanically over the whole field rather than by
/// sampling: **every cell has a strictly downhill path to the boundary, so there are zero internal
/// basins by construction.**
void priority_flood(TerrainField& out, float epsilon = 1.0e-4f);

/// D8 receivers plus the traversal order. Requires a depression-filled field.
[[nodiscard]] FlowNetwork build_flow_network(const TerrainField& field);

/// Upstream contributing CELLS per cell (multiply by `cell_area()` for m²). Exactly conservative:
/// the total delivered to the outlets equals the cell count, which goal 304's Check asserts.
void accumulate_flow(TerrainField& out, const FlowNetwork& net);

/// Braun-Willett implicit stream-power incision, n = 1. Unconditionally stable at any dt.
void incise_stream_power(TerrainField& out, const FlowNetwork& net, const FluvialParams& p);

/// Linear hillslope diffusion -- the term whose absence the research calls "the single most
/// recognizable 'procedural terrain' tell".
void diffuse_hillslopes(TerrainField& out, const FluvialParams& p);

/// L_c = (D/K)^(1/(2m+2)) -- the valley spacing the parameters imply, derived rather than written
/// down, because goal 306 checks the measured spacing against it.
[[nodiscard]] float characteristic_valley_spacing(const FluvialParams& p) noexcept;

} // namespace world::generation::field
