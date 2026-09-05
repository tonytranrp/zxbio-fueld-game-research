#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "world/generation/heightmap_generator.hpp"

// TERRAIN_FIXES_BRIEF Group R task 8: the direct diagnostic separating the two corrugation
// hypotheses. If the tight ripple exists in the RAW noise output, the bug is frequency/unit
// scale (task 9); if the raw heightmap is smooth at the intended wavelength, the bug is in
// meshing/normals (task 10). Expected wavelength, stated BEFORE looking (per the task's own
// rule): kFeatureScale=200 world units for the base octave; the highest of 4 octaves at
// lacunarity 2 is 200/8 = 25 units, contributing amplitude 64 * 0.5^3 = 8 units -- worst-case
// slope of the full fBm sum is bounded by sum_i amp_i * 2*pi/lambda_i = 4 * (64*2*pi/200)
// ~= 8.0 units per 1-unit step, and typical slopes far lower. A frequency/scale bug (features
// per voxel instead of per 200 voxels) would swing tens of units per step.
TEST_CASE("Raw heightmap varies at the intended feature scale, not per-voxel", "[generation][smoothness]") {
    const world::generation::HeightmapGenerator generator(1337);

    constexpr int kSamples = 512;
    std::vector<float> heights(static_cast<std::size_t>(kSamples));
    (void)generator.generate_column_heights(0, 0, kSamples, 1, heights.data());

    float maxStep = 0.0f;
    double sumAbsStep = 0.0;
    float minH = heights[0];
    float maxH = heights[0];
    for (int i = 1; i < kSamples; ++i) {
        const float step =
            std::abs(heights[static_cast<std::size_t>(i)] - heights[static_cast<std::size_t>(i - 1)]);
        maxStep = std::max(maxStep, step);
        sumAbsStep += step;
        minH = std::min(minH, heights[static_cast<std::size_t>(i)]);
        maxH = std::max(maxH, heights[static_cast<std::size_t>(i)]);
    }
    const double meanStep = sumAbsStep / (kSamples - 1);

    // Dump the profile for direct eyeballing (task 8's check is a human-inspectable artifact,
    // not just the assertions): one CSV row per sample in the build directory.
    std::FILE* csv = nullptr;
#if defined(_MSC_VER)
    (void)fopen_s(&csv, "heightmap_profile.csv", "w"); // fopen itself is C4996 under /W4 /WX
#else
    csv = std::fopen("heightmap_profile.csv", "w");
#endif
    if (csv != nullptr) {
        std::fprintf(csv, "x,height\n");
        for (int i = 0; i < kSamples; ++i) {
            std::fprintf(csv, "%d,%.4f\n", i, static_cast<double>(heights[static_cast<std::size_t>(i)]));
        }
        std::fclose(csv);
    }
    std::printf("heightmap profile over %d units: range [%.1f, %.1f], max 1-unit step %.3f, mean %.3f "
                "(dumped to heightmap_profile.csv)\n",
                kSamples, static_cast<double>(minH), static_cast<double>(maxH), static_cast<double>(maxStep),
                meanStep);

    // The hypothesis separator: an intended-scale heightmap stays under the analytic slope bound;
    // a per-voxel-frequency bug produces steps comparable to the full amplitude.
    CHECK(maxStep < 10.0f);
    // And it must not be flat (which would mean the generator or offsets broke differently).
    CHECK(maxH - minH > 10.0f);
}
