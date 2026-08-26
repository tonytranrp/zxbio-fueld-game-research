#include <pb/adapt/void_adapter.hpp>

struct PbPublicHeaderVoidInput {};

struct PbPublicHeaderVoidFn {
  void operator()(PbPublicHeaderVoidInput) const noexcept {}
};

using PbPublicHeaderVoidAdapter = pb::void_adapter<PbPublicHeaderVoidFn, PbPublicHeaderVoidInput>;

static_assert(pb::core::Stage<PbPublicHeaderVoidAdapter>);
static_assert(noexcept(PbPublicHeaderVoidAdapter{}(PbPublicHeaderVoidInput{})));

int pb_public_header_adapt_void() { return 0; }
