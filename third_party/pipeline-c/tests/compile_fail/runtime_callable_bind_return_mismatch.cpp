#include <pb/adapt/runtime_callable.hpp>

struct In {
  int value{};
};

struct Out {
  int value{};
};

struct WrongOut {
  int value{};
};

int main() {
  auto stage = pb::bind_callable<In, Out>([](In input) { return WrongOut{input.value}; });
  (void)stage;
  return 0;
}
