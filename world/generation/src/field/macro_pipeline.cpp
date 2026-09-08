#include "world/generation/field/macro_pipeline.hpp"

#include "world/generation/field/climate.hpp"
#include "world/generation/field/fluvial.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace world::generation::field {
namespace {

// ---------------------------------------------------------------------------------------------
// Stage 1 of research Part 7 §10.2: the continental skeleton.
//
// THIS IS THE ONLY STAGE IMPLEMENTED IN THIS COMMIT, and it deliberately reproduces the SHAPE of
// today's terrain rather than replacing it. The architecture change (goal 295-299) and the physics
// (goals 300+) are separate risks, and putting them in one commit would mean that if the world
// looks wrong afterwards there are two candidate causes. With this stage as a low-passed copy of
// the existing field, `height_at` returns macro + detail summing to a world of the same character,
// and any difference is attributable to the RECONSTRUCTION -- which is exactly what goal 297's
// continuity test and goal 298's cost measurement are about.
//
// The noise itself is a plain value-noise fBm rather than FastNoise2's Simplex, for one reason
// that matters and is not a preference: this translation unit must not include FastNoise2.
// `heightmap_generator.cpp` is documented as "the only place FastNoise2 headers are included"
// (project brief §8), and a pipeline stage is not that place. When goal 300 replaces this stage
// with real continents the same constraint applies.
[[nodiscard]] std::uint32_t hash2(std::int32_t x, std::int32_t z, std::uint32_t seed) noexcept {
    std::uint32_t h = seed;
    h ^= static_cast<std::uint32_t>(x) * 0x9E3779B9u;
    h = (h ^ (h >> 15)) * 0x85EBCA6Bu;
    h ^= static_cast<std::uint32_t>(z) * 0xC2B2AE35u;
    h = (h ^ (h >> 13)) * 0xC2B2AE35u;
    return h ^ (h >> 16);
}

[[nodiscard]] float value_noise(float x, float z, std::uint32_t seed) noexcept {
    const auto ix = static_cast<std::int32_t>(std::floor(x));
    const auto iz = static_cast<std::int32_t>(std::floor(z));
    const float fx = x - static_cast<float>(ix);
    const float fz = z - static_cast<float>(iz);
    // Quintic smoothstep: C² at the lattice, so an fBm of these has a continuous second
    // derivative and the erosion solvers downstream do not see lattice-aligned curvature.
    const auto fade = [](float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); };
    const float ux = fade(fx);
    const float uz = fade(fz);
    const auto corner = [&](std::int32_t cx, std::int32_t cz) {
        return static_cast<float>(hash2(cx, cz, seed)) * (1.0f / 2147483648.0f) - 1.0f;
    };
    const float a = corner(ix, iz);
    const float b = corner(ix + 1, iz);
    const float c = corner(ix, iz + 1);
    const float d = corner(ix + 1, iz + 1);
    return (a + (b - a) * ux) + ((c + (d - c) * ux) - (a + (b - a) * ux)) * uz;
}

