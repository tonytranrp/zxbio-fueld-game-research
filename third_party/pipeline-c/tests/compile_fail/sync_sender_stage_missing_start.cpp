#include <pb/adapt/sender_receiver.hpp>

struct In {};
struct Out {};

struct MissingStartFactory {
  using output_type = Out;

  struct sender {
    template <class Receiver>
    struct operation {
      Receiver receiver;
      // Missing start() on purpose.
    };

    template <class Receiver>
    auto connect(Receiver receiver) && -> operation<Receiver> {
      return operation<Receiver>{receiver};
    }
  };

  auto operator()(In) const { return sender{}; }
};

using BrokenStage = pb::sync_sender_stage<MissingStartFactory, In>;

static_assert(pb::core::Stage<BrokenStage>);

int main() {
  (void)BrokenStage{}(In{});
  return 0;
}
