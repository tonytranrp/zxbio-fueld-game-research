#include <pb/pipeline.hpp>

struct Input {};
struct Parsed {};

void consume(Input) {}
Parsed parse(Parsed) { return {}; }

using VoidAdapter = pb::adapt_fn<&consume, Input, void>;
using ParseAdapter = pb::adapt_fn<&parse, Parsed, Parsed>;
using Broken = pb::from<Input>::then<VoidAdapter>::then<ParseAdapter>::to<Parsed>;

static_assert(pb::valid<Broken>,
              "Non-final void adapter stages are unsupported: a void output cannot connect to a following stage input");
