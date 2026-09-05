#include "world/svo/tree_builder.hpp"

#include "detail/tree_builder_impl.hpp"

namespace world::svo {

// The production sampler's instantiation (cpp-heavy-templates rule 13): one translation unit
// pays the template's compile cost; every consumer sees only the extern declaration.
template BrickTree build_tree<TerrainSampler>(const TerrainSampler&, const TreeGeometry&, const BuildParams&,
                                              engine::jobs::ThreadPool*, BuildStats*);

} // namespace world::svo
