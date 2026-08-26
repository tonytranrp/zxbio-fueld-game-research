#include <pb/pipeline.hpp>

struct Raw {};

using Pipeline = pb::from<Raw>::to<Raw>;

// Should fail with an explicit out-of-range stage metadata diagnostic for descriptor aliases.
using BadStageDescriptor = pb::pipeline_stage_descriptor_t<Pipeline, 0>;

int main() { return 0; }
