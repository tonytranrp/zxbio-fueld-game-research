#include "world/generation/heightmap_generator.hpp"

#include <stdexcept>

#include "world/generation/field/field_sampler.hpp"

#include <FastNoise/FastNoise.h>

namespace world::generation {

namespace {

// Tunable terrain-shape constants, named rather than left as magic numbers scattered in
// build_terrain_noise() (M1.2 brief §2.3: "worth exposing as a tunable constant, not
// hardcoding a specific count as gospel").
constexpr float kFeatureScale = 200.0f; // world units per noise period
constexpr int kOctaveCount = 4;
constexpr float kLacunarity = 2.0f;
constexpr float kGain = 0.5f;

// FastNoise2's Simplex/FractalFBm output lands in roughly [-1, 1] -- a Remap node converts that
// into an actual world-Y surface height range in one place, inside the node tree itself (kept
// here rather than as a manual post-multiply in terrain_fill.cpp, matching §2's framing that the
// node tree IS the concrete mechanism for terrain shape, "mountains" included).
constexpr float kNoiseOutputMin = -1.0f;
constexpr float kNoiseOutputMax = 1.0f;
constexpr float kBaseHeight = 0.0f;
constexpr float kAmplitude = 64.0f;

// Pinned to one explicit, low SIMD level deliberately (M1.2 brief §2.5): world generation must
// be bit-identical for a given seed regardless of which CPU/SIMD level runs it, and pinning avoids
// ISA-dependent instructions (FMA etc.) that could introduce last-bit rounding differences across
// machines or across FastSIMD's own runtime auto-dispatch. SCALAR would be the strictest choice,
// but this project's FastNoise2 build only compiles SSE2/SSE41/AVX2/AVX512 (confirmed directly
// from the Phase 0 build log -- SCALAR is absent), so FastNoise::New<T> returns null for it --
// SSE2 is the actual lowest compiled level, and is still a safe universal choice for this
// x86-64-only project (SSE2 is mandatory baseline on every x86-64 CPU, unlike SSE41/AVX2/AVX512).
constexpr FastSIMD::FeatureSet kPinnedFeatureSet = FastSIMD::FeatureSet::SSE2;

template <typename T>
FastNoise::SmartNode<T> new_pinned_node() {
    FastNoise::SmartNode<T> node = FastNoise::New<T>(kPinnedFeatureSet);
    if (!node) {
        // A system-boundary check, not defensive clutter: this only fires if FastNoise2 is ever
        // rebuilt without kPinnedFeatureSet compiled in, which New<T>'s own contract says returns
        // null for rather than failing loudly -- worth failing loudly here instead of segfaulting
        // on the first dereference.
        throw std::runtime_error("FastNoise2 was not compiled with the pinned SIMD feature set");
    }
    return node;
}

FastNoise::SmartNode<> build_terrain_noise() {
    auto simplex = new_pinned_node<FastNoise::Simplex>();
    simplex->SetScale(kFeatureScale);

    auto fractal = new_pinned_node<FastNoise::FractalFBm>();
    fractal->SetSource(simplex);
    fractal->SetOctaveCount(kOctaveCount);
    fractal->SetLacunarity(kLacunarity);
    fractal->SetGain(kGain);

    auto remap = new_pinned_node<FastNoise::Remap>();
    remap->SetSource(fractal);
    remap->SetFromMin(kNoiseOutputMin);
    remap->SetFromMax(kNoiseOutputMax);
    remap->SetToMin(kBaseHeight - kAmplitude);
    remap->SetToMax(kBaseHeight + kAmplitude);

    return remap;
}

} // namespace

// Prompt 006 goal 297: the DETAIL half of the split. The macro field carries everything at and
// above the 16 m cell size; this carries what a 16 m grid provably cannot, and it is analytic
// because there is nowhere to bake it that would not be the same noise call again.
//
// The FIRST version of this was two octaves at 50 m and 25 m at gain 0.5, remapped to +-16 m,
// chosen to reproduce "the two the shipped 4-octave stack had below the macro pair". That reasoning
// was about matching the OLD stack's octaves, and goal 320 measured what it actually produced:
//
//   * spectral beta 0.50 against a band of [1.6, 2.5], with log-log R² 0.54 -- the surface has a
//     BREAK where the macro's roll-off meets the detail's band and is not a power law at all
//   * variogram Hurst 0.129 against [0.46, 0.77], the same cause seen by a second instrument
//   * mean land slope 42.6 degrees, because a 25 m octave carrying ~5 m of amplitude has a gradient
//     of 2*pi*5/25 = 1.26, which is 51 degrees -- and nothing at all existed below 25 m, so the
//     world was perfectly smooth under it and cliff-like just above
//
// The parameters are `DetailParams` now, defaulted to the values `test_detail_spectrum.cpp` SWEPT
// and selected against the acceptance suite rather than reasoned toward. Two changes matter:
//
//   * gain 0.71 rather than 0.5. With lacunarity 2 the fractal's Hurst exponent is H = -log2(gain),
//     so 0.5 gives H = 1 (beta 3, far too smooth per octave) and 0.71 gives H = 0.5 -- beta = 2,
//     which is research §9.2's target value exactly.
//   * five octaves from 32 m rather than two from 50 m. 32 m is the finest wavelength a 16 m macro
//     grid can reconstruct, so the two terms JOIN there instead of overlapping and beating; and
//     five octaves carry the surface down to 2 m instead of stopping dead at 25 m.

FastNoise::SmartNode<> build_detail_noise(const DetailParams& p) {
    auto simplex = new_pinned_node<FastNoise::Simplex>();
    simplex->SetScale(p.scale_m);
    auto fractal = new_pinned_node<FastNoise::FractalFBm>();
    fractal->SetSource(simplex);
    fractal->SetOctaveCount(std::max(1, p.octaves));
    fractal->SetLacunarity(p.lacunarity);
    fractal->SetGain(p.gain);
    // THE REMAP MUST DIVIDE BY THE OCTAVE WEIGHT SUM, and this is a real bug that predates the
    // sweep. A FractalFBm here is NOT normalised: an N-octave stack at gain g spans roughly
    // +-sum(g^i), not +-1. Mapping [-1, 1] onto [-A, A] therefore delivers A * sum(g^i), which for
    // five octaves at gain 0.71 is 2.82x the amplitude the constant asks for.
    //
    // MEASURED, which is how it was found: the surface's semivariogram read gamma(7.8 m) = 16.6 m²
    // -- a 5.8 m RMS height change over 7.8 m of ground, a 37-degree slope EVERYWHERE -- against
    // gamma(500 m) = 44.3 m². The detail term was drowning the macro field at every scale below a
    // kilometre, which is why both the spectral and variogram instruments read the surface as
    // uncorrelated no matter what the octave structure was changed to.
    //
    // The same error is present in `build_terrain_noise` and always was: four octaves at gain 0.5 is
    // a weight sum of 1.875, so its stated 64 m amplitude has always delivered ~120 m. That matches
    // the ~112 m relief this world has been documented as having, which is the confirmation.
    float weightSum = 0.0f;
    float w = 1.0f;
    for (int i = 0; i < std::max(1, p.octaves); ++i) {
        weightSum += w;
        w *= p.gain;
    }
    auto remap = new_pinned_node<FastNoise::Remap>();
    remap->SetSource(fractal);
    remap->SetFromMin(-weightSum);
    remap->SetFromMax(weightSum);
    remap->SetToMin(-p.amplitude_m);
    remap->SetToMax(p.amplitude_m);
    return remap;
}

struct HeightmapGenerator::Impl {
    FastNoise::SmartNode<> root;   // the whole analytic field, when there is no macro field
    FastNoise::SmartNode<> detail; // the high-frequency half, when there is
    std::shared_ptr<const field::TerrainField> macro;
    int seed;
};

HeightmapGenerator::HeightmapGenerator(int seed)
    : impl_(std::make_unique<Impl>(Impl{build_terrain_noise(), nullptr, nullptr, seed})) {}

HeightmapGenerator::HeightmapGenerator(int seed, std::shared_ptr<const field::TerrainField> macro,
                                       const DetailParams& detail)
    : impl_(std::make_unique<Impl>(
          Impl{build_terrain_noise(), build_detail_noise(detail), std::move(macro), seed})) {}

const field::TerrainField* HeightmapGenerator::macro_field() const noexcept {
    return impl_->macro.get();
}

HeightmapGenerator::~HeightmapGenerator() = default;
HeightmapGenerator::HeightmapGenerator(HeightmapGenerator&&) noexcept = default;
HeightmapGenerator& HeightmapGenerator::operator=(HeightmapGenerator&&) noexcept = default;

HeightmapMinMax HeightmapGenerator::generate_column_heights(std::int32_t worldXOffset,
                                                            std::int32_t worldZOffset, std::int32_t width,
                                                            std::int32_t depth, float* outHeights) const {
    // FastNoise2's 2D grid has no notion of "X/Z" -- its own two axes are used here to carry our
    // world's horizontal plane (X, Z), Y being height. Output layout is row-major, X innermost:
    // out[y * xCount + x] (confirmed directly from Generator.h's own doc comment).
    const FastNoise::OutputMinMax minMax = impl_->root->GenUniformGrid2D(
        outHeights, static_cast<float>(worldXOffset), static_cast<float>(worldZOffset), width, depth, 1.0f,
        1.0f, impl_->seed);
    return HeightmapMinMax{minMax.min, minMax.max};
}

HeightmapMinMax HeightmapGenerator::generate_column_heights_spaced(float xStart, float zStart,
                                                                   std::int32_t width, std::int32_t depth,
                                                                   float step, float* outHeights) const {
    // THE BULK PATH MUST AGREE WITH `height_at`, and for the whole of Group AM-B it did not.
    //
    // This function fills every brick and every column -- it is where essentially all of the
    // world's geometry comes from -- and it called the raw four-octave noise root, ignoring the
    // macro field completely. `height_at` read macro + detail. So the world that was RENDERED and
    // the world that was COLLIDED WITH were two different surfaces, and every acceptance statistic
    // in this pass was measured on the one nobody could see.
    //
    // It was caught by the rule that a visual change is verified by a VIEWED capture: after the
    // detail retune put mean slope at 4.8 degrees, the rendered frame still showed near-vertical
    // spires. Nothing in the numbers said so, because the numbers were reading `height_at`.
    if (impl_->macro == nullptr) {
        const FastNoise::OutputMinMax minMax =
            impl_->root->GenUniformGrid2D(outHeights, xStart, zStart, width, depth, step, step, impl_->seed);
        return HeightmapMinMax{minMax.min, minMax.max};
    }

    // The detail term in bulk -- one grid call, the same as before -- then the macro field added
    // per cell. The min/max has to be RECOMPUTED rather than taken from the grid call, because the
    // macro shifts every cell by a different amount and the detail's own extremes are not the
    // sum's. (`HeightField::range` is built on this return value and uses it to reject boxes, so a
    // wrong bound here is a hole in the world, not a cosmetic error.)
    impl_->detail->GenUniformGrid2D(outHeights, xStart, zStart, width, depth, step, step, impl_->seed);
    const field::FieldSampler macro{*impl_->macro, field::Plane::Elevation};
    float lo = std::numeric_limits<float>::max();
    float hi = std::numeric_limits<float>::lowest();
    for (std::int32_t z = 0; z < depth; ++z) {
        const float worldZ = zStart + static_cast<float>(z) * step;
        for (std::int32_t x = 0; x < width; ++x) {
            float& h = outHeights[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                                  static_cast<std::size_t>(x)];
            h += macro.value_at(xStart + static_cast<float>(x) * step, worldZ);
            lo = std::min(lo, h);
            hi = std::max(hi, h);
        }
    }
    if (width <= 0 || depth <= 0) {
        return HeightmapMinMax{0.0f, 0.0f};
    }
    return HeightmapMinMax{lo, hi};
}

float HeightmapGenerator::height_at(float worldX, float worldZ) const {
    if (impl_->macro == nullptr) {
        return impl_->root->GenSingle2D(worldX, worldZ, impl_->seed);
    }
    // macro + detail, both always present. See the header for why this is a SUM and not a
    // fallback: a term that is absent until something is baked makes the world's shape depend on
    // where the player has been.
    const field::FieldSampler macro{*impl_->macro, field::Plane::Elevation};
    return macro.value_at(worldX, worldZ) + impl_->detail->GenSingle2D(worldX, worldZ, impl_->seed);
}

glm::vec2 HeightmapGenerator::slope_at(float worldX, float worldZ) const {
    // Without a macro field there is nothing analytic to differentiate, so this falls back to a
    // central difference at 1 m -- which is what TerrainSampler already did, kept here so the two
    // paths answer the same question rather than differing by which constructor was used.
    if (impl_->macro == nullptr) {
        constexpr float kEps = 0.5f;
        const float dx = height_at(worldX + kEps, worldZ) - height_at(worldX - kEps, worldZ);
        const float dz = height_at(worldX, worldZ + kEps) - height_at(worldX, worldZ - kEps);
        return glm::vec2{dx / (2.0f * kEps), dz / (2.0f * kEps)};
    }
    const field::FieldSampler macro{*impl_->macro, field::Plane::Elevation};
    const glm::vec2 macroGradient = macro.gradient_at(worldX, worldZ);
    // The detail term has no closed-form derivative here (it is inside FastNoise2), so its
    // contribution is a central difference -- at 1 m, which is NOT an arbitrary choice: CLAUDE.md
    // records that the walkable-slope decision already reads "the ANALYTIC heightfield by central
    // difference at 1 m", so this is the existing, documented behaviour kept rather than a new
    // number introduced in passing.
    //
    // A first version used 6.25 m ("half the finest detail wavelength"), reasoning from the signal
    // rather than from the consumer. It is the wrong scale: a slope query asks "is this walkable",
    // which is a question about the metre the body occupies, and 6.25 m smoothed away most of the
    // detail term's contribution -- measured as a 0.9 disagreement with a central difference of
    // height_at over the same ground.
    constexpr float kDetailEps = 1.0f;
    const float dx = impl_->detail->GenSingle2D(worldX + kDetailEps, worldZ, impl_->seed) -
                     impl_->detail->GenSingle2D(worldX - kDetailEps, worldZ, impl_->seed);
    const float dz = impl_->detail->GenSingle2D(worldX, worldZ + kDetailEps, impl_->seed) -
                     impl_->detail->GenSingle2D(worldX, worldZ - kDetailEps, impl_->seed);
    return macroGradient + glm::vec2{dx / (2.0f * kDetailEps), dz / (2.0f * kDetailEps)};
}

} // namespace world::generation
