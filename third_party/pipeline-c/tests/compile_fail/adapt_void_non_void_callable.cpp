#include <pb/pipeline.hpp>

struct Raw {};

struct ValueReturningLogger {
  int operator()(Raw) const { return 0; }
};

using BrokenVoidAdapter = pb::void_adapter<ValueReturningLogger, Raw>;

static_assert(pb::Stage<BrokenVoidAdapter>,
              "pb::void_adapter: Fn must be callable with Input and return void");
