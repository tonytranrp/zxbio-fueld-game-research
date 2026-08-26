/// @file policy_copying_conflict.cpp
/// @brief Negative regression: the copying policy axis is single-valued.

#include <pb/pipeline.hpp>

namespace {

struct Raw {
  int value{};
};
struct Done {
  int value{};
};

struct Stage {
  using input_type = Raw;
  using output_type = Done;
  Done operator()(Raw input) const { return {.value = input.value}; }
};

// A second copying marker is ambiguous regardless of whether it is a duplicate
// or a different marker.  The builder must reject it instead of letting the
// first marker silently win.
using BadPipeline = pb::from<Raw>::with<pb::policy::copying::value>::with<
    pb::policy::copying::clone>::then<Stage>::to<Done>;

} // namespace

int main() { return sizeof(BadPipeline); }
