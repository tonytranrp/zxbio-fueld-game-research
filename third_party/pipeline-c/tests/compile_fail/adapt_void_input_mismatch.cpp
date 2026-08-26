#include <pb/pipeline.hpp>

struct Raw {};
struct Receipt {};

struct ReceiptLogger {
  void operator()(Receipt) const {}
};

using BrokenVoidAdapter = pb::void_adapter<ReceiptLogger, Raw>;

static_assert(pb::Stage<BrokenVoidAdapter>,
              "pb::void_adapter: Fn must be callable with Input and return void");