/// n octaves of value noise at a given base scale, gain 0.5, normalised to about [-1, 1].
[[nodiscard]] float fbm(float x, float z, float scale, int octaves, std::uint32_t seed) noexcept {
    float sum = 0.0f;
    float amplitude = 1.0f;
    float total = 0.0f;
    float frequency = 1.0f / scale;
    for (int i = 0; i < octaves; ++i) {
        sum += amplitude * value_noise(x * frequency, z * frequency, seed + static_cast<std::uint32_t>(i));
        total += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return sum / std::max(total, 1e-6f);
}

void stage_continents(TerrainField& out, const MacroParams& params) {
    // GOAL 301. Three things, in the order they matter.
    //
    // ONE: COHERENT LANDMASSES, NOT SPECKLE. The first version used the shipped terrain's own
    // 200 m feature scale, and `tools/terrain_dump` showed the result plainly -- an isotropic
    // speckle of small islands across the whole 8 km field, with no continent anywhere. That is
    // the noise-terrain failure the research names, and at macro scale it is glaring where a game
    // frame hid it. The continent mask is now a 4 km feature: an 8 km field holds one or two
    // landmasses, which is what a patch of a real coast looks like.
    //
    // TWO: THE HYPSOMETRY, AND AN ADAPTATION THAT HAS TO BE STATED RATHER THAN SILENTLY MADE.
    // Research §9.3 asks for a BIMODAL area-elevation curve with ~29% land. **That is a
    // whole-Earth statistic and this field is 8 km across -- 1.3e-7 of the planet's surface.** A
    // random 8 km patch of Earth is almost entirely land or almost entirely ocean; it cannot
    // express a planetary land fraction, and forcing 29% on it would be applying a statistic to a
    // sample that cannot carry it.
    //
    // What §9.3 DOES say that is testable on a patch is the shape of the LAND half: the land peak
    // sits within a few hundred metres of sea level with a tail to higher ground, and the
    // distribution is not Gaussian. That is a hypsometric CURVE property, it is measurable here,
    // and it is what the power curve below produces -- most land low, a thinning tail upward.
    //
    // THREE: AN OROGENIC BELT, per §10.1's instruction to stamp rather than simulate: a linear
    // ridge along a plate-boundary curve with a low-pass "root" under it, and the fluvial stages
    // then carve real drainage through the fake mountain. "The rivers will make the stamps
    // credible; nothing else will."
    constexpr float kContinentScale = 4000.0f; // one or two landmasses in an 8 km field
    constexpr float kReliefScale = 700.0f;
    constexpr float kLandAmplitude = 90.0f;
    constexpr float kOceanDepth = 70.0f;
    constexpr float kBeltAmplitude = 55.0f;
    const FieldGeometry& g = out.geometry();
    std::span<float> elevation = out.plane(Plane::Elevation);
    std::span<float> lithology = out.plane(Plane::Lithology);
    const auto seed = static_cast<std::uint32_t>(params.seed);

    for (std::int32_t cz = 0; cz < g.cells; ++cz) {
        for (std::int32_t cx = 0; cx < g.cells; ++cx) {
            const glm::vec2 w = g.to_world(cx, cz);
            // The continent mask: >0 is land crust, <0 is ocean crust. Two separate fields, which
            // is §10.2(1)'s "separate ocean and land crust" -- and the reason the coastline is a
            // crust boundary rather than a contour of one noise field.
            const float continent = fbm(w.x, w.y, kContinentScale, 3, seed);
            const float relief = fbm(w.x, w.y, kReliefScale, 4, seed + 101u);

            float height = 0.0f;
            if (continent > 0.0f) {
                // Land. The power curve is the hypsometry: `pow(t, 2.2)` maps a uniform-ish noise
                // to a distribution concentrated near zero with a thinning tail, so most land sits
                // near sea level and high ground is rare -- §9.3's land-half shape.
                const float t = std::clamp(continent * 1.6f, 0.0f, 1.0f);
                const float shelf = std::pow(t, 2.2f);
                height = kLandAmplitude * shelf * (0.55f + 0.45f * relief);
            } else {
                // Ocean crust: deeper, and smoother, because the abyssal plain is.
                const float t = std::clamp(-continent * 1.8f, 0.0f, 1.0f);
                height = -kOceanDepth * std::pow(t, 1.4f) * (0.7f + 0.3f * relief);
            }

            // The orogenic belt: a ridge along a sinusoidal plate-boundary curve, with a wide
            // low-pass root that lifts the whole region around it (isostatic-looking, per Part 1
            // §2) and a narrow crest on top.
            const float beltAxis = 0.35f * kContinentScale * std::sin(w.x / (0.6f * kContinentScale));
            const float distance = std::abs(w.y - beltAxis);
            const float root = std::exp(-(distance * distance) / (2.0f * 1400.0f * 1400.0f));
            const float crest = std::exp(-(distance * distance) / (2.0f * 320.0f * 320.0f));
            if (continent > 0.0f) {
                height += kBeltAmplitude * (0.35f * root + 0.65f * crest * (0.6f + 0.4f * relief));
            }

            const std::size_t i = out.index(cx, cz);
            elevation[i] = height + params.sea_level;
            // Lithology: harder rock in the belt's core, softer on the plains. Erodibility reads
            // this in a later goal; writing it here keeps the stage that KNOWS where the belt is
            // as the stage that records it.
            lithology[i] = crest > 0.5f ? 1.0f : 0.0f;
        }
    }
}

// Group AM-B stages 3a-3e. Split into named stages rather than one "erode" call so that
// --field-stages N can stop between them: goal 306's before/after capture is exactly "run
// everything up to diffusion, then run diffusion", and a single stage could not express that.
// Stage 2 of §10.2, and it runs BEFORE the fluvial core on purpose: the precipitation field is
// the rain the incision should be using, so computing it after the erosion would compute a climate
// for a landscape that no longer exists.
void stage_climate(TerrainField& out, const MacroParams&) { compute_climate(out); }

void stage_fill_depressions(TerrainField& out, const MacroParams&) { priority_flood(out); }

void stage_flow(TerrainField& out, const MacroParams&) {
    const FlowNetwork net = build_flow_network(out);
    accumulate_flow(out, net);
}

void stage_incise(TerrainField& out, const MacroParams& params) {
    FluvialParams p;
    p.sea_level = params.sea_level;
    // The network is rebuilt here rather than carried from the previous stage: incision changes
    // elevations, so the receivers it started from are stale by the time it finishes. Rebuilding
    // between incision passes is what lets a valley capture a neighbouring one -- drainage
    // reorganisation, which is a real landscape behaviour and not an implementation detail.
    for (int pass = 0; pass < 4; ++pass) {
        priority_flood(out);
        const FlowNetwork net = build_flow_network(out);
        accumulate_flow(out, net);
        FluvialParams sub = p;
        sub.steps = p.steps / 4;
        incise_stream_power(out, net, sub);
    }
}

void stage_diffuse(TerrainField& out, const MacroParams& params) {
    FluvialParams p;
    p.sea_level = params.sea_level;
    diffuse_hillslopes(out, p);
}

constexpr std::array kStageTable{
    Stage{"continents", &stage_continents},
    Stage{"climate", &stage_climate},
    Stage{"fill_depressions", &stage_fill_depressions},
    Stage{"flow", &stage_flow},
    Stage{"incise", &stage_incise},
    Stage{"diffuse", &stage_diffuse},
    // CLIMATE RUNS TWICE, and the second pass is not redundant. The first has to come before the
    // incision because the incision wants a rain field; but that leaves the shipped precipitation
    // plane keyed to PRE-EROSION topography, while everything downstream (biomes, vegetation) reads
    // it against the post-erosion terrain it can actually see. Measured, on the shipped seed: the
    // same background fraction gives a 32.4:1 land wet/dry ratio on the pre-erosion field and
    // 16.1:1 on the post-erosion one -- a factor of two, from valleys that did not exist when the
    // first pass ran. Research Part 7 notes one orographic pass per erosion checkpoint is standard
    // practice in coupled fastscape work; this is the cheapest honest version of that, and the
    // stage is O(n) so a second pass costs a few milliseconds.
    Stage{"climate_final", &stage_climate},
};

} // namespace

