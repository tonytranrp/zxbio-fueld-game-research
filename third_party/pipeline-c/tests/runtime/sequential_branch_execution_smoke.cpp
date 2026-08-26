#include <cassert>
#include <stdexcept>

#include <pb/pipeline.hpp>

namespace domain {
struct Raw {
  int selector{};
  int value{};
};

struct Decision {
  int code{};
};

struct ParsePredicate {
  using input_type = Raw;
  using output_type = bool;

  auto operator()(Raw input) const -> bool { return input.selector == 1; }
};

struct ReviewPredicate {
  using input_type = Raw;
  using output_type = bool;

  auto operator()(Raw input) const -> bool { return input.selector == 0; }
};

struct ParseStage {
  using input_type = Raw;
  using output_type = Decision;

  auto operator()(Raw input) const -> Decision {
    if (input.value < 0) {
      throw std::runtime_error{"parse failed"};
    }
    ++parse_calls();
    return Decision{input.value};
  }

  static int& parse_calls() {
    static int value = 0;
    return value;
  }
};

struct ReviewStage {
  using input_type = Raw;
  using output_type = Decision;

  auto operator()(Raw input) const -> Decision {
    ++review_calls();
    return Decision{input.value + 10};
  }

  static int& review_calls() {
    static int value = 0;
    return value;
  }
};

struct Observed : pb::runtime::observer {
  void on_stage_start(const pb::runtime::stage_id&) override { ++starts; }
  void on_stage_success(const pb::runtime::stage_id&) override { ++successes; }
  void on_stage_failure(const pb::runtime::stage_id&, const pb::runtime::error&) override { ++failures; }
  void on_stage_exception(const pb::runtime::stage_id&, const pb::runtime::error&) override { ++exceptions; }

  int starts = 0;
  int successes = 0;
  int failures = 0;
  int exceptions = 0;
};

using ParseCase = pb::case_<ParsePredicate>::then<ParseStage>;
using ReviewCase = pb::case_<ReviewPredicate>::then<ReviewStage>;
using BranchPipeline = pb::from<Raw>::branch<ParseCase, ReviewCase>::to<Decision>;
} // namespace domain

void branch_routing_and_observer_smoke() {
  auto engine = pb::compile<domain::BranchPipeline>(pb::runtime::sequential{});

  assert(domain::ParseStage::parse_calls() == 0);
  assert(domain::ReviewStage::review_calls() == 0);

  auto parse_success = engine.run(domain::Raw{.selector = 1, .value = 7});
  assert(parse_success.has_value());
  assert(parse_success.value().code == 7);
  assert(domain::ParseStage::parse_calls() == 1);
  assert(domain::ReviewStage::review_calls() == 0);

  auto review_success = engine.run(domain::Raw{.selector = 0, .value = 3});
  assert(review_success.has_value());
  assert(review_success.value().code == 13);
  assert(domain::ParseStage::parse_calls() == 1);
  assert(domain::ReviewStage::review_calls() == 1);

  domain::Observed observer{};
  auto engine_with_observer = pb::compile<domain::BranchPipeline>(pb::runtime::sequential{});
  engine_with_observer.set_observer(&observer);
  [[maybe_unused]] auto parse_result = engine_with_observer.run(domain::Raw{.selector = 1, .value = 2});
  [[maybe_unused]] auto review_result = engine_with_observer.run(domain::Raw{.selector = 0, .value = 4});
  assert(observer.starts >= 4);
  assert(observer.successes >= 4);
  assert(observer.failures == 0);
  assert(observer.exceptions == 0);
}

void branch_error_propagation_smoke() {
  auto failing_engine = pb::compile<domain::BranchPipeline>(pb::runtime::sequential{});
  auto fail_result = failing_engine.try_run(domain::Raw{.selector = 1, .value = -1});
  assert(!fail_result.has_value());
  assert(fail_result.error().category == pb::runtime::error_category::exception);
}

int main() {
  branch_routing_and_observer_smoke();
  branch_error_propagation_smoke();
  return 0;
}
