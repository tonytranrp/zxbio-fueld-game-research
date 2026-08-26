#include <pb/adapt/sender_receiver.hpp>

struct In {};

struct VoidSenderFactory {
  using output_type = void;

  auto operator()(In) const { return pb::sync_just(0); }
};

using BrokenStage = pb::sync_sender_stage<VoidSenderFactory, In>;

static_assert(pb::core::Stage<BrokenStage>,
              "pb::sync_sender_stage: void-valued senders are not supported");
