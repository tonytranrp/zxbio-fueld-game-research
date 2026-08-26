#include <pb/pipeline.hpp>

struct Raw {};

using Pipeline = pb::from<Raw>::to<Raw>;

// Should fail because public stage key/name helpers require an in-range stage index.
static_assert(pb::stage_key<Pipeline, 0>() == "bad");

int main() { return 0; }
