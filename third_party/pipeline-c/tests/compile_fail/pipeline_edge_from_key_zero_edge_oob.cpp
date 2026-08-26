#include <pb/pipeline.hpp>

struct Raw {};

using Pipeline = pb::from<Raw>::to<Raw>;

// Should fail because public edge key/name helpers require an in-range edge index.
static_assert(pb::edge_from_key<Pipeline, 0>() == "bad");

int main() { return 0; }
