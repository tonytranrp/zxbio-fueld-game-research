// tree_dump: writes a grown tree SKELETON (world/generation/tree_skeleton, docs/goals.md group AF)
// as a Wavefront .obj of line segments, so the space-colonization result can be looked at before
// anything voxelizes it. Goal 186's check asks for exactly that, and for a good reason: a skeleton
// bug is obvious in a picture and nearly invisible in a voxel field.
//
// Line elements (`l`) rather than triangles: the skeleton IS lines, and every .obj viewer and most
// plotting scripts read them. Radii from the pipe model are written as a comment per segment rather
// than as geometry -- turning them into tubes is the voxelizer's job (goal 191), not this tool's.
//
// Usage: tree_dump [species] [seed] [out.obj]
//   species: round | conifer | shrub | aspen   (default round)
// The same values are also settable by name (--species/--seed/--out); --help lists them.
//   seed:    integer                            (default 1337)
//   out:     path                               (default tree_<species>_<seed>.obj)

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "dump_options.hpp"
#include "engine/cli/help.hpp"
#include "sway_frames.hpp"
#include "world/chunk/chunk_voxels.hpp"
#include "world/generation/field/macro_pipeline.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/tree_placement.hpp"
#include "world/generation/tree_skeleton.hpp"
#include "world/generation/tree_sway.hpp"
#include "world/wind/wind_field.hpp"

