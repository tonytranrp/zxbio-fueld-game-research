// svo_render: the CPU REFERENCE renderer for the sparse-brick octree (docs/goals.md Group W).
// Builds the same tree the app builds (same TerrainSampler, same build_tree, same LOD rule),
// marches every pixel with the same trace_ray the HLSL shader mirrors, shades with the same sky /
// sun / fog palette, and writes a PNG -- so the micro-voxel world can be LOOKED AT before a single
// GPU line exists, and so a GPU frame at the same --pos/--yaw/--pitch has a ground truth to diff
// against. Runs in the GPU-less CI jobs (no Diligent), with --verify applying the app's own
// local-contrast metric.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "engine/core/math.hpp"
#include "engine/jobs/thread_pool.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/materials/materials.hpp"
#include "world/svo/brick_tree.hpp"
#include "world/svo/ray_trace.hpp"
#include "world/svo/terrain_sampler.hpp"
#include "world/svo/tree_builder.hpp"

#include "../png_writer.hpp"

namespace {

using world::chunk::MaterialID;
using namespace world::svo;

struct Options {
    int seed = 1337;
    int voxel_log2 = -7; // 7.8 mm
    int root_log2 = 9;   // 512 m
    float lod_radius = 4.0f;
    // Default pose: hovering above the ~65 m summit near the origin, looking down the -Z valley
    // toward the sea -- chosen by looking at real renders (a ground-level pose here stares into a
    // slope half a meter away), not guessed.
    glm::vec3 pos{12.0f, 82.0f, 24.0f};
    float yaw_deg = 0.0f;
    float pitch_deg = -22.0f;
    bool pos_set = false;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    float fov_deg = 70.0f;
    bool trees = true;
    bool shadows = true;
    bool ao = true;
    bool grain = true;
    bool verify = false;
    bool lod_march = true;
    // Group Z knobs (the app's defaults; see SvoRenderer::Settings).
    float smooth_pixels = 6.0f;
    float grain_amplitude = 0.10f;
    float ao_radius_px = 32.0f;
    float shadow_lod = 4.0f;
    float ao_lod = 8.0f;
    // --lod-center: build the tree's LOD around a point OTHER than the camera -- how the app looks
    // after the camera has moved away from the last build center (goal 164's repro).
    glm::vec3 lod_center{0.0f};
    bool lod_center_set = false;
    // --view: one shading term instead of the shaded color (mirrors --debug-view in the app).
    std::string view;
    std::string out = "svo_render.png";
    unsigned threads = std::max(1u, std::thread::hardware_concurrency());
};

bool parse(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        if (a == "--seed") {
            o.seed = std::atoi(next());
        } else if (a == "--voxel-log2") {
            o.voxel_log2 = std::atoi(next());
        } else if (a == "--root-log2") {
            o.root_log2 = std::atoi(next());
        } else if (a == "--lod-radius") {
            o.lod_radius = static_cast<float>(std::atof(next()));
        } else if (a == "--pos") {
            const char* v = next();
#if defined(_MSC_VER)
            if (v == nullptr || sscanf_s(v, "%f,%f,%f", &o.pos.x, &o.pos.y, &o.pos.z) != 3) {
#else
            if (v == nullptr || std::sscanf(v, "%f,%f,%f", &o.pos.x, &o.pos.y, &o.pos.z) != 3) {
#endif
                std::fprintf(stderr, "--pos expects x,y,z\n");
                return false;
            }
            o.pos_set = true;
        } else if (a == "--xz") {
            const char* v = next();
#if defined(_MSC_VER)
            if (v == nullptr || sscanf_s(v, "%f,%f", &o.pos.x, &o.pos.z) != 2) {
#else
            if (v == nullptr || std::sscanf(v, "%f,%f", &o.pos.x, &o.pos.z) != 2) {
#endif
                std::fprintf(stderr, "--xz expects x,z (eye height is derived from the terrain)\n");
                return false;
            }
            o.pos_set = false;
        } else if (a == "--yaw") {
            o.yaw_deg = static_cast<float>(std::atof(next()));
        } else if (a == "--pitch") {
            o.pitch_deg = static_cast<float>(std::atof(next()));
        } else if (a == "--size") {
            const char* v = next();
#if defined(_MSC_VER)
            if (v == nullptr || sscanf_s(v, "%ux%u", &o.width, &o.height) != 2) {
#else
            if (v == nullptr || std::sscanf(v, "%ux%u", &o.width, &o.height) != 2) {
#endif
                std::fprintf(stderr, "--size expects WxH\n");
                return false;
            }
        } else if (a == "--fov") {
            o.fov_deg = static_cast<float>(std::atof(next()));
        } else if (a == "--no-trees") {
            o.trees = false;
        } else if (a == "--no-shadows") {
            o.shadows = false;
        } else if (a == "--no-ao") {
            o.ao = false;
        } else if (a == "--no-grain") {
            o.grain = false;
        } else if (a == "--no-lod-march") {
            o.lod_march = false;
        } else if (a == "--smooth-pixels") {
            o.smooth_pixels = static_cast<float>(std::atof(next()));
        } else if (a == "--grain") {
            o.grain_amplitude = static_cast<float>(std::atof(next()));
        } else if (a == "--ao-radius") {
            o.ao_radius_px = static_cast<float>(std::atof(next()));
        } else if (a == "--shadow-lod") {
            o.shadow_lod = static_cast<float>(std::atof(next()));
        } else if (a == "--lod-center") {
            const char* v = next();
#if defined(_MSC_VER)
            if (v == nullptr ||
                sscanf_s(v, "%f,%f,%f", &o.lod_center.x, &o.lod_center.y, &o.lod_center.z) != 3) {
#else
            if (v == nullptr ||
                std::sscanf(v, "%f,%f,%f", &o.lod_center.x, &o.lod_center.y, &o.lod_center.z) != 3) {
#endif
                std::fprintf(stderr, "--lod-center expects x,y,z\n");
                return false;
            }
            o.lod_center_set = true;
        } else if (a == "--view") {
            o.view = next();
        } else if (a == "--verify") {
            o.verify = true;
        } else if (a == "--out") {
            o.out = next();
        } else if (a == "--threads") {
            o.threads = static_cast<unsigned>(std::max(1, std::atoi(next())));
        } else {
            std::fprintf(
                stderr,
                "unknown argument %s (known: --seed N --voxel-log2 N --root-log2 N --lod-radius M --pos "
                "x,y,z --xz x,z --yaw D --pitch D --size WxH --fov D --no-trees --no-shadows --no-ao "
                "--no-grain --no-lod-march --smooth-pixels N --grain A --ao-radius PX --shadow-lod M "
                "--lod-center x,y,z --view "
                "lit|ao|normal|facenormal|level|steps|coverage|cubepx|smooth|lodcube "
                "--verify --out file.png --threads N)\n",
                argv[i]);
            return false;
        }
    }
    return true;
}

// ---- shading: the same palette as render/diligent/shaders/sky_common.fxh + terrain.psh.hlsl ----
const glm::vec3 kSunDirection = glm::normalize(glm::vec3{0.4f, -1.0f, 0.25f}); // toward surfaces
const glm::vec3 kSunColor{1.05f, 0.95f, 0.78f};
const glm::vec3 kSkyZenith{0.30f, 0.52f, 0.80f};
const glm::vec3 kSkyHorizon{0.78f, 0.83f, 0.88f};
const glm::vec3 kSkyBelowHorizon{0.50f, 0.56f, 0.63f};
const glm::vec3 kSkyAmbient{0.34f, 0.33f, 0.30f};
const glm::vec3 kGroundAmbient{0.14f, 0.15f, 0.19f};

glm::vec3 sky_gradient(const glm::vec3& dir) {
    const float t = std::pow(std::clamp(dir.y, 0.0f, 1.0f), 0.45f);
    const glm::vec3 sky = glm::mix(kSkyHorizon, kSkyZenith, t);
    return glm::mix(sky, kSkyBelowHorizon, std::clamp(-dir.y * 3.0f, 0.0f, 1.0f));
}

glm::vec3 sky_radiance(const glm::vec3& dir) {
    const float toSun = std::clamp(glm::dot(dir, -kSunDirection), 0.0f, 1.0f);
    return sky_gradient(dir) + kSunColor * (3.0f * std::pow(toSun, 4096.0f) + 0.25f * std::pow(toSun, 64.0f));
}

float hash2(const glm::vec2& p) {
    const float s = std::sin(glm::dot(p, glm::vec2{127.1f, 311.7f})) * 43758.5453f;
    return s - std::floor(s);
}

float hash3(const glm::vec3& p) {
    const float s = std::sin(glm::dot(p, glm::vec3{127.1f, 311.7f, 74.7f})) * 43758.5453f;
    return s - std::floor(s);
}

// svo_march.psh.hlsl's LevelColor: a repeating 6-hue ramp so adjacent levels contrast.
glm::vec3 level_color(int level) {
    const float h = (static_cast<float>(level) / 6.0f - std::floor(static_cast<float>(level) / 6.0f)) * 6.0f;
    const glm::vec3 c =
        glm::clamp(glm::vec3{std::abs(h - 3.0f) - 1.0f, 2.0f - std::abs(h - 2.0f), 2.0f - std::abs(h - 4.0f)},
                   0.0f, 1.0f);
    const float parity = static_cast<float>(level % 2);
    return c * (0.55f + 0.45f * parity);
}

float value_noise(const glm::vec2& p) {
    const glm::vec2 cell = glm::floor(p);
    const glm::vec2 f = p - cell;
    const glm::vec2 u = f * f * (3.0f - 2.0f * f);
    const float a = hash2(cell);
    const float b = hash2(cell + glm::vec2{1.0f, 0.0f});
    const float c = hash2(cell + glm::vec2{0.0f, 1.0f});
    const float d = hash2(cell + glm::vec2{1.0f, 1.0f});
    return glm::mix(glm::mix(a, b, u.x), glm::mix(c, d, u.x), u.y);
}

// Soft-knee tonemap matching composite.psh.hlsl's intent closely enough for a diagnostic image.
glm::vec3 tonemap(const glm::vec3& c) {
    return c / (c + glm::vec3{1.0f}) * 1.35f;
}

std::uint8_t to_srgb8(float linear) {
    const float c = std::clamp(linear, 0.0f, 1.0f);
    const float s = c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<std::uint8_t>(std::clamp(s * 255.0f + 0.5f, 0.0f, 255.0f));
}

} // namespace

int main(int argc, char** argv) {
    Options o;
    if (!parse(argc, argv, o)) {
        return EXIT_FAILURE;
    }
    const world::generation::HeightmapGenerator heightmap(o.seed);
    if (!o.pos_set) {
        // Standing on the ground -- or, when the column is under the sea, on the water surface.
        o.pos.y = std::max(heightmap.height_at(o.pos.x, o.pos.z), 0.0f) + 1.7f;
    }
    std::printf("surface heights around the camera:");
    for (int iz = -2; iz <= 2; ++iz) {
        const float dz = 16.0f * static_cast<float>(iz);
        std::printf("\n  z=%+6.1f:", static_cast<double>(o.pos.z + dz));
        for (int ix = -2; ix <= 2; ++ix) {
            const float dx = 16.0f * static_cast<float>(ix);
            std::printf(" %6.1f", static_cast<double>(heightmap.height_at(o.pos.x + dx, o.pos.z + dz)));
        }
    }
    std::printf("\n");

    // Root cube: XZ-centered on the camera (snapped to 8 m so rebuilds keep voxel alignment); Y
    // covers [8 - half, 8 + half): this terrain spans [-64, 64] m plus ~15 m of trees, so a 128 m
    // root keeps every hilltop and tree while only clipping the deepest (never visible) sea floor.
    TreeGeometry g;
    g.root_size_log2 = o.root_log2;
    g.voxel_size_log2 = o.voxel_log2;
    const float half = g.root_edge() * 0.5f;
    g.origin = glm::vec3{std::floor((o.pos.x - half) / 8.0f) * 8.0f, 8.0f - half,
                         std::floor((o.pos.z - half) / 8.0f) * 8.0f};

    TerrainSamplerParams sp;
    sp.seed = o.seed;
    sp.trees = o.trees;
    const Box region{g.origin, g.max_corner()};
    const auto samplerStart = std::chrono::steady_clock::now();
    TerrainSampler sampler(heightmap, sp, region);
    const double samplerSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - samplerStart).count();

    BuildParams bp;
    bp.lod_center = o.lod_center_set ? o.lod_center : o.pos;
    bp.lod_radius = o.lod_radius;
    engine::jobs::ThreadPool pool(o.threads);
    sampler.set_focus(bp.lod_center, 4.0f * o.lod_radius);
    BuildStats stats;
    const BrickTree tree = build_tree(sampler, g, bp, &pool, &stats);
    const BrickTree::Stats ts = tree.stats();
    std::printf(
        "tree: root %g m, voxel %g mm, %zu bricks, %zu internal, %zu solid, %.1f MB, deepest level %d/%d; "
        "sampler %.2fs, build %.2fs (%zu classified in %.2fs CPU, %zu bricks sampled in %.2fs CPU, %zu "
        "trees)\n",
        static_cast<double>(g.root_edge()), static_cast<double>(g.finest_voxel_edge()) * 1000.0,
        ts.brick_leaves, ts.internal_nodes, ts.solid_leaves, static_cast<double>(tree.memory_bytes()) / 1.0e6,
        ts.deepest_level, g.max_brick_level(), samplerSeconds, stats.seconds, stats.boxes_classified,
        stats.classify_seconds, stats.bricks_sampled, stats.fill_seconds, sampler.trees().size());
    std::printf("noise grid calls: %llu (column-cache hits %llu)\n",
                static_cast<unsigned long long>(TerrainSampler::debug_grid_calls()),
                static_cast<unsigned long long>(TerrainSampler::debug_grid_cache_hits()));
    std::printf("bricks per level (voxel edge): kept / sampled (empty, solid):\n");
    for (int level = 0; level <= g.max_brick_level(); ++level) {
        const auto lv = static_cast<std::size_t>(level);
        if (stats.sampled_per_level[lv] > 0) {
            std::printf("  L%d (%.4g m): %zu / %zu (%zu empty, %zu solid)\n", level,
                        static_cast<double>(g.level_voxel_edge(level)), ts.bricks_per_level[lv],
                        stats.sampled_per_level[lv], stats.empty_per_level[lv], stats.solid_per_level[lv]);
        }
    }

    std::printf("camera (%.2f, %.2f, %.2f): surface %.2f, tree material at camera %d (leaf level %d)\n",
                static_cast<double>(o.pos.x), static_cast<double>(o.pos.y), static_cast<double>(o.pos.z),
                static_cast<double>(heightmap.height_at(o.pos.x, o.pos.z)),
                static_cast<int>(tree.material_at(o.pos)), tree.leaf_level_at(o.pos));
    std::printf("column under the camera (y: material/level):");
    for (int iy = 1; iy <= 12; ++iy) {
        const glm::vec3 p = o.pos - glm::vec3{0.0f, 0.5f * static_cast<float>(iy), 0.0f};
        std::printf(" %.1f:%d/%d", static_cast<double>(p.y), static_cast<int>(tree.material_at(p)),
                    tree.leaf_level_at(p));
    }
    {
        Ray down;
        down.origin = o.pos;
        down.dir = glm::vec3{0.0f, -1.0f, 0.0f};
        const Hit h = trace_ray(tree, down);
        const Hit b = trace_ray_brute_force(tree, down);
        std::printf(
            "\nstraight-down ray: hit=%d t=%.3f material=%d level=%d steps=%u | brute force hit=%d t=%.3f "
            "material=%d\n",
            h.hit ? 1 : 0, static_cast<double>(h.t), static_cast<int>(h.material), h.level, h.steps,
            b.hit ? 1 : 0, static_cast<double>(b.t), static_cast<int>(b.material));
    }

    // Camera: the app's spectator conventions (yaw about +Y, 0 = -Z; pitch about local +X).
    const glm::quat orientation = glm::angleAxis(glm::radians(o.yaw_deg), glm::vec3{0.0f, 1.0f, 0.0f}) *
                                  glm::angleAxis(glm::radians(o.pitch_deg), glm::vec3{1.0f, 0.0f, 0.0f});
    const glm::vec3 forward = orientation * glm::vec3{0.0f, 0.0f, -1.0f};
    const glm::vec3 right = orientation * glm::vec3{1.0f, 0.0f, 0.0f};
    const glm::vec3 up = orientation * glm::vec3{0.0f, 1.0f, 0.0f};
    const float tanHalf = std::tan(glm::radians(o.fov_deg) * 0.5f);
    const float aspect = static_cast<float>(o.width) / static_cast<float>(o.height);
    // One pixel's angular size: the LOD early-out threshold (same formula the GPU uses).
    const float rawPixelAngle = 2.0f * tanHalf / static_cast<float>(o.height);
    TraceParams primary;
    primary.lod_pixel_angle = o.lod_march ? rawPixelAngle : 0.0f;
    primary.smooth_pixel_angle = o.smooth_pixels * rawPixelAngle;
    // Secondary rays (svo_march.psh.hlsl's rule, goal 164): LOD by distance from THEIR origin
    // (t_offset stays 0), coarser multipliers, no smoothing.
    constexpr float kSecondaryCoverage = 0.35f; // svo_march.psh.hlsl's kSecondaryCoverage
    TraceParams shadow;
    shadow.lod_pixel_angle = primary.lod_pixel_angle * o.shadow_lod;
    shadow.lod_coverage_threshold = kSecondaryCoverage;
    TraceParams aoParams;
    aoParams.lod_pixel_angle = primary.lod_pixel_angle * o.ao_lod;
    aoParams.lod_coverage_threshold = kSecondaryCoverage;
    const std::string_view view = o.view;
    if (!view.empty() && view != "lit" && view != "ao" && view != "normal" && view != "facenormal" &&
        view != "level" && view != "steps" && view != "coverage" && view != "cubepx" && view != "smooth" &&
        view != "lodcube" && view != "material" && view != "distance") {
        std::fprintf(stderr, "unknown --view %s\n", o.view.c_str());
        return EXIT_FAILURE;
    }

    {
        // Center-pixel ray: marcher vs brute force, to separate "wrong rays" from "wrong marcher".
        Ray center;
        center.origin = o.pos;
        center.dir = forward;
        const Hit h = trace_ray(tree, center);
        const Hit b = trace_ray_brute_force(tree, center);
        std::printf(
            "center ray dir (%.3f,%.3f,%.3f): hit=%d t=%.3f material=%d level=%d steps=%u | brute force "
            "hit=%d t=%.3f material=%d\n",
            static_cast<double>(forward.x), static_cast<double>(forward.y), static_cast<double>(forward.z),
            h.hit ? 1 : 0, static_cast<double>(h.t), static_cast<int>(h.material), h.level, h.steps,
            b.hit ? 1 : 0, static_cast<double>(b.t), static_cast<int>(b.material));
    }

    {
        // 5x5 pixel probe: hit distance / node level / material, to relate what the image shows
        // to which LOD level produced it.
        std::printf("pixel probe (t m / level / material):\n");
        for (int py = 0; py < 5; ++py) {
            for (int px = 0; px < 5; ++px) {
                const float ndcX = (static_cast<float>(px) + 0.5f) / 5.0f * 2.0f - 1.0f;
                const float ndcY = 1.0f - (static_cast<float>(py) + 0.5f) / 5.0f * 2.0f;
                Ray ray;
                ray.origin = o.pos;
                ray.dir = glm::normalize(forward + right * (ndcX * tanHalf * aspect) + up * (ndcY * tanHalf));
                const Hit h = trace_ray(tree, ray, primary);
                if (h.hit) {
                    std::printf("  %7.2f/%2d/%d%s", static_cast<double>(h.t), h.level,
                                static_cast<int>(h.material), h.lod_cube ? "L" : " ");
                } else {
                    std::printf("  %14s", "sky");
                }
            }
            std::printf("\n");
        }
    }

    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(o.width) * o.height * 3u);
    std::atomic<std::uint64_t> totalSteps{0};
    std::atomic<std::uint64_t> secondarySteps{0};
    std::atomic<std::uint64_t> hits{0};
    std::atomic<std::uint64_t> shadowed{0};
    const auto renderStart = std::chrono::steady_clock::now();
    std::vector<std::future<void>> rows;
    rows.reserve(o.height);
    for (std::uint32_t y = 0; y < o.height; ++y) {
        rows.push_back(pool.submit([&, y] {
            std::uint64_t steps = 0;
            std::uint64_t steps2 = 0;
            std::uint64_t rowHits = 0;
            std::uint64_t rowShadowed = 0;
            for (std::uint32_t x = 0; x < o.width; ++x) {
                const float ndcX = (static_cast<float>(x) + 0.5f) / static_cast<float>(o.width) * 2.0f - 1.0f;
                const float ndcY =
                    1.0f - (static_cast<float>(y) + 0.5f) / static_cast<float>(o.height) * 2.0f;
                Ray ray;
                ray.origin = o.pos;
                ray.dir = glm::normalize(forward + right * (ndcX * tanHalf * aspect) + up * (ndcY * tanHalf));
                const Hit hit = trace_ray(tree, ray, primary);
                steps += hit.steps;
                glm::vec3 color;
                if (!hit.hit) {
                    color = view.empty() ? sky_radiance(ray.dir) : glm::vec3{0.0f};
                    if (view == "steps") {
                        color = glm::vec3{std::clamp(static_cast<float>(hit.steps) / 256.0f, 0.0f, 1.0f)};
                    }
                } else {
                    ++rowHits;
                    // svo_march.psh.hlsl's shading, statement for statement (Group Z).
                    const glm::vec3 faceNormal{hit.normal};
                    const glm::vec3 p = hit.position;
                    const float cubePixels = hit.cube_edge / std::max(hit.t * rawPixelAngle, 1.0e-6f);
                    const float faceWeight = std::clamp((cubePixels - 1.5f) / 3.0f, 0.0f, 1.0f);
                    const bool haveSmooth =
                        !hit.solid_leaf && glm::dot(hit.smooth_normal, hit.smooth_normal) > 0.01f;
                    const glm::vec3 smoothNormal =
                        haveSmooth ? glm::normalize(hit.smooth_normal) : faceNormal;
                    const glm::vec3 n = glm::normalize(glm::mix(smoothNormal, faceNormal, faceWeight));

                    const world::materials::MaterialDef& props =
                        world::materials::properties_of(hit.material);
                    glm::vec3 albedo{props.albedo.r, props.albedo.g, props.albedo.b};
                    const float n1 = value_noise(glm::vec2{p.x, p.z} * (1.0f / 24.0f));
                    const float n2 = value_noise(glm::vec2{p.x, p.z} * (1.0f / 7.0f) + 17.31f);
                    albedo *= 0.90f + 0.20f * (0.65f * n1 + 0.35f * n2);
                    if (o.grain && hit.cube_edge > 0.0f) {
                        const glm::vec3 cell =
                            glm::floor((p - faceNormal * (0.5f * hit.cube_edge)) / hit.cube_edge);
                        const float amplitude =
                            o.grain_amplitude * std::clamp((cubePixels - 1.5f) / 2.5f, 0.0f, 1.0f);
                        albedo *= 1.0f + amplitude * (hash3(cell) * 2.0f - 1.0f);
                    }
                    const float diffuse = std::max(glm::dot(n, -kSunDirection), 0.0f);
                    // Secondary origins (svo_march.psh.hlsl): off the face, then lifted along the
                    // averaged normal by one local cube (shadows) / half (AO) so the staircase does
                    // not shade itself; a solid leaf is its own face and gets no lift.
                    const float liftEdge = hit.solid_leaf ? 0.0f : hit.cube_edge;
                    const glm::vec3 faceOffset =
                        p + faceNormal * (g.finest_voxel_edge() * 0.5f + hit.t * 1.0e-4f);
                    const glm::vec3 shadowOrigin = faceOffset + smoothNormal * liftEdge;
                    const glm::vec3 offsetOrigin = faceOffset + smoothNormal * (0.5f * liftEdge);
                    float lit = 1.0f;
                    if (o.shadows && diffuse > 0.0f) {
                        Ray sray;
                        sray.origin = shadowOrigin;
                        sray.dir = -kSunDirection;
                        const Hit sh = trace_ray(tree, sray, shadow);
                        steps2 += sh.steps;
                        lit = sh.hit ? 0.0f : 1.0f;
                        rowShadowed += sh.hit ? 1u : 0u;
                    }
                    float ao = 1.0f;
                    if (o.ao) {
                        const float rayLength = std::max(0.15f, o.ao_radius_px * hit.t * rawPixelAngle);
                        const glm::vec3 helper =
                            std::abs(n.y) < 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{1.0f, 0.0f, 0.0f};
                        const glm::vec3 tangent = glm::normalize(glm::cross(helper, n));
                        const glm::vec3 bitangent = glm::cross(n, tangent);
                        const float rot =
                            hash2(glm::vec2{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f}) *
                            6.2831853f;
                        float occluded = 0.0f;
                        for (int k = 0; k < 4; ++k) {
                            const float phi = rot + static_cast<float>(k) * 1.5707963f;
                            Ray aray;
                            aray.origin = offsetOrigin;
                            aray.dir = glm::normalize(
                                n * 0.75f + (tangent * std::cos(phi) + bitangent * std::sin(phi)) * 0.66f);
                            TraceParams ap = aoParams;
                            ap.max_t = rayLength;
                            const Hit ah = trace_ray(tree, aray, ap);
                            steps2 += ah.steps;
                            if (ah.hit) {
                                occluded += 1.0f - std::clamp(ah.t / rayLength, 0.0f, 1.0f);
                            }
                        }
                        ao = 1.0f - 0.6f * (occluded * 0.25f);
                    }
                    const glm::vec3 ambient = glm::mix(kGroundAmbient, kSkyAmbient, n.y * 0.5f + 0.5f);
                    color = albedo * (ambient * ao + kSunColor * diffuse * lit);
                    if (props.shading == world::materials::Shading::Water) {
                        const glm::vec3 viewDir = -ray.dir;
                        const float cosTheta = std::clamp(viewDir.y, 0.0f, 1.0f);
                        const float fresnel = 0.02f + 0.98f * std::pow(1.0f - cosTheta, 5.0f);
                        const glm::vec3 body{0.10f, 0.28f, 0.40f};
                        color =
                            glm::mix(body, sky_radiance(glm::reflect(ray.dir, glm::vec3{0.0f, 1.0f, 0.0f})),
                                     fresnel) *
                            glm::mix(0.6f, 1.0f, lit);
                    }
                    // exp2 height fog converging on the sky gradient (terrain.psh.hlsl's formula).
                    const float dist = hit.t;
                    const float heightFactor = std::exp2(-std::max(p.y, 0.0f) * 0.012f);
                    const float density = 0.0030f * (0.80f + 0.20f * heightFactor);
                    const float rawFog = 1.0f - std::exp2(-(dist * density) * (dist * density) * 1.442695f);
                    const float fogAmount = std::clamp(rawFog * 1.12f, 0.0f, 1.0f);
                    color = glm::mix(color, sky_gradient(ray.dir), fogAmount);

                    if (!view.empty()) {
                        if (view == "lit") {
                            color = glm::vec3{lit};
                        } else if (view == "ao") {
                            color = glm::vec3{ao};
                        } else if (view == "normal") {
                            color = n * 0.5f + 0.5f;
                        } else if (view == "facenormal") {
                            color = faceNormal * 0.5f + 0.5f;
                        } else if (view == "level") {
                            color = level_color(hit.level);
                        } else if (view == "steps") {
                            color = glm::vec3{std::clamp(static_cast<float>(hit.steps) / 256.0f, 0.0f, 1.0f)};
                        } else if (view == "coverage") {
                            color = glm::vec3{hit.coverage};
                        } else if (view == "cubepx") {
                            color = glm::vec3{std::clamp(cubePixels / 8.0f, 0.0f, 1.0f)};
                        } else if (view == "smooth") {
                            color = haveSmooth ? smoothNormal * 0.5f + 0.5f : glm::vec3{1.0f, 0.0f, 1.0f};
                        } else if (view == "lodcube") {
                            color = hit.lod_cube ? glm::vec3{1.0f, 0.2f, 0.1f} : glm::vec3{0.1f, 0.4f, 1.0f};
                        } else if (view == "material") {
                            color = glm::vec3{props.albedo.r, props.albedo.g, props.albedo.b};
                        } else if (view == "distance") {
                            color = glm::vec3{hit.t * 0.5f - std::floor(hit.t * 0.5f)}; // 2 m bands
                        }
                    }
                }
                // Debug views are written raw (the app disables its post chain for them too).
                const glm::vec3 mapped = view.empty() ? tonemap(color) : color;
                const std::size_t i = (static_cast<std::size_t>(y) * o.width + x) * 3u;
                rgb[i + 0] = to_srgb8(mapped.x);
                rgb[i + 1] = to_srgb8(mapped.y);
                rgb[i + 2] = to_srgb8(mapped.z);
            }
            totalSteps.fetch_add(steps, std::memory_order_relaxed);
            secondarySteps.fetch_add(steps2, std::memory_order_relaxed);
            hits.fetch_add(rowHits, std::memory_order_relaxed);
            shadowed.fetch_add(rowShadowed, std::memory_order_relaxed);
        }));
    }
    for (std::future<void>& f : rows) {
        f.get();
    }
    const double renderSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - renderStart).count();
    const double pixels = static_cast<double>(o.width) * o.height;
    std::printf(
        "render: %ux%u in %.2fs (%.1f Mrays/s primary%s%s), %.1f%% pixels hit terrain, %.1f primary + "
        "%.1f secondary traversal steps/pixel, %.1f%% of hits in sun shadow\n",
        o.width, o.height, renderSeconds, pixels / renderSeconds / 1.0e6, o.shadows ? " + shadow rays" : "",
        o.ao ? " + 4 AO rays" : "", 100.0 * static_cast<double>(hits.load()) / pixels,
        static_cast<double>(totalSteps.load()) / pixels, static_cast<double>(secondarySteps.load()) / pixels,
        hits.load() > 0 ? 100.0 * static_cast<double>(shadowed.load()) / static_cast<double>(hits.load())
                        : 0.0);