std::span<const Stage> stages() noexcept {
    return kStageTable;
}

void run_pipeline(TerrainField& out, const MacroParams& params, int count,
                  void (*on_stage)(std::string_view, double, void*), void* user) {
    const std::span<const Stage> all = stages();
    const std::size_t limit = count < 0 ? all.size() : std::min(all.size(), static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < limit; ++i) {
        const auto started = std::chrono::steady_clock::now();
        all[i].run(out, params);
        if (on_stage != nullptr) {
            const double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            on_stage(all[i].name, seconds, user);
        }
    }
}


bool recentre_on_land(TerrainField& field, float regionHalfExtent, float minElevation) {
    const std::span<const float> h = field.plane(Plane::Elevation);
    const FieldGeometry& g = field.geometry();
    const auto margin = static_cast<std::int32_t>(std::ceil(regionHalfExtent / g.cell_size));
    if (margin * 2 >= g.cells) {
        return false;
    }

    // "At least regionHalfExtent from any water" is a distance query, and doing it per candidate
    // would be O(cells * margin²). One pass of a separable min-filter over a land mask gives the
    // same answer in O(cells * margin): a cell survives only if every cell within the box is land.
    // A box rather than a disc, deliberately -- the playable region IS a box.
    std::vector<std::uint8_t> land(field.cell_count(), 0);
    for (std::size_t i = 0; i < field.cell_count(); ++i) {
        land[i] = h[i] > minElevation ? 1u : 0u;
    }
    std::vector<std::uint8_t> rows(field.cell_count(), 0);
    for (std::int32_t cz = 0; cz < g.cells; ++cz) {
        for (std::int32_t cx = 0; cx < g.cells; ++cx) {
            std::uint8_t all = 1;
            for (std::int32_t d = -margin; d <= margin && all != 0; ++d) {
                const std::int32_t sx = cx + d;
                all = (sx < 0 || sx >= g.cells) ? 0u : land[field.index(sx, cz)];
            }
            rows[field.index(cx, cz)] = all;
        }
    }
    std::vector<std::uint8_t> ok(field.cell_count(), 0);
    for (std::int32_t cz = 0; cz < g.cells; ++cz) {
        for (std::int32_t cx = 0; cx < g.cells; ++cx) {
            std::uint8_t all = 1;
            for (std::int32_t d = -margin; d <= margin && all != 0; ++d) {
                const std::int32_t sz = cz + d;
                all = (sz < 0 || sz >= g.cells) ? 0u : rows[field.index(cx, sz)];
            }
            ok[field.index(cx, cz)] = all;
        }
    }

    // Nearest to the field centre, so the playable region stays as close to the middle of the
    // eroded field as the terrain allows -- the edges are where the fill's boundary conditions and
    // the flow network's off-field termini live.
    const std::int32_t centre = g.cells / 2;
    std::int32_t bestX = -1;
    std::int32_t bestZ = -1;
    std::int64_t bestD2 = std::numeric_limits<std::int64_t>::max();
    for (std::int32_t cz = 0; cz < g.cells; ++cz) {
        for (std::int32_t cx = 0; cx < g.cells; ++cx) {
            if (ok[field.index(cx, cz)] == 0) {
                continue;
            }
            const std::int64_t dx = cx - centre;
            const std::int64_t dz = cz - centre;
            const std::int64_t d2 = dx * dx + dz * dz;
            if (d2 < bestD2) {
                bestD2 = d2;
                bestX = cx;
                bestZ = cz;
            }
        }
    }
    if (bestX < 0) {
        return false;
    }

    // Shift the origin so the chosen cell is at world (0, 0). Elevations are untouched.
    FieldGeometry moved = g;
    moved.origin_x = -static_cast<float>(bestX) * g.cell_size;
    moved.origin_z = -static_cast<float>(bestZ) * g.cell_size;
    field.set_geometry(moved);
    return true;
}

} // namespace world::generation::field
