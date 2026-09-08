// Prompt 006 goal 309. See rivers.hpp for why this is a polyline module.
//
// THE MEANDER CURVE. Research Part 2 §7.2 gives three constraints on a meander -- wavelength
// 10-14 W, sinuosity 1.2-2.2, radius of curvature 2-3 W -- and does not give the CURVE. A lateral
// sinusoid y = A sin(2πs/λ) cannot satisfy them together: solving for sinuosity 1.4 puts R/W at
// about 1.5, and solving for R/W = 2.5 puts sinuosity at 1.15. That is not a tuning failure, it is
// the sinusoid being the wrong shape.
//
// What ships is the SINE-GENERATED CURVE: the channel's DIRECTION, not its offset, varies
// sinusoidally with distance along it -- θ(s) = ω sin(2πs/λ). This is the classic meander form
// (Langbein & Leopold), and it is the shape a flexible strip takes when bending stress is spread
// most evenly along it, which is why real bends match it and a sinusoid does not: a sinusoid
// concentrates curvature at its crests. FLAGGED: the sine-generated curve is NOT in this project's
// research corpus -- Part 2 §7 gives the statistics, not the planform equation. It is brought in
// from outside and the module measures what it produced (wavelength/width, sinuosity, and R/W are
// all reported) rather than trusting it to land in the bands.
//
// ω is solved by bisection against the TARGET SINUOSITY rather than set from a formula, so the
// produced geometry hits the number the research states regardless of how the curve is discretised.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>

#include "world/generation/field/rivers.hpp"

