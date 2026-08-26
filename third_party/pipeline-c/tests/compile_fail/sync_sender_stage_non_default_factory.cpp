#include <pb/adapt/sender_receiver.hpp>

struct In {};
struct Out {};

struct NonDefaultFactory {
  using output_type = Out;

  explicit NonDefaultFactory(int) {}

  auto operator()(In) const { return pb::sync_just(Out{}); }
};

using BrokenStage = pb::sync_sender_stage<NonDefaultFactory, In>;

static_assert(pb::core::Stage<BrokenStage>,
              "pb::sync_sender_stage: Factory must be default-constructible");
