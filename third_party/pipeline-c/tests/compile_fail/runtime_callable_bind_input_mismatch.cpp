#include <pb/adapt/runtime_callable.hpp>

struct In {
  int value{};
};

struct Out {
  int value{};
};

struct OtherIn {
  int value{};
};

int main() {
  auto stage = pb::bind_callable<In, Out>([](OtherIn input) { return Out{input.value}; });
  (void)stage;
  return 0;
}
