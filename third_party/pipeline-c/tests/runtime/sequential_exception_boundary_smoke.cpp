#include <pb/pipeline.hpp>

#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct Input {
  int value{};
};

struct Middle {
  int value{};
};

struct Output {
  int value{};
};

struct AddOne {
  using input_type = Input;
  using output_type = Middle;

  static constexpr auto stage_name() noexcept { return "add_one"; }

  Middle operator()(Input input) const { return {input.value + 1}; }
};

struct BoundaryThrow {
  using input_type = Middle;
  using output_type = Output;

  static constexpr auto stage_key() noexcept { return "boundary.throw"; }
  static constexpr auto stage_name() noexcept { return "boundary_throw"; }

  Output operator()(Middle input) const {
    if (input.value < 0) {
      throw std::runtime_error{"negative boundary"};
    }
    return {input.value * 2};
  }
};

struct UnknownBoundaryThrow {
  using input_type = Middle;
  using output_type = Output;

  static constexpr auto stage_key() noexcept { return "boundary.unknown"; }
  static constexpr auto stage_name() noexcept { return "unknown_boundary_throw"; }

  Output operator()(Middle) const { throw 7; }
};

struct ResultStart {
  using input_type = Input;
  using output_type = Middle;

  static constexpr auto stage_key() noexcept { return "result.start"; }
  static constexpr auto stage_name() noexcept { return "result_start"; }

  pb::runtime::result<Middle> operator()(Input input) const { return Middle{input.value + 1}; }
};

struct ResultBoundaryThrow {
  using input_type = Middle;
  using output_type = Output;

  static constexpr auto stage_key() noexcept { return "result.boundary.throw"; }
  static constexpr auto stage_name() noexcept { return "result_boundary_throw"; }

  Output operator()(Middle input) const {
    if (input.value < 0) {
      throw std::runtime_error{"negative result boundary"};
    }
    return {input.value * 2};
  }
};

struct ResultBoundaryFailure {
  using input_type = Middle;
  using output_type = Output;

  static constexpr auto stage_key() noexcept { return "result.boundary.fail"; }
  static constexpr auto stage_name() noexcept { return "result_boundary_failure"; }

  pb::runtime::result<Output> operator()(Middle input) const {
    if (input.value == 0) {
      return pb::runtime::error{.category = pb::runtime::error_category::stage_failure,
                                .message = "zero result boundary"};
    }
    return Output{input.value * 2};
  }
};

using ThrowPipeline = pb::from<Input>::then<AddOne>::then<BoundaryThrow>::to<Output>;
using UnknownThrowPipeline = pb::from<Input>::then<AddOne>::then<UnknownBoundaryThrow>::to<Output>;
using ResultThenThrowPipeline = pb::from<Input>::then<ResultStart>::then<ResultBoundaryThrow>::to<Output>;
using ResultThenFailurePipeline = pb::from<Input>::then<ResultStart>::then<ResultBoundaryFailure>::to<Output>;
static_assert(pb::valid<ThrowPipeline>);
static_assert(pb::valid<UnknownThrowPipeline>);
static_assert(pb::valid<ResultThenThrowPipeline>);
static_assert(pb::valid<ResultThenFailurePipeline>);

struct recording_observer final : pb::runtime::observer {
  std::vector<std::string> events{};

  void on_stage_start(const pb::runtime::stage_id& stage) override {
    events.push_back("start:" + stage.name + "/" + stage.key);
  }

  void on_stage_success(const pb::runtime::stage_id& stage) override {
    events.push_back("success:" + stage.name + "/" + stage.key);
  }

  void on_stage_failure(const pb::runtime::stage_id& stage, const pb::runtime::error& error) override {
    events.push_back("failure:" + stage.name + "/" + stage.key + ":" + error.message);
  }

  void on_stage_exception(const pb::runtime::stage_id& stage, const pb::runtime::error& error) override {
    events.push_back("exception:" + stage.name + "/" + stage.key + ":" + pb::runtime::describe(error));
  }
};

