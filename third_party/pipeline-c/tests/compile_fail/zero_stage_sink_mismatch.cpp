#include <pb/pipeline.hpp>

struct Raw {};
struct Receipt {};

using Broken = pb::from<Raw>::to<Receipt>;
static_assert(pb::valid<Broken>);

int main() { return 0; }
