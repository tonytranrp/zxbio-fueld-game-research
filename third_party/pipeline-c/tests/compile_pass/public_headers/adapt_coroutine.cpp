#include <pb/adapt/coroutine.hpp>

namespace {
struct probe_input {
  int value{};
};

struct probe_coroutine {
  pb::coro::sync_task<int> operator()(probe_input in) const { co_return in.value + 1; }
};

using probe_stage = pb::coroutine_stage<probe_coroutine, probe_input>;
static_assert(pb::core::Stage<probe_stage>);
static_assert(pb::coro::sync_task_type<pb::coro::sync_task<int>>);
} // namespace

int pb_public_header_adapt_coroutine() { return 0; }
