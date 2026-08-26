#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};

struct StatefulParser {
  explicit StatefulParser(int) {}

  Parsed operator()(Raw) const { return {}; }
};

using BrokenAdaptedFunctor = pb::adapt_functor<StatefulParser, Raw, Parsed>;

static_assert(pb::adapted_stage<BrokenAdaptedFunctor>,
              "Adapted stage callable object must be default-initializable");
