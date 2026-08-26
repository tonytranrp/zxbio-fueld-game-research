#include <pb/adapt/sender_receiver.hpp>

struct In {};
struct Out {};

struct MissingOutputTypeFactory {
  auto operator()(In) const { return pb::sync_just(Out{}); }
};

using BrokenStage = pb::sync_sender_stage<MissingOutputTypeFactory, In>;

static_assert(pb::core::Stage<BrokenStage>,
              "pb::sync_sender_stage: Factory must be default-constructible, expose output_type");
