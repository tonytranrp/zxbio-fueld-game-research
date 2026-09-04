#include "world/generation/heightmap_generator.hpp"

#include <stdexcept>

#include <FastNoise/FastNoise.h>

namespace world::generation {

namespace {

// Tunable terrain-shape constants, named rather than left as magic numbers scattered in
// build_terrain_noise() (M1_2_BRIEF.md §2.3: "worth exposing as a tunable constant, not
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

// Pinned to one explicit, low SIMD level deliberately (M1_2_BRIEF.md §2.5): world generation must
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

struct HeightmapGenerator::Impl {
    FastNoise::SmartNode<> root;
    int seed;
};

HeightmapGenerator::HeightmapGenerator(int seed) : impl_(std::make_unique<Impl>(Impl{build_terrain_noise(), seed})) {}

HeightmapGenerator::~HeightmapGenerator() = default;
HeightmapGenerator::HeightmapGenerator(HeightmapGenerator&&) noexcept = default;
HeightmapGenerator& HeightmapGenerator::operator=(HeightmapGenerator&&) noexcept = default;

HeightmapMinMax HeightmapGenerator::generate_column_heights(std::int32_t worldXOffset, std::int32_t worldZOffset,
                                                              std::int32_t width, std::int32_t depth,
                                                              float* outHeights) const {
    // FastNoise2's 2D grid has no notion of "X/Z" -- its own two axes are used here to carry our
    // world's horizontal plane (X, Z), Y being height. Output layout is row-major, X innermost:
    // out[y * xCount + x] (confirmed directly from Generator.h's own doc comment).
    const FastNoise::OutputMinMax minMax =
        impl_->root->GenUniformGrid2D(outHeights, static_cast<float>(worldXOffset), static_cast<float>(worldZOffset),
                                       width, depth, 1.0f, 1.0f, impl_->seed);
    return HeightmapMinMax{minMax.min, minMax.max};
}

} // namespace world::generation
