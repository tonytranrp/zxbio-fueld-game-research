#include <pb/pipeline.hpp>

struct Raw {};

using Pipeline = pb::from<Raw>::to<Raw>;

// Should fail with an explicit out-of-range stage metadata diagnostic for alias helpers.
using BadStage = pb::pipeline_stage_t<Pipeline, 0>;

int main() { return 0; }