int main() {
  {
    auto engine = pb::compile<ThrowPipeline>(pb::runtime::sequential{});
    recording_observer observer{};
    engine.set_observer(&observer);

    try {
      (void)engine.run(Input{-2});
      return 1;
    } catch (const std::runtime_error& error) {
      assert(std::string_view{error.what()} == "negative boundary");
    }

    assert((observer.events == std::vector<std::string>{
                                   "start:add_one/add_one",
                                   "success:add_one/add_one",
                                   "start:boundary_throw/boundary.throw",
                                   "exception:boundary_throw/boundary.throw:exception at boundary_throw "
                                   "(boundary.throw): negative boundary",
                               }));
  }

  {
    auto engine = pb::compile<ThrowPipeline>(pb::runtime::sequential{});
    recording_observer observer{};
    engine.set_observer(&observer);

    auto failed = engine.try_run(Input{-2});
    assert(!failed.has_value());
    assert(failed.error().category == pb::runtime::error_category::exception);
    assert(failed.error().stage.key == "boundary.throw");
    assert(failed.error().stage.name == "boundary_throw");
    assert(failed.error().message == "negative boundary");
    assert(pb::runtime::describe(failed.error()) == "exception at boundary_throw (boundary.throw): negative boundary");
    assert((observer.events == std::vector<std::string>{
                                   "start:add_one/add_one",
                                   "success:add_one/add_one",
                                   "start:boundary_throw/boundary.throw",
                                   "exception:boundary_throw/boundary.throw:exception at boundary_throw "
                                   "(boundary.throw): negative boundary",
                               }));
  }

  {
    auto engine = pb::compile<UnknownThrowPipeline>(pb::runtime::sequential{});
    recording_observer observer{};
    engine.set_observer(&observer);

    try {
      (void)engine.run(Input{1});
      return 1;
    } catch (...) {
    }

    assert((observer.events == std::vector<std::string>{
                                   "start:add_one/add_one",
                                   "success:add_one/add_one",
                                   "start:unknown_boundary_throw/boundary.unknown",
                                   "exception:unknown_boundary_throw/boundary.unknown:exception at "
                                   "unknown_boundary_throw (boundary.unknown): stage threw an unknown exception",
                               }));
  }

  {
    auto engine = pb::compile<UnknownThrowPipeline>(pb::runtime::sequential{});
    recording_observer observer{};
    engine.set_observer(&observer);

    auto failed = engine.try_run(Input{1});
    assert(!failed.has_value());
    assert(failed.error().category == pb::runtime::error_category::exception);
    assert(failed.error().stage.key == "boundary.unknown");
    assert(failed.error().stage.name == "unknown_boundary_throw");
    assert(failed.error().message == "stage threw an unknown exception");
    assert(pb::runtime::describe(failed.error()) ==
           "exception at unknown_boundary_throw (boundary.unknown): stage threw an unknown exception");
    assert((observer.events == std::vector<std::string>{
                                   "start:add_one/add_one",
                                   "success:add_one/add_one",
                                   "start:unknown_boundary_throw/boundary.unknown",
                                   "exception:unknown_boundary_throw/boundary.unknown:exception at "
                                   "unknown_boundary_throw (boundary.unknown): stage threw an unknown exception",
                               }));
  }

  {
    auto engine = pb::compile<ResultThenThrowPipeline>(pb::runtime::sequential{});
    recording_observer observer{};
    engine.set_observer(&observer);

    auto failed = engine.run(Input{-2});
    assert(!failed.has_value());
    assert(failed.error().category == pb::runtime::error_category::exception);
    assert(failed.error().stage.key == "result.boundary.throw");
    assert(failed.error().stage.name == "result_boundary_throw");
    assert(failed.error().message == "negative result boundary");
    assert(pb::runtime::describe(failed.error()) ==
           "exception at result_boundary_throw (result.boundary.throw): negative result boundary");
    assert((observer.events == std::vector<std::string>{
                                   "start:result_start/result.start",
                                   "success:result_start/result.start",
                                   "start:result_boundary_throw/result.boundary.throw",
                                   "exception:result_boundary_throw/result.boundary.throw:exception at "
                                   "result_boundary_throw (result.boundary.throw): negative result boundary",
                               }));
  }

  {
    auto engine = pb::compile<ResultThenFailurePipeline>(pb::runtime::sequential{});
    recording_observer observer{};
    engine.set_observer(&observer);

    auto failed = engine.run(Input{-1});
    assert(!failed.has_value());
    assert(failed.error().category == pb::runtime::error_category::stage_failure);
    assert(failed.error().stage.key == "result.boundary.fail");
    assert(failed.error().stage.name == "result_boundary_failure");
    assert(failed.error().message == "zero result boundary");
    assert(pb::runtime::describe(failed.error()) ==
           "stage_failure at result_boundary_failure (result.boundary.fail): zero result boundary");
    assert((observer.events == std::vector<std::string>{
                                   "start:result_start/result.start",
                                   "success:result_start/result.start",
                                   "start:result_boundary_failure/result.boundary.fail",
                                   "failure:result_boundary_failure/result.boundary.fail:zero result boundary",
                               }));
  }

  {
    auto engine = pb::compile<ResultThenThrowPipeline>(pb::runtime::sequential{});
    recording_observer observer{};
    engine.set_observer(&observer);

    auto failed = engine.try_run(Input{-2});
    assert(!failed.has_value());
    assert(failed.error().category == pb::runtime::error_category::exception);
    assert(failed.error().stage.key == "result.boundary.throw");
    assert(failed.error().stage.name == "result_boundary_throw");
    assert(failed.error().message == "negative result boundary");
    assert((observer.events == std::vector<std::string>{
                                   "start:result_start/result.start",
                                   "success:result_start/result.start",
                                   "start:result_boundary_throw/result.boundary.throw",
                                   "exception:result_boundary_throw/result.boundary.throw:exception at "
                                   "result_boundary_throw (result.boundary.throw): negative result boundary",
                               }));
  }

  return 0;
}
