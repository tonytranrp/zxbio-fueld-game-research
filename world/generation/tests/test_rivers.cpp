// Prompt 006 goal 309's Checks: the meander statistics, base level, and acceptance test 9's
// remaining half (every river ends somewhere, and every lake spills).
//
// These run on the FULL pipeline output rather than a synthetic ridge, because unlike the climate
// tests there is nothing here whose right answer has to be constructed -- the research states the
// bands (wavelength 10-14 W, sinuosity 1.2-2.2) as properties of real river populations, and the
// question is whether the generated population lands in them.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "world/generation/field/fluvial.hpp"
#include "world/generation/field/macro_pipeline.hpp"
#include "world/generation/field/rivers.hpp"
#include "world/generation/field/terrain_field.hpp"

using namespace world::generation::field;

namespace {

struct Baked {
    TerrainField unfilled;
    TerrainField filled;
    FlowNetwork net;
    RiverNetwork rivers;
};

/// The whole pipeline, then the extraction -- 384 cells at 16 m = 6.1 km. The size is load-bearing
/// for the same reason it is in test_fluvial.cpp: stage 1's continent mask is 4 km, and a field
/// smaller than that holds one hillside and no drainage network to extract rivers from.
[[nodiscard]] Baked bake(int seed = 1337, std::int32_t cells = 384) {
    const float half = 0.5f * 16.0f * static_cast<float>(cells);
    TerrainField field{
        FieldGeometry{.origin_x = -half, .origin_z = -half, .cell_size = 16.0f, .cells = cells}};
    run_pipeline(field, MacroParams{.seed = seed}, -1);

    Baked out{field, field, {}, {}};
    priority_flood(out.filled);
    out.net = build_flow_network(out.filled);
    accumulate_flow(out.filled, out.net);
    out.rivers = extract_rivers(out.filled, out.net, out.unfilled.plane(Plane::Elevation), HydrologyParams{});
    return out;
}

} // namespace

TEST_CASE("meander wavelength over width lands in the research band", "[generation][rivers]") {
    // Research Part 2 §7.2: lambda = 10-14 W, composite fit 10.2, Braudrick's flume 14.
    //
    // MEASURED off the produced node positions, not read back off the parameter -- see
    // RiverNode::lateral_offset_m for why the obvious chord-based measurement does not work.
    const Baked b = bake();
    REQUIRE(b.rivers.reaches.size() > 50);
    const float ratio = b.rivers.measured_wavelength_over_width();
    INFO("measured lambda/W = " << ratio << " over the largest tenth of reaches");
    CHECK(ratio > 10.0f);
    CHECK(ratio < 14.0f);
}

TEST_CASE("sinuosity lands in the research band", "[generation][rivers]") {
    // Research Part 2 §7.3: Sacramento bends average ~1.4, cutoff-prone ~2.0, working band 1.2-2.2.
    const Baked b = bake();
    const float sinuosity = b.rivers.measured_sinuosity();
    INFO("measured sinuosity = " << sinuosity);
    CHECK(sinuosity > 1.2f);
    CHECK(sinuosity < 2.2f);
}

