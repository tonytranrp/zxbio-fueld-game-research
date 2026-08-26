#include <pb/adapt/sender_receiver.hpp>

struct In {};
struct Out {};

struct MissingConnectFactory {
  using output_type = Out;

  struct sender {
    // Missing connect(receiver) on purpose.
  };

  auto operator()(In) const { return sender{}; }
};

using BrokenStage = pb::sync_sender_stage<MissingConnectFactory, In>;

int main() {
  (void)BrokenStage{}(In{});
  return 0;
}
