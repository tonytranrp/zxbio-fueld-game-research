#include <pb/adapt/sender_receiver.hpp>

struct In {};
struct WrongIn {};
struct Out {};

struct InputMismatchFactory {
  using output_type = Out;

  auto operator()(WrongIn) const { return pb::sync_just(Out{}); }
};

using BrokenStage = pb::sync_sender_stage<InputMismatchFactory, In>;

static_assert(pb::core::Stage<BrokenStage>,
              "pb::sync_sender_stage: Factory must be invocable with Input");