namespace world::generation::field {
namespace {

constexpr float kSecondsPerYear = 3.15576e7f;
constexpr float kTwoPi = 6.28318530718f;

/// The deterministic hash every stage in this pipeline uses for jitter. Same-seed-same-world is a
/// standing rule, so nothing here may touch a global RNG.
[[nodiscard]] float hash01(std::uint32_t a, std::uint32_t b, std::uint32_t seed) {
    std::uint32_t h = seed ^ (a * 0x9e3779b9u) ^ (b * 0x85ebca6bu);
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return static_cast<float>(h) / 4294967296.0f;
}

/// Arc length of a sine-generated curve of unit valley length, per unit wavelength, at amplitude ω.
///
/// Sinuosity is channel length over valley length, and for θ(s) = ω sin(2πs/λ) the valley-direction
/// advance per unit channel length is the mean of cos(θ(s)). So sinuosity = 1 / mean(cos θ). This
/// integrates that mean numerically rather than reaching for the closed form (it is 1/J₀(ω), a
/// Bessel function) -- 256 samples of a cosine is cheaper than pulling in a special function, and
/// the whole point of solving by bisection is not to depend on the identity being remembered right.
[[nodiscard]] double sinuosity_of(double omega) {
    constexpr int kSamples = 256;
    double mean = 0.0;
    for (int i = 0; i < kSamples; ++i) {
        const double s = (static_cast<double>(i) + 0.5) / kSamples;
        mean += std::cos(omega * std::sin(kTwoPi * s));
    }
    mean /= kSamples;
    return mean > 1e-9 ? 1.0 / mean : 1.0e9;
}

/// The ω that produces `target` sinuosity. Monotone in ω over [0, π), so bisection is exact enough.
[[nodiscard]] double solve_omega(double target) {
    if (target <= 1.0) {
        return 0.0;
    }
    double lo = 0.0;
    double hi = 2.4; // sinuosity ~ 4.4 here; well past the research's 2.2 ceiling
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (sinuosity_of(mid) < target) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return 0.5 * (lo + hi);
}

} // namespace

float RiverNetwork::measured_wavelength_over_width(float topFraction) const {
    // The wavelength is a property of the reach as built (one bend per `meander_wavelength_ratio`
    // widths of channel), so this measures it back off the produced nodes rather than reporting the
    // parameter -- goal 309's Check asks for a MEASURED ratio.
    std::vector<const RiverReach*> ranked;
    for (const RiverReach& r : reaches) {
        if (r.nodes.size() >= 4 && r.channel_length_m > 0.0f) {
            ranked.push_back(&r);
        }
    }
    if (ranked.empty()) {
        return 0.0f;
    }
    std::sort(ranked.begin(), ranked.end(), [](const RiverReach* a, const RiverReach* b) {
        return a->nodes.back().discharge_m3s > b->nodes.back().discharge_m3s;
    });
    const std::size_t take =
        std::max<std::size_t>(1, static_cast<std::size_t>(topFraction * static_cast<float>(ranked.size())));

    double sum = 0.0;
    std::size_t counted = 0;
    for (std::size_t k = 0; k < take; ++k) {
        const RiverReach& r = *ranked[k];
        // Zero crossings of the node's LATERAL OFFSET from its own valley centreline: two per
        // wavelength. Measuring against the reach's end-to-end chord instead reported 75.6 for a
        // requested 12, because a D8 reach wanders and the chord is not its local axis -- the
        // offset drifts to one side and most of the crossings never happen.
        int crossings = 0;
        float previous = 0.0f;
        bool have = false;
        double widthSum = 0.0;
        for (const RiverNode& node : r.nodes) {
            if (have && ((node.lateral_offset_m > 0.0f) != (previous > 0.0f))) {
                ++crossings;
            }
            previous = node.lateral_offset_m;
            have = true;
            widthSum += static_cast<double>(node.width_m);
        }
        if (crossings < 2 || r.valley_length_m <= 0.0f) {
            continue;
        }
        const double wavelength = 2.0 * static_cast<double>(r.valley_length_m) / crossings;
        const double meanWidth = widthSum / static_cast<double>(r.nodes.size());
        if (meanWidth > 1e-6) {
            sum += wavelength / meanWidth;
            ++counted;
        }
    }
    return counted == 0 ? 0.0f : static_cast<float>(sum / static_cast<double>(counted));
}

float RiverNetwork::measured_sinuosity(float topFraction) const {
    std::vector<const RiverReach*> ranked;
    for (const RiverReach& r : reaches) {
        if (r.valley_length_m > 1e-3f) {
            ranked.push_back(&r);
        }
    }
    if (ranked.empty()) {
        return 0.0f;
    }
    std::sort(ranked.begin(), ranked.end(), [](const RiverReach* a, const RiverReach* b) {
        return a->nodes.back().discharge_m3s > b->nodes.back().discharge_m3s;
    });
    const std::size_t take =
        std::max<std::size_t>(1, static_cast<std::size_t>(topFraction * static_cast<float>(ranked.size())));
    double sum = 0.0;
    for (std::size_t k = 0; k < take; ++k) {
        sum += static_cast<double>(ranked[k]->channel_length_m) / ranked[k]->valley_length_m;
    }
    return static_cast<float>(sum / static_cast<double>(take));
}

bool RiverNetwork::every_reach_terminates() const {
    for (const RiverReach& r : reaches) {
        if (r.terminus == RiverReach::Terminus::Lake) {
            if (r.lake < 0 || static_cast<std::size_t>(r.lake) >= lakes.size()) {
                return false;
            }
            if (!lakes[static_cast<std::size_t>(r.lake)].has_spill_path) {
                return false;
            }
        }
    }
    for (const Lake& l : lakes) {
        if (!l.has_spill_path) {
            return false;
        }
    }
    return true;
}

RiverNetwork extract_rivers(const TerrainField& filled, const FlowNetwork& net,
                            std::span<const float> unfilledElevation, const HydrologyParams& p) {
    RiverNetwork out;
    const std::span<const float> h = filled.plane(Plane::Elevation);
    const std::span<const float> acc = filled.plane(Plane::FlowAccum);
    const std::span<const float> precip = filled.plane(Plane::Precipitation);
    const FieldGeometry& g = filled.geometry();
    const std::int32_t cols = filled.cells();
    const float cellArea = g.cell_area();
    const float thresholdCells = p.channel_threshold_km2 * 1.0e6f / cellArea;
    const std::size_t n = filled.cell_count();

    const auto cell_x = [cols](std::size_t i) { return static_cast<std::int32_t>(i % static_cast<std::size_t>(cols)); };
    const auto cell_z = [cols](std::size_t i) { return static_cast<std::int32_t>(i / static_cast<std::size_t>(cols)); };
    const auto on_edge = [&](std::size_t i) {
        const std::int32_t x = cell_x(i);
        const std::int32_t z = cell_z(i);
        return x == 0 || z == 0 || x == cols - 1 || z == cols - 1;
    };

    // ---- discharge -------------------------------------------------------------------------
    // Precipitation-weighted, so a wet windward basin carries more water per km² than a dry lee
    // one. This is the join between goal 302 and this goal, and without it the orographic field
    // would decorate the world without doing anything in it.
    const std::vector<float> wetAcc = accumulate_weighted(filled, net, precip);
    const std::vector<std::uint8_t> order = strahler_order(filled, net, p.channel_threshold_km2);

    // The PRECIPITATION-WEIGHTED contributing area, km². Because the precipitation plane is
    // normalised to a field mean of 1.0, this averages to the true area -- so Petit & Pauquet's
    // relation is applied at the scale it was calibrated at -- while a wet windward basin gets a
    // larger effective area than a dry lee one of the same size. That is the whole join between
    // goal 302 and this goal.
    const auto effective_area_km2 = [&](std::size_t i) { return wetAcc[i] * cellArea / 1.0e6f; };
    const auto mean_discharge_at = [&](std::size_t i) {
        // sum(precipitation multiplier) over the basin * cell area * runoff depth per year -> m³/s
        return wetAcc[i] * cellArea * p.mean_annual_runoff_m / kSecondsPerYear;
    };
    // Petit & Pauquet (1997): Q_bf = 0.087 A^1.044, calibrated on 4-2,700 km² Ardennes catchments.
    const auto bankfull_at = [&](std::size_t i) {
        return p.bankfull_area_coefficient *
               std::pow(std::max(effective_area_km2(i), 0.0f), p.bankfull_area_exponent);
    };
    const auto width_at = [&](std::size_t i) {
        return p.width_coefficient * std::pow(std::max(bankfull_at(i), 0.0f), p.width_exponent);
    };

    // ---- lakes -----------------------------------------------------------------------------
    // A lake is EXACTLY the set of cells the fill had to raise. See rivers.hpp on why this is a
    // definition rather than the epsilon-slope heuristic it replaced.
    std::vector<std::int32_t> lakeOf(n, -1);
    if (unfilledElevation.size() == n) {
        constexpr float kFilledBy = 1.0e-3f; // above priority_flood's own 1e-4 epsilon step
        std::vector<std::uint8_t> flooded(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            flooded[i] = (h[i] - unfilledElevation[i] > kFilledBy && h[i] > p.sea_level) ? 1u : 0u;
        }
        std::vector<std::uint32_t> stack;
        std::vector<std::uint32_t> members;
        for (std::size_t start = 0; start < n; ++start) {
            if (flooded[start] == 0 || lakeOf[start] >= 0) {
                continue;
            }
            const auto id = static_cast<std::int32_t>(out.lakes.size());
            Lake lake;
            lake.surface_m = h[start];
            std::size_t lowestRim = start;
            float lowestRimHeight = std::numeric_limits<float>::max();
            stack.assign(1, static_cast<std::uint32_t>(start));
            members.clear();
            lakeOf[start] = id;
            while (!stack.empty()) {
                const std::uint32_t c = stack.back();
                stack.pop_back();
                members.push_back(c);
                lake.surface_m = std::max(lake.surface_m, h[c]);
                const std::int32_t ccx = cell_x(c);
                const std::int32_t ccz = cell_z(c);
                for (std::int32_t dz = -1; dz <= 1; ++dz) {
                    for (std::int32_t dx = -1; dx <= 1; ++dx) {
                        if ((dx == 0 && dz == 0) || !filled.in_bounds(ccx + dx, ccz + dz)) {
                            continue;
                        }
                        const std::size_t nb = filled.index(ccx + dx, ccz + dz);
                        if (flooded[nb] != 0) {
                            if (lakeOf[nb] < 0) {
                                lakeOf[nb] = id;
                                stack.push_back(static_cast<std::uint32_t>(nb));
                            }
                        } else if (h[nb] < lowestRimHeight) {
                            lowestRimHeight = h[nb];
                            lowestRim = nb;
                        }
                    }
                }
            }
            lake.cell_count = members.size();
            // A one- or two-cell "lake" is a numerical artefact of the fill, not a water body.
            // Its cells must be UNCLAIMED again, or a reach flowing into one carries a lake index
            // that was never pushed -- which is exactly the out-of-range that made the first
            // version report "every reach terminates: NO" on a network whose lakes were all fine.
            if (lake.cell_count < 4) {
                for (const std::uint32_t c : members) {
                    lakeOf[c] = -1;
                }
                continue;
            }
            lake.outlet_cell = lowestRim;
            // The spill path: walk the receivers from the rim and see where it ends.
            std::size_t walk = lowestRim;
            for (std::size_t step = 0; step < n; ++step) {
                if (h[walk] <= p.sea_level) {
                    lake.has_spill_path = true;
                    break;
                }
                if (on_edge(walk)) {
                    lake.has_spill_path = true; // leaves the patch, which is a real terminus
                    break;
                }
                const std::uint32_t r = net.receiver[walk];
                if (r == walk) {
                    break; // a sink that is neither sea nor edge: no spill path
                }
                walk = r;
            }
            out.lakes.push_back(lake);
        }
    }

    // ---- reaches ---------------------------------------------------------------------------
    const auto isChannel = [&](std::size_t i) { return acc[i] >= thresholdCells && h[i] > p.sea_level; };
    // A reach's head cell, so a downstream reach can be found by the cell it starts at and the
    // junction links can be filled in once every reach exists.
    std::vector<std::size_t> joinCell;
    // Which reach owns each cell. A first version indexed HEADS only and resolved just 69% of the
    // junctions -- because a tributary joining a larger river lands MID-REACH, not on that river's
    // head, and only a confluence of two equal orders creates a new head. The topology was right
    // and the lookup was wrong.
    std::vector<std::int32_t> reachOfCell(n, -1);

    std::vector<std::uint8_t> claimed(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (!isChannel(i) || claimed[i] != 0) {
            continue;
        }
        // The head of a reach: no channel donor of the SAME Strahler order flows in.
        bool head = true;
        const std::int32_t cx = cell_x(i);
        const std::int32_t cz = cell_z(i);
        for (std::int32_t dz = -1; dz <= 1 && head; ++dz) {
            for (std::int32_t dx = -1; dx <= 1 && head; ++dx) {
                if ((dx == 0 && dz == 0) || !filled.in_bounds(cx + dx, cz + dz)) {
                    continue;
                }
                const std::size_t nb = filled.index(cx + dx, cz + dz);
                if (net.receiver[nb] == i && isChannel(nb) && order[nb] == order[i]) {
                    head = false;
                }
            }
        }
        if (!head) {
            continue;
        }

        RiverReach reach;
        reach.strahler = order[i];
        std::vector<std::size_t> cells;
        std::size_t c = i;
        std::size_t join = n;
        for (std::size_t step = 0; step < n; ++step) {
            claimed[c] = 1;
            cells.push_back(c);
            const std::uint32_t r = net.receiver[c];
            if (r == c) {
                reach.terminus = RiverReach::Terminus::FieldEdge;
                break;
            }
            if (h[r] <= p.sea_level) {
                cells.push_back(r);
                reach.terminus = RiverReach::Terminus::Sea;
                break;
            }
            if (lakeOf[r] >= 0) {
                cells.push_back(r);
                reach.terminus = RiverReach::Terminus::Lake;
                reach.lake = lakeOf[r];
                break;
            }
            if (on_edge(r)) {
                cells.push_back(r);
                reach.terminus = RiverReach::Terminus::FieldEdge;
                break;
            }
            if (!isChannel(r) || order[r] != order[c]) {
                // The water continues into a higher-order reach. That is a JUNCTION, not a
                // terminus, and calling it one made 531 of 1160 reaches look like dead ends.
                cells.push_back(r);
                reach.terminus = RiverReach::Terminus::Junction;
                join = r;
                break;
            }
            c = r;
        }
        if (cells.size() < 2) {
            continue;
        }

        // ---- the straight valley centreline --------------------------------------------------
        std::vector<glm::vec2> centre;
        centre.reserve(cells.size());
        double widthSum = 0.0;
        for (const std::size_t cell : cells) {
            centre.push_back(g.to_world(cell_x(cell), cell_z(cell)));
            widthSum += static_cast<double>(width_at(cell));
        }
        const auto meanWidth = static_cast<float>(widthSum / static_cast<double>(cells.size()));

        // ---- smooth the D8 staircase out of the centreline ------------------------------------
        // Research Part 7 §11(7): "route on the macro-grid then SMOOTH/meander the centreline". D8
        // moves in 45-degree steps of one cell, and on this world a cell (16 m) is over half a
        // meander wavelength (~29 m) -- so without this the curve being measured is the grid's
        // zig-zag with a meander added, and the produced wavelength reads 17.2 W for a requested 12.
        // Endpoints are pinned: a river must still start where its channel head is and end at its
        // mouth.
        for (int pass = 0; pass < p.centreline_smoothing_passes && centre.size() > 2; ++pass) {
            std::vector<glm::vec2> next = centre;
            for (std::size_t k = 1; k + 1 < centre.size(); ++k) {
                next[k] = 0.25f * centre[k - 1] + 0.5f * centre[k] + 0.25f * centre[k + 1];
            }
            centre.swap(next);
        }

        double valleyLength = 0.0;
        std::vector<double> segStart(centre.size(), 0.0);
        for (std::size_t k = 1; k < centre.size(); ++k) {
            valleyLength += static_cast<double>(glm::length(centre[k] - centre[k - 1]));
            segStart[k] = valleyLength;
        }
        reach.valley_length_m = static_cast<float>(valleyLength);
        if (valleyLength < 1e-3) {
            continue;
        }

        // ---- meander --------------------------------------------------------------------------
        const float lambda = p.meander_wavelength_ratio * std::max(meanWidth, 1e-3f);
        const auto omega = static_cast<float>(solve_omega(p.meander_sinuosity));
        const float phase =
            kTwoPi * p.meander_jitter * (hash01(static_cast<std::uint32_t>(cells.front()), reach.strahler, p.seed) - 0.5f);

        const double sampleStep = std::max(static_cast<double>(lambda) / 16.0, 0.25);
        // The channel is LONGER than the valley by the sinuosity, so the sample budget has to be
        // too. A first version sized it from the valley length alone, ran out of samples before the
        // valley was consumed, and reported a sinuosity of 1.02 for a requested 1.4 -- the curve was
        // right and the loop simply stopped early.
        constexpr double kSinuosityHeadroom = 4.0;
        const auto sampleCount = static_cast<std::size_t>(valleyLength * kSinuosityHeadroom / sampleStep) + 4;

        glm::vec2 cursor = centre.front();
        double travelled = 0.0; // valley-direction arc length
        double channelLength = 0.0;
        std::size_t segment = 0;
        reach.nodes.reserve(std::min<std::size_t>(sampleCount, 4096));
        for (std::size_t k = 0; k < sampleCount; ++k) {
            while (segment + 1 < centre.size() && travelled >= segStart[segment + 1]) {
                ++segment;
            }
            if (segment + 1 >= centre.size()) {
                break;
            }
            const glm::vec2 seg = centre[segment + 1] - centre[segment];
            const float segLen = glm::length(seg);
            if (segLen < 1e-6f) {
                ++segment;
                continue;
            }
            const glm::vec2 segDir = seg / segLen;
            // Where the straight valley centreline is at this arc length, and the node's signed
            // offset from it -- the quantity the wavelength is measured on.
            const glm::vec2 onCentre =
                centre[segment] + segDir * static_cast<float>(travelled - segStart[segment]);
            const glm::vec2 perp{-segDir.y, segDir.x};

            const float theta = omega * std::sin(kTwoPi * static_cast<float>(travelled) / lambda + phase);
            const float cs = std::cos(theta);
            const float sn = std::sin(theta);
            const glm::vec2 flow{segDir.x * cs - segDir.y * sn, segDir.x * sn + segDir.y * cs};

            const std::size_t cell = cells[std::min(segment, cells.size() - 1)];
            RiverNode node;
            node.position = cursor;
            node.lateral_offset_m = glm::dot(cursor - onCentre, perp);
            node.discharge_m3s = mean_discharge_at(cell);
            node.width_m = width_at(cell);
            node.elevation_m = h[cell];
            reach.nodes.push_back(node);

            cursor += flow * static_cast<float>(sampleStep);
            channelLength += sampleStep;
            travelled += sampleStep * static_cast<double>(cs);
        }
        reach.channel_length_m = static_cast<float>(channelLength);
        if (reach.nodes.size() < 2) {
            continue;
        }

        // ---- base level -----------------------------------------------------------------------
        // Research Part 2 §8: "rivers can never cut below [base level] (locally) for long", and a
        // graded profile falls monotonically. The sampled elevations come from a field diffused
        // AFTER incision, so they wobble upward by centimetres; a river that runs uphill for one
        // node is a rendering artefact waiting to happen.
        float running = reach.nodes.front().elevation_m;
        for (RiverNode& node : reach.nodes) {
            running = std::min(running, node.elevation_m);
            node.elevation_m = std::max(running, p.sea_level);
        }
        if (reach.terminus == RiverReach::Terminus::Sea) {
            reach.nodes.back().elevation_m = p.sea_level;
        }
        const auto reachId = static_cast<std::int32_t>(out.reaches.size());
        // A junction reach carries its join cell as its last node so the two polylines meet without
        // a visible gap, but that cell BELONGS to the downstream reach. Claiming it here is what
        // made the link resolve to self and then to nothing: whichever reach happened to be built
        // first took the shared cell, and the other could no longer be found through it.
        const std::size_t owned =
            cells.size() - (reach.terminus == RiverReach::Terminus::Junction ? 1u : 0u);
        for (std::size_t k = 0; k < owned; ++k) {
            if (reachOfCell[cells[k]] < 0) {
                reachOfCell[cells[k]] = reachId;
            }
        }
        joinCell.push_back(join);
        out.reaches.push_back(std::move(reach));
    }

    // Link junctions: a reach that ends at one flows into whichever reach OWNS the cell it joined.
    for (std::size_t r = 0; r < out.reaches.size(); ++r) {
        if (out.reaches[r].terminus != RiverReach::Terminus::Junction || joinCell[r] >= n) {
            continue;
        }
        const std::int32_t owner = reachOfCell[joinCell[r]];
        // Never link a reach to itself: the join cell is shared with the downstream reach, and
        // whichever was built first claimed it.
        out.reaches[r].downstream = owner == static_cast<std::int32_t>(r) ? -1 : owner;
    }

    // ---- deltas ----------------------------------------------------------------------------
    // Research Part 2 §9.1, the Galloway triangle: river, wave and tide. THIS WORLD HAS NO TIDE
    // MODEL AND NO WAVE MODEL, so two of the three vertices cannot be selected on their own terms.
    // Rather than label deltas from a coin flip, the choice runs on the one axis both sides of
    // which are computable here:
    //
    //   supply ~ discharge (the river's own sediment-delivery proxy)
    //   wave   ~ open-water FETCH at the mouth, sampled as the fraction of a disc around it that is
    //            below sea level -- an exposed headland mouth is planed off by alongshore energy,
    //            a sheltered embayed mouth progrades
    //
    // Tide-dominated is therefore UNREACHABLE and is deliberately not in the enum. Saying so is
    // better than an enumerator that can never occur.
    for (const RiverReach& reach : out.reaches) {
        if (reach.terminus != RiverReach::Terminus::Sea) {
            continue;
        }
        const RiverNode& mouth = reach.nodes.back();
        const glm::vec2 cell = g.to_cell(mouth.position.x, mouth.position.y);
        const auto mx = static_cast<std::int32_t>(std::lround(cell.x));
        const auto mz = static_cast<std::int32_t>(std::lround(cell.y));
        constexpr std::int32_t kFetchRadius = 12; // ~200 m at 16 m cells
        std::size_t water = 0;
        std::size_t sampled = 0;
        for (std::int32_t dz = -kFetchRadius; dz <= kFetchRadius; ++dz) {
            for (std::int32_t dx = -kFetchRadius; dx <= kFetchRadius; ++dx) {
                if (dx * dx + dz * dz > kFetchRadius * kFetchRadius || !filled.in_bounds(mx + dx, mz + dz)) {
                    continue;
                }
                ++sampled;
                water += h[filled.index(mx + dx, mz + dz)] <= p.sea_level ? 1u : 0u;
            }
        }
        const float openness = sampled == 0 ? 0.0f : static_cast<float>(water) / static_cast<float>(sampled);
        Delta delta;
        delta.apex = mouth.position;
        delta.discharge_m3s = mouth.discharge_m3s;
        delta.supply_over_wave = mouth.discharge_m3s / std::max(openness, 1e-3f);
        // The 0.5 pivot is the point at which the mouth faces as much open water as land -- a
        // headland rather than an embayment.
        delta.regime = openness > 0.5f ? DeltaRegime::WaveDominated : DeltaRegime::RiverDominated;
        // Research §9.2 gives progradation in tens of m/yr; this is a static stamp, so the radius
        // comes from the channel instead -- a delta is a distributary fan a few channel widths
        // across at its apex, wider when the river wins and clipped back when the waves do.
        delta.radius_m = mouth.width_m * (delta.regime == DeltaRegime::RiverDominated ? 8.0f : 4.0f);
        out.deltas.push_back(delta);
    }

    return out;
}

} // namespace world::generation::field
