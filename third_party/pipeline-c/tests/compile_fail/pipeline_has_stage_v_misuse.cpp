#include <pb/pipeline.hpp>

struct Raw {};
struct Parse {};

static_assert(pb::pipeline_has_stage_v<Parse, Raw>);

int main() { return 0; }
