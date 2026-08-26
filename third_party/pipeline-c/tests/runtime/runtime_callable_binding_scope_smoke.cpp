#include <pb/adapt/runtime_callable.hpp>

#include <cstdlib>
#include <functional>
#include <memory>

namespace {
void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

struct In {
  int value{};
};

struct Out {
  int value{};
};
} // namespace

int main() {
  auto outer_calls = std::make_shared<int>(0);
  auto inner_calls = std::make_shared<int>(0);

  auto outer = pb::bind_callable<In, Out>([outer_calls](In input) {
    ++*outer_calls;
    return Out{input.value + 10};
  });
  auto inner = pb::bind_callable<In, Out>([inner_calls](In input) {
    ++*inner_calls;
    return Out{input.value + 100};
  });

  // Direct calls use each stage's owned callable when no binding is active.
  pb_test_require(outer(In{1}).value == 11);
  pb_test_require(inner(In{1}).value == 101);
  pb_test_require(*outer_calls == 1);
  pb_test_require(*inner_calls == 1);

  // Nested guards are LIFO: inner binding overrides outer, then outer is restored.
  {
    auto outer_guard = pb::with_runtime_callable_binding(outer);
    pb::runtime_callable<In, Out> default_constructed{};
    pb_test_require(default_constructed(In{2}).value == 12);
    pb_test_require(*outer_calls == 2);

    {
      auto inner_guard = pb::with_runtime_callable_binding(inner);
      pb_test_require(default_constructed(In{3}).value == 103);
      pb_test_require(*inner_calls == 2);
    }

    pb_test_require(default_constructed(In{4}).value == 14);
    pb_test_require(*outer_calls == 3);

    // Binding an unbound stage throws immediately and does not clobber the
    // existing outer binding.
    pb::runtime_callable<In, Out> unbound{};
    bool threw = false;
    try {
      auto bad_guard = pb::with_runtime_callable_binding(unbound);
      (void)bad_guard;
    } catch (const std::bad_function_call&) {
      threw = true;
    }
    pb_test_require(threw);
    pb_test_require(default_constructed(In{5}).value == 15);
    pb_test_require(*outer_calls == 4);
  }

  // After every guard is gone, an unbound stage still reports the documented
  // bad_function_call path.
  bool threw = false;
  try {
    [[maybe_unused]] auto ignored = pb::runtime_callable<In, Out>{}(In{6});
  } catch (const std::bad_function_call&) {
    threw = true;
  }
  pb_test_require(threw);

  return 0;
}
