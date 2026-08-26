#include <pb/pipeline.hpp>

struct Raw {};

struct StatefulLogger {
  explicit StatefulLogger(int) {}

  void operator()(Raw) const {}
};

using BrokenVoidAdapter = pb::void_adapter<StatefulLogger, Raw>;

static_assert(pb::adapted_stage<BrokenVoidAdapter>,
              "pb::void_adapter: Fn must be default-constructible");
