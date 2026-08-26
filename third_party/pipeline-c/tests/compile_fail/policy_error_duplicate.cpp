/// @file policy_error_duplicate.cpp
/// @brief Negative regression: duplicate runtime error policy markers are rejected.

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

using BadPipeline = pb::from<Raw>::with<pb::policy::errors::throwing>::with<
    pb::policy::errors::throwing>::then<Stage>::to<Done>;

} // namespace

int main() { return sizeof(BadPipeline); }
