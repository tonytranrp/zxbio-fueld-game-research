/// @file policy_diagnostics_conflict.cpp
/// @brief Negative regression: the diagnostics policy axis is single-valued.

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

using BadPipeline = pb::from<Raw>::with<pb::policy::diagnostics::verbose>::with<
    pb::policy::diagnostics::quiet>::then<Stage>::to<Done>;

} // namespace

int main() { return sizeof(BadPipeline); }
