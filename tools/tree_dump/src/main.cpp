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
//   seed:    integer                            (default 1337)
//   out:     path                               (default tree_<species>_<seed>.obj)

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <string>
#include <string_view>

#include "world/generation/tree_skeleton.hpp"

namespace {

using world::generation::SkeletonSegment;
using world::generation::TreeSkeleton;
using world::generation::TreeSpecies;

bool parse_int(const char* s, int& out) {
    if (s == nullptr) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || v < std::numeric_limits<int>::min() ||
        v > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(v);
    return true;
}

bool parse_species(std::string_view name, TreeSpecies& out) {
    if (name == "round") {
        out = TreeSpecies::RoundBroadleaf;
    } else if (name == "conifer") {
        out = TreeSpecies::Conifer;
    } else if (name == "shrub") {
        out = TreeSpecies::Shrub;
    } else if (name == "aspen") {
        out = TreeSpecies::Aspen;
    } else {
        return false;
    }
    return true;
}

int run(int argc, char** argv) {
    TreeSpecies species = TreeSpecies::RoundBroadleaf;
    std::string speciesName = "round";
    int seed = 1337;

    if (argc > 1) {
        speciesName = argv[1];
        if (!parse_species(speciesName, species)) {
            std::fprintf(stderr, "species must be round|conifer|shrub|aspen, got \"%s\"\n", argv[1]);
            return EXIT_FAILURE;
        }
    }
    if (argc > 2 && !parse_int(argv[2], seed)) {
        std::fprintf(stderr, "seed must be an integer\n");
        return EXIT_FAILURE;
    }
    const std::string path =
        argc > 3 ? argv[3] : ("tree_" + speciesName + "_" + std::to_string(seed) + ".obj");

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
