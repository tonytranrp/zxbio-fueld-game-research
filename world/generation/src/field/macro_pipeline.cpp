#include "world/generation/field/macro_pipeline.hpp"

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

void stage_continents(TerrainField& out, const MacroParams& params) {
    // Two octaves at the macro scale. The finer octaves of the shipped terrain are NOT baked --
    // they stay analytic in `HeightmapGenerator` as the detail term, because a 16 m cell cannot
    // carry a 25 m feature and baking it would low-pass the world by exactly the amount the
    // sampling theorem says.
    constexpr float kFeatureScale = 200.0f; // the shipped terrain's own, so the character matches
    constexpr float kAmplitude = 64.0f;     // ditto
    const FieldGeometry& g = out.geometry();
    std::span<float> elevation = out.plane(Plane::Elevation);
    const auto seed = static_cast<std::uint32_t>(params.seed);
    for (std::int32_t cz = 0; cz < g.cells; ++cz) {
        for (std::int32_t cx = 0; cx < g.cells; ++cx) {
            const glm::vec2 w = g.to_world(cx, cz);
            const float o1 = value_noise(w.x / kFeatureScale, w.y / kFeatureScale, seed);
            const float o2 =
                value_noise(w.x / (kFeatureScale * 0.5f), w.y / (kFeatureScale * 0.5f), seed + 1u);
            // Amplitudes 1 and 1/2, normalised -- the fBm gain of 0.5 the shipped terrain uses.
            elevation[out.index(cx, cz)] = kAmplitude * (o1 + 0.5f * o2) / 1.5f + params.sea_level;
        }
    }
}

constexpr std::array kStageTable{
    Stage{"continents", &stage_continents},
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

} // namespace world::generation::field