namespace {

using world::generation::SkeletonSegment;
using world::generation::TreeSkeleton;
using world::generation::TreeSpecies;


// Prompt 007 goal 335's viewed capture. Drives the sway model with the real wind field, then writes
// `sway_frames` orthographic side views evenly spaced across `sway_periods` fundamental periods --
// and prints the numbers the calibration of `SwayParams::drag_pressure` rests on: "the tree leans
// about 1.5% of its height in a fresh breeze" is a claim that has to be measurable from here.
void write_sway_frames(const TreeSkeleton& tree, const tools::tree_dump::Options& opt,
                       const std::string& speciesName) {
    using world::generation::kSwayTick;

    world::generation::SwayParams params =
        world::generation::sway_params_for(world::generation::species_params(opt.species));
    params.branch_reaction = opt.branch_reaction;
    world::generation::SwayState state = world::generation::make_sway_state(tree, params);
    if (state.empty()) {
        std::fprintf(stderr, "the sway state is empty -- nothing to draw\n");
        return;
    }

    world::wind::WindParams wind = world::wind::kDefaultWind;
    wind.base_speed = opt.wind_speed;

    const world::generation::TreeBounds bounds = tree.bounds();
    const float height = bounds.max.y - bounds.min.y;
    const float f0 = world::generation::fundamental_frequency_hz(tree, params);
    const float period = f0 > 0.0f ? 1.0f / f0 : 4.0f;

    std::size_t dynamic = 0;
    for (const std::uint8_t q : state.quasi_static) {
        dynamic += q == 0u ? 1u : 0u;
    }
    std::printf("  sway: %zu chains (%zu dynamic, %zu quasi-static) over %zu segments; f0 = %.3f Hz "
                "(period %.2f s)\n",
                state.size(), dynamic, state.size() - dynamic, state.segment_count(),
                static_cast<double>(f0), static_cast<double>(period));

    // Settle: drive from rest so the first written frame is not the transient of the tree being
    // switched on. A step response is a different experiment and it lives in the unit test.
    float t = 0.0f;
    const auto settleSteps = static_cast<int>(opt.sway_settle_seconds / kSwayTick);
    float peak = 0.0f;
    for (int i = 0; i < settleSteps; ++i) {
        world::generation::step_sway(state, wind, t, kSwayTick);
        t += kSwayTick;
        peak = std::max(peak, glm::length(world::generation::tip_displacement(state, tree)));
    }
    std::printf("  settled %.1f s at %.1f m/s: peak tip displacement %.3f m on a %.2f m tree (%.2f%% of "
                "height)\n",
                static_cast<double>(opt.sway_settle_seconds), static_cast<double>(opt.wind_speed),
                static_cast<double>(peak), static_cast<double>(height),
                static_cast<double>(100.0f * peak / std::max(height, 1.0e-3f)));

    const std::string prefix =
        opt.sway_png.empty() ? (speciesName + "_sway_" + std::to_string(opt.seed)) : opt.sway_png;
    const tools::tree_dump::SideView view = tools::tree_dump::SideView::fit(
        bounds, opt.sway_size.width, opt.sway_size.height, std::max(0.4f, 2.0f * peak));

    std::vector<glm::vec3> posedStart;
    std::vector<glm::vec3> posedEnd;
    const float frameDt = opt.sway_periods * period / static_cast<float>(opt.sway_frames);

    for (int f = 0; f < opt.sway_frames; ++f) {
        tools::tree_dump::SwayCanvas canvas(opt.sway_size.width, opt.sway_size.height);
        canvas.clear(18, 20, 26);

        world::generation::pose_skeleton(state, tree, posedStart, posedEnd);
        for (std::size_t i = 0; i < tree.segments.size(); ++i) {
            const world::generation::SkeletonSegment& seg = tree.segments[i];
            const float halfWidth = std::max(0.9f, seg.radius * view.scale);
            // The rest pose first, in grey, so the posed one draws over it where they coincide.
            canvas.line(view.px(seg.start.x), view.py(seg.start.y), view.px(seg.end.x),
                        view.py(seg.end.y), halfWidth, 62, 66, 72);
        }
        for (std::size_t i = 0; i < tree.segments.size(); ++i) {
            const world::generation::SkeletonSegment& seg = tree.segments[i];
            const float halfWidth = std::max(0.9f, seg.radius * view.scale);
            // Leaf-bearing tips green, wood brown -- the same two-material split the voxelizer will
            // make, so a picture here and a picture there are comparable.
            const bool foliage = seg.leaf_count > 0.0f && seg.radius < 0.04f;
            canvas.line(view.px(posedStart[i].x), view.py(posedStart[i].y), view.px(posedEnd[i].x),
                        view.py(posedEnd[i].y), halfWidth, foliage ? 96 : 150, foliage ? 190 : 112,
                        foliage ? 84 : 66);
        }

        const std::string path = prefix + "_" + std::to_string(f) + ".png";
        if (!canvas.write(path)) {
            std::fprintf(stderr, "cannot write %s\n", path.c_str());
            return;
        }
        const glm::vec3 tip = world::generation::tip_displacement(state, tree);
        std::printf("  frame %d: t = %.3f s, tip %+.3f m x, %+.3f m z -> %s\n", f,
                    static_cast<double>(t), static_cast<double>(tip.x), static_cast<double>(tip.z),
                    path.c_str());

        const auto steps = std::max(1, static_cast<int>(frameDt / kSwayTick));
        for (int i = 0; i < steps; ++i) {
            world::generation::step_sway(state, wind, t, kSwayTick);
            t += kSwayTick;
        }
    }
}

int run(int argc, char** argv) {
    tools::tree_dump::Options opt;
    const engine::cli::ParseOutcome parsed = tools::tree_dump::parse_options(argc, argv, opt);
    if (!parsed.ok) {
        std::fprintf(stderr, "%s\n\n", parsed.message.c_str());
        std::fputs(tools::tree_dump::help_text().c_str(), stderr);
        return EXIT_FAILURE;
    }
    if (parsed.help_requested) {
        std::fputs(tools::tree_dump::help_text().c_str(), stdout);
        return EXIT_SUCCESS;
    }
    if (opt.have_near) {
        // The macro field, so --near lists the trees the APP has rather than the ones the
        // pre-pipeline noise world would have had.
        const world::generation::HeightmapGenerator heightmap(
            opt.seed, world::generation::field::bake_playable_field(opt.seed));
        const auto toChunk = [](float v) {
            return static_cast<std::int32_t>(std::floor(v / static_cast<float>(world::chunk::kChunkSize)));
        };
        const auto reach =
            static_cast<std::int32_t>(opt.near_radius / static_cast<float>(world::chunk::kChunkSize)) + 1;
        const std::int32_t cx = toChunk(opt.near_xz.x);
        const std::int32_t cz = toChunk(opt.near_xz.y);
        std::printf("tree placements within %.0f m of (%.1f, %.1f), seed %d:\n",
                    static_cast<double>(opt.near_radius), static_cast<double>(opt.near_xz.x),
                    static_cast<double>(opt.near_xz.y), opt.seed);
        std::size_t found = 0;
        for (std::int32_t dz = -reach; dz <= reach; ++dz) {
            for (std::int32_t dx = -reach; dx <= reach; ++dx) {
                for (const world::generation::TreePlacement& t :
                     world::generation::compute_tree_placements(cx + dx, cz + dz, opt.seed, heightmap)) {
                    const float ddx = t.world_x - opt.near_xz.x;
                    const float ddz = t.world_z - opt.near_xz.y;
                    const float d = std::sqrt(ddx * ddx + ddz * ddz);
                    if (d > opt.near_radius) {
                        continue;
                    }
                    ++found;
                    std::printf("  %6.2f m  at (%8.2f, %8.2f) base y %6.2f  trunk %.2f  canopy %.2f  %s\n",
                                static_cast<double>(d), static_cast<double>(t.world_x),
                                static_cast<double>(t.world_z), static_cast<double>(t.base_height),
                                static_cast<double>(t.trunk_height), static_cast<double>(t.canopy_radius),
                                tools::tree_dump::species_name(world::generation::species_of(t)).data());
                }
            }
        }
        std::printf("  %zu placements\n", found);
        return EXIT_SUCCESS;
    }

    const TreeSpecies species = opt.species;
    const std::string speciesName{tools::tree_dump::species_name(species)};
    const int seed = opt.seed;
    const std::string path =
        opt.out.empty() ? ("tree_" + speciesName + "_" + std::to_string(seed) + ".obj") : opt.out;

    TreeSkeleton tree = world::generation::grow_skeleton(seed, {0.0f, 0.0f, 0.0f},
                                                         world::generation::species_params(species));
    if (tree.empty()) {
        std::fprintf(stderr, "the skeleton grew nothing -- check the species parameters\n");
        return EXIT_FAILURE;
    }
    world::generation::apply_pipe_model(tree);

    // Same shape as mesh_dump's own open: MSVC treats plain fopen as a deprecation error under /WX.
    std::FILE* out = nullptr;
#if defined(_MSC_VER)
    (void)fopen_s(&out, path.c_str(), "w");
#else
    out = std::fopen(path.c_str(), "w");
#endif
    if (out == nullptr) {
        std::fprintf(stderr, "cannot write %s\n", path.c_str());
        return EXIT_FAILURE;
    }
    std::fprintf(out, "# %s skeleton, seed %d: %zu segments\n", speciesName.c_str(), seed,
                 tree.segments.size());
    std::fprintf(out, "o %s_skeleton\n", speciesName.c_str());
    // Two vertices per segment. Sharing them between a parent's end and a child's start would halve
    // the file, but keeping them separate means a line's two vertices are always adjacent, which is
    // what makes the `l` elements trivially correct.
    for (const SkeletonSegment& s : tree.segments) {
        std::fprintf(out, "v %.5f %.5f %.5f\n", static_cast<double>(s.start.x),
                     static_cast<double>(s.start.y), static_cast<double>(s.start.z));
        std::fprintf(out, "v %.5f %.5f %.5f\n", static_cast<double>(s.end.x), static_cast<double>(s.end.y),
                     static_cast<double>(s.end.z));
    }
    for (std::size_t i = 0; i < tree.segments.size(); ++i) {
        // .obj indices are 1-based.
        std::fprintf(out, "l %zu %zu  # r=%.4f leaf=%.3f\n", i * 2 + 1, i * 2 + 2,
                     static_cast<double>(tree.segments[i].radius),
                     static_cast<double>(tree.segments[i].leaf_count));
    }
    std::fclose(out);

    if (opt.sway_frames > 0) {
        write_sway_frames(tree, opt, speciesName);
    }

    const world::generation::TreeBounds b = tree.bounds();
    std::printf("%s seed %d: %zu segments, %zu tips, %.2f m^2 leaf, trunk radius %.3f m\n",
                speciesName.c_str(), seed, tree.segments.size(),
                world::generation::leaf_segments(tree).size(),
                static_cast<double>(world::generation::total_leaf_area(tree)),
                static_cast<double>(tree.segments.front().radius));
    std::printf("  bounds x[%.2f %.2f] y[%.2f %.2f] z[%.2f %.2f] -> %s\n", static_cast<double>(b.min.x),
                static_cast<double>(b.max.x), static_cast<double>(b.min.y), static_cast<double>(b.max.y),
                static_cast<double>(b.min.z), static_cast<double>(b.max.z), path.c_str());
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "tree_dump: %s\n", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        std::fprintf(stderr, "tree_dump: unknown exception\n");
        return EXIT_FAILURE;
    }
}
