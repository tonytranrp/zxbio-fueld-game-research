#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/core/math.hpp"
#include "world/generation/tree_placement.hpp"

namespace world::generation {

// Grown tree structure (Prompt 001 Group C / docs/goals.md group AF): a branch skeleton produced by
// SPACE COLONIZATION (Runions et al. 2007), the standard algorithm and the one
// research/tree-motion-growth-and-appearance.md §8 names as the pipeline's first stage.
//
// The idea in one paragraph: scatter "attractors" through the crown volume, then repeatedly let
// every branch tip that has attractors near it grow one segment toward their average direction,
// deleting attractors a tip has reached. Branching is not scripted -- it emerges when a tip's
// attractors pull in genuinely different directions, which is why the result looks grown rather
// than recursed. It also fills the crown volume you give it, which is what makes the silhouettes
// controllable.
//
// This replaces nothing yet. v1's implicit box trunk + octahedron lobes (tree_placement.hpp) is
// still what both renderers draw; the skeleton is the input the voxelizer (goal 191) will use.

struct SkeletonSegment {
    // Index of the parent segment, or -1 for the root. ALWAYS earlier in the list than this
    // segment, so a tip-to-root walk is a reverse iteration and a root-to-tip walk is a forward
    // one -- no recursion, no sorting, and goal 186's connectivity test pins it.
    std::int32_t parent = -1;
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f};
    float radius = 0.0f;     // set by the pipe model (goal 187), zero until then
    float leaf_count = 0.0f; // distal leaf units carried by this segment (goal 188)
};

struct TreeSkeleton {
    std::vector<SkeletonSegment> segments;

    [[nodiscard]] bool empty() const noexcept { return segments.empty(); }
    // Conservative world-space bounds of every segment endpoint.
    [[nodiscard]] TreeBounds bounds() const noexcept;
};

// The knobs that make one species different from another. Deliberately physical-ish rather than
// artistic: the crown is a volume to fill, and the growth constants are lengths, so a change here
// has a predictable effect instead of an aesthetic one.
struct SpeciesParams {
    // Crown volume, an ellipsoid centred `crown_centre_height` above the base. `crown_taper`
    // shrinks the horizontal radius linearly toward the top, which is what makes a conifer a cone
    // and a broadleaf a dome from the same code.
    float crown_radius = 3.0f;
    float crown_height = 5.0f;
    float crown_centre_height = 5.0f;
    float crown_taper = 0.0f; // 0 = ellipsoid, 1 = cone (radius reaches 0 at the crown top)

    float trunk_height = 3.0f;      // bare stem below the crown; 0 for a shrub
    float segment_length = 0.4f;    // one growth step
    float attraction_radius = 2.6f; // an attractor influences tips within this
    float kill_radius = 0.7f;       // ...and is consumed when a tip gets this close
    std::uint32_t attractor_count = 220;
    std::uint32_t max_iterations = 220;

    // Species flavour for the sway/flutter work (goal 190), carried here so one struct describes a
    // species end to end. Higher = leaves twist more readily in a gust (an aspen's flattened
    // petiole is the textbook case -- research §4.4 makes petiole stiffness the species knob).
    float flutter_response = 1.0f;
};

// The four presets: the three existing silhouettes so the world still reads the same at a glance,
// plus the aspen the wind work wants as a showcase.
enum class TreeSpecies : std::uint8_t { RoundBroadleaf, Conifer, Shrub, Aspen };

[[nodiscard]] SpeciesParams species_params(TreeSpecies species) noexcept;

// The species a v1 placement stands in for, so a v2 tree can be grown exactly where a v1 tree was
// and the world keeps its distribution.
[[nodiscard]] TreeSpecies species_of(const TreePlacement& tree) noexcept;

// Grow a skeleton. Deterministic in (seed, world position, species): the same tree in the same
// world is the same bytes on every run and in every tool, which is the property the whole project's
// determinism standard rests on.
[[nodiscard]] TreeSkeleton grow_skeleton(int seed, glm::vec3 base, const SpeciesParams& params);

// Convenience: grow the skeleton a v1 placement implies, scaled to its own trunk height and canopy
// radius so a v2 tree occupies the space the v1 tree did.
[[nodiscard]] TreeSkeleton grow_skeleton_for(int seed, const TreePlacement& tree);

// ---- the pipe model (goal 187) and leaf mass (goal 188) -------------------------------------------

// Leaf area per leaf-bearing tip, and the constant relating accumulated leaf count to cross-section.
// The Pipe Model (Shinozaki 1964; tree research §1.2/§5.3): a tree is a bundle of conducting pipes,
// each supplying one unit of leaf, so a branch's cross-sectional AREA is proportional to the leaf it
// carries -- which is also da Vinci's rule, and the reason trunks taper the way they do.
struct PipeModelParams {
    // Leaf Area Index: square metres of leaf per square metre of crown FOOTPRINT. This is the
    // number canopy ecology actually measures, and real broadleaf canopies sit in a 0.5-3.0 band.
    //
    // Leaf area is a property of the CROWN, not of how many tips the colonization happened to grow
    // -- which is the whole reason it is expressed this way. A first version hung a fixed area on
    // each tip, and tip count turned out to vary 7x between seeds (34 at one, 245 at another), so
    // the same species came out at LAI 0.3 or 8.7 depending on the seed. Scaling by footprint and
    // sharing across whatever tips exist makes the canopy density a species property and the tip
    // count purely a matter of how finely it is subdivided.
    float leaf_area_index = 1.6f;
    // Sapwood cross-section per unit leaf area -- the Huber value. Sanity-checked against a real
    // tree rather than picked: a mature oak at 0.6 m DBH carries roughly 500 m^2 of leaf, and
    // 0.283 m^2 / 500 m^2 is about 5.7e-4. A first attempt used 6e-5, an order of magnitude too
    // small, and every radius in the tree fell under min_radius -- so the floor became the whole
    // model and the pipe-model tests failed by reporting a trunk exactly as thick as a twig.
    float area_per_leaf_unit = 5.0e-4f;
    float min_radius = 0.01f; // a twig is still a centimetre thick
};

// Assign `leaf_count` (distal leaf area, m^2) and `radius` to every segment.
//
// The crown footprint is measured from the skeleton's own horizontal extent, so the tree's leaf
// area follows the crown it actually grew rather than the crown it was asked for. `apply_pipe_model`
// then walks tip-to-root in ONE backward pass: because a segment's parent is always earlier in the
// list, every child is visited before its parent -- no recursion and no second data structure.
// Not noexcept: it allocates a per-segment scratch buffer, so it can throw bad_alloc like any
// other allocating function. Saying otherwise would be a lie the compiler enforces with
// std::terminate.
void apply_pipe_model(TreeSkeleton& skeleton, const PipeModelParams& params = {});

// Horizontal footprint area of the crown, m^2 -- what LAI is measured against.
[[nodiscard]] float crown_footprint(const TreeSkeleton& skeleton) noexcept;

// Total leaf area the skeleton carries, after apply_pipe_model.
[[nodiscard]] float total_leaf_area(const TreeSkeleton& skeleton); // allocates; see above

// Segments with no children -- the tips that actually bear leaves.
[[nodiscard]] std::vector<std::size_t> leaf_segments(const TreeSkeleton& skeleton);

} // namespace world::generation