TEST_CASE("every river ends somewhere and every lake spills", "[generation][rivers]") {
    // Research Part 7 §9, acceptance test 9's REMAINING HALF. test_fluvial.cpp asserts the fill
    // leaves zero internal basins; this asserts the consequence at the network level -- no river
    // stops in the middle of a hillside, and no lake is a closed sink.
    const Baked b = bake();
    REQUIRE(!b.rivers.reaches.empty());
    CHECK(b.rivers.every_reach_terminates());

    // Stated mechanically rather than trusting the helper: walk every reach and every lake.
    std::size_t sea = 0;
    std::size_t lake = 0;
    std::size_t junction = 0;
    std::size_t edge = 0;
    for (const RiverReach& r : b.rivers.reaches) {
        REQUIRE(r.nodes.size() >= 2);
        switch (r.terminus) {
        case RiverReach::Terminus::Sea:
            ++sea;
            break;
        case RiverReach::Terminus::Lake:
            ++lake;
            // A lake terminus must name a real lake, and that lake must spill. A version that let
            // discarded sub-4-cell components keep their claim on cells produced exactly this
            // failure -- an index into a lake that was never stored.
            REQUIRE(r.lake >= 0);
            REQUIRE(static_cast<std::size_t>(r.lake) < b.rivers.lakes.size());
            CHECK(b.rivers.lakes[static_cast<std::size_t>(r.lake)].has_spill_path);
            break;
        case RiverReach::Terminus::Junction:
            ++junction;
            break;
        case RiverReach::Terminus::FieldEdge:
            ++edge;
            break;
        }
    }
    INFO("reaches: " << sea << " to sea, " << lake << " to lake, " << junction << " to a junction, " << edge
                     << " off-field");
    for (const Lake& l : b.rivers.lakes) {
        CHECK(l.has_spill_path);
        CHECK(l.cell_count >= 4);
    }
    // A junction is not a terminus: the water continues, so the reach must name where it goes.
    // (A reach can legitimately fail to resolve one when its downstream neighbour was rejected for
    // being under two nodes long, so this asserts the population rather than every case.)
    std::size_t linked = 0;
    for (const RiverReach& r : b.rivers.reaches) {
        linked += (r.terminus == RiverReach::Terminus::Junction && r.downstream >= 0) ? 1u : 0u;
    }
    if (junction > 0) {
        CHECK(static_cast<double>(linked) / static_cast<double>(junction) > 0.9);
    }
}

TEST_CASE("river profiles fall monotonically and never below base level", "[generation][rivers]") {
    // Research Part 2 §8: the graded profile is concave-UP and monotonically falling, and "rivers
    // can never cut below [base level] (locally) for long".
    const Baked b = bake();
    std::size_t checked = 0;
    for (const RiverReach& r : b.rivers.reaches) {
        float previous = r.nodes.front().elevation_m;
        for (const RiverNode& node : r.nodes) {
            CHECK(node.elevation_m <= previous + 1e-4f);
            CHECK(node.elevation_m >= 0.0f); // sea level
            previous = node.elevation_m;
            ++checked;
        }
        if (r.terminus == RiverReach::Terminus::Sea) {
            CHECK(r.nodes.back().elevation_m == Catch::Approx(0.0f).margin(1e-5));
        }
    }
    INFO("checked " << checked << " nodes");
    CHECK(checked > 1000);
}

TEST_CASE("width grows downstream and follows the area relation", "[generation][rivers]") {
    // Petit & Pauquet Q_bf = 0.087 A^1.044 composed with W = a Q^0.5 gives W proportional to
    // A^0.522, so width must grow monotonically with contributing area along a reach -- a river
    // that narrows downstream would mean the accumulation or the composition is wrong.
    const Baked b = bake();
    std::size_t narrowing = 0;
    std::size_t transitions = 0;
    double maxWidth = 0.0;
    for (const RiverReach& r : b.rivers.reaches) {
        for (std::size_t k = 1; k < r.nodes.size(); ++k) {
            ++transitions;
            narrowing += r.nodes[k].width_m < r.nodes[k - 1].width_m - 1e-4f ? 1u : 0u;
            maxWidth = std::max(maxWidth, static_cast<double>(r.nodes[k].width_m));
        }
    }
    INFO("narrowing steps " << narrowing << " of " << transitions << "; widest channel " << maxWidth << " m");
    CHECK(narrowing == 0);
    // And the scale finding this whole module is shaped around: the largest river on an 8 km field
    // is a few metres wide, which is SUB-CELL against a 16 m macro grid. If this ever exceeds the
    // cell size, rivers have become a heightfield feature and rivers.hpp's premise needs revisiting.
    CHECK(maxWidth < 16.0);
}

TEST_CASE("the river network is deterministic", "[generation][rivers]") {
    const auto positions = [] {
        const Baked b = bake(99, 256);
        std::vector<float> flat;
        for (const RiverReach& r : b.rivers.reaches) {
            for (const RiverNode& node : r.nodes) {
                flat.push_back(node.position.x);
                flat.push_back(node.position.y);
                flat.push_back(node.width_m);
            }
        }
        return flat;
    };
    const std::vector<float> a = positions();
    const std::vector<float> c = positions();
    REQUIRE(!a.empty());
    CHECK(a == c);
}