    if (!svo_render::PngWriter::write(o.out.c_str(), o.width, o.height, rgb.data())) {
        std::fprintf(stderr, "failed to write %s\n", o.out.c_str());
        return EXIT_FAILURE;
    }
    std::printf("wrote %s\n", o.out.c_str());

    if (o.verify) {
        // The app's --verify-frame local-contrast metric (frame_verify.cpp): a pixel counts when
        // it differs from its left or up neighbor by > 4/255 on any channel.
        std::uint64_t differing = 0;
        for (std::uint32_t y = 1; y < o.height; ++y) {
            for (std::uint32_t x = 1; x < o.width; ++x) {
                const std::size_t i = (static_cast<std::size_t>(y) * o.width + x) * 3u;
                const std::size_t l = i - 3u;
                const std::size_t u = i - static_cast<std::size_t>(o.width) * 3u;
                bool differs = false;
                for (std::size_t c = 0; c < 3 && !differs; ++c) {
                    differs = std::abs(int(rgb[i + c]) - int(rgb[l + c])) > 4 ||
                              std::abs(int(rgb[i + c]) - int(rgb[u + c])) > 4;
                }
                differing += differs ? 1u : 0u;
            }
        }
        const double fraction = static_cast<double>(differing) / pixels;
        const bool ok = fraction >= 0.06;
        std::printf("verify: %.1f%% of pixels carry local contrast (threshold 6%%): %s\n", fraction * 100.0,
                    ok ? "OK" : "FAILED");
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
