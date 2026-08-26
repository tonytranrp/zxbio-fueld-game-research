#include <pb/pipeline.hpp>

struct Raw {};

using Pipeline = pb::from<Raw>::to<Raw>;

// Should fail because a zero-stage pipeline has no edge metadata.
using BadToStage = pb::pipeline_edge_to_stage_t<Pipeline, 0>;

int main() { return 0; }
