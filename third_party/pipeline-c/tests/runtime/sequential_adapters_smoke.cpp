#include <pb/pipeline.hpp>

#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


namespace {
void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}
}  // namespace

struct Input {
  int value{};
};

struct Parsed {
  int value{};
};

struct MoveOnlyParsed {
  std::unique_ptr<int> value{};
};

struct Output {
  int value{};
};

struct MoveOnlyOutput {
  std::unique_ptr<int> value{};
};

struct opaque_error {
  int code{};
};

struct diagnostic_error {
  pb::runtime::error diagnostic{};
};

struct move_only_diagnostic_error {
  pb::runtime::error diagnostic{};
  std::unique_ptr<int> token{};
};

struct move_only_opaque_error {
  std::unique_ptr<int> code{};
};

template <class T, class E>
struct external_expected {
  using value_type = T;
  using error_type = E;

  bool ok{};
  T value_{};
  E error_{};

  [[nodiscard]] bool has_value() const { return ok; }
  [[nodiscard]] const T& value() const& { return value_; }
  [[nodiscard]] T&& value() && { return std::move(value_); }
  [[nodiscard]] const E& error() const& { return error_; }
  [[nodiscard]] E&& error() && { return std::move(error_); }
};

template <class E>
struct external_void_expected {
  using value_type = void;
  using error_type = E;

  bool ok{};
  E error_{};

  [[nodiscard]] bool has_value() const { return ok; }
  void value() const {}
  [[nodiscard]] const E& error() const& { return error_; }
  [[nodiscard]] E&& error() && { return std::move(error_); }
};

namespace adapter_stage_names {
struct parse_input {
  static constexpr auto value = "parse_input";
};

struct multiply_input {
  static constexpr auto value = "multiply_input";
};

struct parse_member {
  static constexpr auto value = "parse_member";
};

struct parse_functor {
  static constexpr auto value = "parse_functor";
};

struct parse_functor_move_only_diagnostic {
  static constexpr auto value = "parse_functor_move_only_diagnostic";
};

struct parse_opaque {
  static constexpr auto value = "parse_opaque";
};

struct parse_diagnostic {
  static constexpr auto value = "parse_diagnostic";
};

struct parse_result_move_only_error {
  static constexpr auto value = "parse_result_move_only_error";
};

struct parse_member_move_only_diagnostic {
  static constexpr auto value = "parse_member_move_only_diagnostic";
};

struct parse_member_move_only_value {
  static constexpr auto value = "parse_member_move_only_value";
};

struct consume_void {
  static constexpr auto value = "consume_void";
};

struct consume_void_move_only {
  static constexpr auto value = "consume_void_move_only";
};

struct direct_member_consume_void {
  static constexpr auto value = "direct_member_consume_void";
};

struct direct_member_emit_move_only {
  static constexpr auto value = "direct_member_emit_move_only";
};

struct parse_lvalue_ref_member {
  static constexpr auto value = "parse_lvalue_ref_member";
};

struct parse_noexcept_lvalue_ref_member {
  static constexpr auto value = "parse_noexcept_lvalue_ref_member";
};
} // namespace adapter_stage_names

Parsed parse_input_fn(Input input) {
  return Parsed{input.value + 1};
}

Parsed parse_input_throwing(Input input) {
  if (input.value < -100) {
    throw std::runtime_error{"parse_input_throwing failed"};
  }
  return Parsed{input.value + 2};
}

Parsed double_input(Parsed parsed) {
  return Parsed{parsed.value * 2};
}

struct MemberParser {
  Parsed parse(Input input) const { return Parsed{input.value + 3}; }

  external_expected<Parsed, std::string> parse_checked(Input input) const {
    if (input.value < 0) {
      return {.ok = false, .error_ = "member parse failed"};
    }
    return {.ok = true, .value_ = {input.value + 3}};
  }

  external_expected<Parsed, move_only_diagnostic_error> parse_move_only_diagnostic(Input input) const {
    if (input.value < 0) {
      return {.ok = false,
              .error_ = {.diagnostic = {.stage = {.key = "member.external", .name = "MemberExternal"},
                                        .category = pb::runtime::error_category::stage_failure,
                                        .message = "move-only member diagnostic failed"},
                         .token = std::make_unique<int>(17)}};
    }
    return {.ok = true, .value_ = {input.value + 11}};
  }

  external_expected<MoveOnlyParsed, std::string> parse_move_only_value(Input input) const {
    if (input.value < 0) {
      return {.ok = false, .error_ = "member move-only value failed"};
    }
    return {.ok = true, .value_ = {.value = std::make_unique<int>(input.value + 17)}};
  }

  external_void_expected<std::string> consume_checked(Input input) const {
    if (input.value < 0) {
      return {.ok = false, .error_ = "member consume failed"};
    }
    return {.ok = true};
  }

  Parsed parse_lvalue_ref(Input input) & { return Parsed{input.value + 19}; }

  Parsed parse_noexcept_lvalue_ref(Input input) & noexcept { return Parsed{input.value + 21}; }
};

struct MemberEmitter {
  Output emit(Parsed parsed) const { return Output{parsed.value + 4}; }

  Output emit_noexcept(Parsed parsed) const noexcept { return Output{parsed.value + 6}; }

  MoveOnlyOutput emit_move_only(MoveOnlyParsed parsed) const {
    return MoveOnlyOutput{.value = std::make_unique<int>(*parsed.value + 4)};
  }
};

struct FunctorParser {
  external_expected<Parsed, std::string> operator()(Input input) const {
    if (input.value < 0) {
      return {.ok = false, .error_ = "functor parse failed"};
    }
    return {.ok = true, .value_ = {input.value + 5}};
  }
};

struct FunctorMoveOnlyDiagnosticParser {
  external_expected<Parsed, move_only_diagnostic_error> operator()(Input input) const {
    if (input.value < 0) {
      return {.ok = false,
              .error_ = {.diagnostic = {.stage = {.key = "functor.external", .name = "FunctorExternal"},
                                        .category = pb::runtime::error_category::stage_failure,
                                        .message = "move-only functor diagnostic failed"},
                         .token = std::make_unique<int>(23)}};
    }
    return {.ok = true, .value_ = {input.value + 13}};
  }
};

external_expected<Parsed, opaque_error> parse_opaque_error(Input input) {
  if (input.value < 0) {
    return {.ok = false, .error_ = {.code = 42}};
  }
  return {.ok = true, .value_ = {input.value + 7}};
}

external_expected<Parsed, diagnostic_error> parse_diagnostic_error(Input input) {
  if (input.value < 0) {
    return {.ok = false,
            .error_ = {.diagnostic = {.stage = {.key = "external.parse", .name = "ExternalParse"},
                                      .category = pb::runtime::error_category::stage_failure,
                                      .message = "diagnostic parse failed"}}};
  }
  return {.ok = true, .value_ = {input.value + 9}};
}

pb::runtime::result<Parsed, move_only_diagnostic_error> parse_result_move_only_error(Input input) {
  if (input.value < 0) {
    return move_only_diagnostic_error{
        .diagnostic = {.stage = {.key = "external.result", .name = "ExternalResult"},
                       .category = pb::runtime::error_category::stage_failure,
                       .message = "move-only result diagnostic failed"},
        .token = std::make_unique<int>(31)};
  }
  return Parsed{input.value + 15};
}

external_void_expected<opaque_error> consume_void_expected(Input input) {
  if (input.value < 0) {
    return {.ok = false, .error_ = {.code = 7}};
  }
  return {.ok = true};
}

external_void_expected<move_only_opaque_error> consume_void_move_only_expected(Input input) {
  if (input.value < 0) {
    return {.ok = false, .error_ = {.code = std::make_unique<int>(13)}};
  }
  return {.ok = true};
}

using ParsedAdapter = pb::adapt<pb::name<adapter_stage_names::parse_input>, pb::fn<parse_input_fn>,
                                 pb::in<Input>, pb::out<Parsed>>;
using ThrowingParsedAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_input>, pb::fn<parse_input_throwing>, pb::in<Input>,
             pb::out<Parsed>>;
using MultiplierAdapter =
    pb::adapt<pb::name<adapter_stage_names::multiply_input>, pb::fn<double_input>,
             pb::in<Parsed>, pb::out<Parsed>>;
using NamedDirectMemberAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_member>, pb::member<&MemberParser::parse>, pb::in<Input>,
              pb::out<Parsed>>;
using NamedDirectExpectedMemberAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_member>, pb::member<&MemberParser::parse_checked>, pb::in<Input>,
              pb::out<Parsed>>;
using DirectMemberMoveOnlyDiagnosticAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_member_move_only_diagnostic>,
              pb::member<&MemberParser::parse_move_only_diagnostic>, pb::in<Input>, pb::out<Parsed>>;
using DirectMemberMoveOnlyValueAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_member_move_only_value>,
              pb::member<&MemberParser::parse_move_only_value>, pb::in<Input>, pb::out<MoveOnlyParsed>>;
using UnnamedDirectMemberAdapter =
    pb::adapt<pb::member<&MemberEmitter::emit>, pb::in<Parsed>, pb::out<Output>>;
using NoexceptDirectMemberAdapter =
    pb::adapt<pb::member<&MemberEmitter::emit_noexcept>, pb::in<Parsed>, pb::out<Output>>;
using DirectMemberMoveOnlyEmitterAdapter =
    pb::adapt<pb::name<adapter_stage_names::direct_member_emit_move_only>,
              pb::member<&MemberEmitter::emit_move_only>, pb::in<MoveOnlyParsed>, pb::out<MoveOnlyOutput>>;
using ExpectedFunctorAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_functor>, pb::functor<FunctorParser>, pb::in<Input>,
              pb::out<Parsed>>;
using FunctorMoveOnlyDiagnosticAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_functor_move_only_diagnostic>,
              pb::functor<FunctorMoveOnlyDiagnosticParser>, pb::in<Input>, pb::out<Parsed>>;
using OpaqueErrorAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_opaque>, pb::fn<parse_opaque_error>, pb::in<Input>,
              pb::out<Parsed>>;
using DiagnosticErrorAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_diagnostic>, pb::fn<parse_diagnostic_error>, pb::in<Input>,
              pb::out<Parsed>>;
using ResultMoveOnlyErrorAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_result_move_only_error>, pb::fn<parse_result_move_only_error>,
              pb::in<Input>, pb::out<Parsed>>;
using VoidExpectedAdapter =
    pb::adapt<pb::name<adapter_stage_names::consume_void>, pb::fn<consume_void_expected>, pb::in<Input>,
              pb::out<void>>;
using VoidMoveOnlyExpectedAdapter =
    pb::adapt<pb::name<adapter_stage_names::consume_void_move_only>, pb::fn<consume_void_move_only_expected>,
              pb::in<Input>, pb::out<void>>;
using DirectMemberVoidExpectedAdapter =
    pb::adapt<pb::name<adapter_stage_names::direct_member_consume_void>, pb::member<&MemberParser::consume_checked>,
              pb::in<Input>, pb::out<void>>;
using LvalueRefQualifiedMemberAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_lvalue_ref_member>,
              pb::member<&MemberParser::parse_lvalue_ref>, pb::in<Input>, pb::out<Parsed>>;
using NoexceptLvalueRefQualifiedMemberAdapter =
    pb::adapt<pb::name<adapter_stage_names::parse_noexcept_lvalue_ref_member>,
              pb::member<&MemberParser::parse_noexcept_lvalue_ref>, pb::in<Input>, pb::out<Parsed>>;

struct Emit {
  using input_type = Parsed;
  using output_type = Output;
  using error_type = pb::runtime::error;

  static constexpr auto stage_key() noexcept { return "emit"; }
  static constexpr auto stage_name() noexcept { return "emit_result"; }

  pb::runtime::result<Output> operator()(Parsed parsed) const {
    if (parsed.value < 0) {
      return pb::runtime::error{.stage = {.key = "emit", .name = "emit_result"},
                               .category = pb::runtime::error_category::stage_failure,
                               .message = "invalid parsed value"};
    }
    return Output{parsed.value + 1};
  }
};

using Pipeline = pb::from<Input>::then<ParsedAdapter>::then<MultiplierAdapter>::then<Emit>::to<Output>;
using ThrowingPipeline =
    pb::from<Input>::then<ThrowingParsedAdapter>::then<MultiplierAdapter>::then<Emit>::to<Output>;
using ThrowingPipelineRaw = pb::from<Input>::then<ThrowingParsedAdapter>::then<MultiplierAdapter>::to<Parsed>;
using DirectMemberPipeline =
    pb::from<Input>::then<NamedDirectMemberAdapter>::then<UnnamedDirectMemberAdapter>::to<Output>;
using DirectExpectedMemberPipeline =
    pb::from<Input>::then<NamedDirectExpectedMemberAdapter>::then<UnnamedDirectMemberAdapter>::to<Output>;
using DirectMemberMoveOnlyDiagnosticPipeline =
    pb::from<Input>::then<DirectMemberMoveOnlyDiagnosticAdapter>::then<UnnamedDirectMemberAdapter>::to<Output>;
using DirectMemberMoveOnlyValuePipeline =
    pb::from<Input>::then<DirectMemberMoveOnlyValueAdapter>::to<MoveOnlyParsed>;
using DirectMemberMoveOnlyValueHandoffPipeline =
    pb::from<Input>::then<DirectMemberMoveOnlyValueAdapter>::then<DirectMemberMoveOnlyEmitterAdapter>::to<MoveOnlyOutput>;
using ExpectedFunctorPipeline =
    pb::from<Input>::then<ExpectedFunctorAdapter>::then<UnnamedDirectMemberAdapter>::to<Output>;
using FunctorMoveOnlyDiagnosticPipeline =
    pb::from<Input>::then<FunctorMoveOnlyDiagnosticAdapter>::then<UnnamedDirectMemberAdapter>::to<Output>;
using OpaqueErrorPipeline =
    pb::from<Input>::then<OpaqueErrorAdapter>::then<UnnamedDirectMemberAdapter>::to<Output>;
using DiagnosticErrorPipeline =
    pb::from<Input>::then<DiagnosticErrorAdapter>::then<UnnamedDirectMemberAdapter>::to<Output>;
using ResultMoveOnlyErrorPipeline =
    pb::from<Input>::then<ResultMoveOnlyErrorAdapter>::then<UnnamedDirectMemberAdapter>::to<Output>;
using VoidExpectedPipeline = pb::from<Input>::then<VoidExpectedAdapter>::to<void>;
using VoidMoveOnlyExpectedPipeline = pb::from<Input>::then<VoidMoveOnlyExpectedAdapter>::to<void>;
using DirectMemberVoidExpectedPipeline = pb::from<Input>::then<DirectMemberVoidExpectedAdapter>::to<void>;
using LvalueRefQualifiedMemberPipeline =
    pb::from<Input>::then<LvalueRefQualifiedMemberAdapter>::then<UnnamedDirectMemberAdapter>::to<Output>;
using NoexceptLvalueRefQualifiedMemberPipeline =
    pb::from<Input>::then<NoexceptLvalueRefQualifiedMemberAdapter>::then<UnnamedDirectMemberAdapter>::to<Output>;

static_assert(pb::adapted_stage<ParsedAdapter>);
static_assert(pb::adapted_stage<MultiplierAdapter>);
static_assert(pb::adapted_stage<NamedDirectMemberAdapter>);
static_assert(pb::adapted_stage<NamedDirectExpectedMemberAdapter>);
static_assert(pb::adapted_stage<DirectMemberMoveOnlyDiagnosticAdapter>);
static_assert(pb::adapted_stage<DirectMemberMoveOnlyValueAdapter>);
static_assert(pb::adapted_stage<UnnamedDirectMemberAdapter>);
static_assert(pb::adapted_stage<NoexceptDirectMemberAdapter>);
static_assert(pb::adapted_stage<DirectMemberMoveOnlyEmitterAdapter>);
static_assert(pb::adapted_stage<ExpectedFunctorAdapter>);
static_assert(pb::adapted_stage<FunctorMoveOnlyDiagnosticAdapter>);
static_assert(pb::adapted_stage<OpaqueErrorAdapter>);
static_assert(pb::adapted_stage<DiagnosticErrorAdapter>);
static_assert(pb::adapted_stage<ResultMoveOnlyErrorAdapter>);
static_assert(pb::adapted_stage<VoidExpectedAdapter>);
static_assert(pb::adapted_stage<VoidMoveOnlyExpectedAdapter>);
static_assert(pb::adapted_stage<DirectMemberVoidExpectedAdapter>);
static_assert(pb::adapted_stage<LvalueRefQualifiedMemberAdapter>);
static_assert(pb::adapted_stage<NoexceptLvalueRefQualifiedMemberAdapter>);
static_assert(!noexcept(LvalueRefQualifiedMemberAdapter{}(Input{1})));
static_assert(noexcept(NoexceptLvalueRefQualifiedMemberAdapter{}(Input{1})));
static_assert(pb::valid<Pipeline>);
static_assert(pb::valid<ThrowingPipeline>);
static_assert(pb::valid<ThrowingPipelineRaw>);
static_assert(pb::valid<DirectMemberPipeline>);
static_assert(pb::valid<DirectExpectedMemberPipeline>);
static_assert(pb::valid<DirectMemberMoveOnlyDiagnosticPipeline>);
static_assert(pb::valid<DirectMemberMoveOnlyValuePipeline>);
static_assert(pb::valid<DirectMemberMoveOnlyValueHandoffPipeline>);
static_assert(pb::valid<ExpectedFunctorPipeline>);
static_assert(pb::valid<FunctorMoveOnlyDiagnosticPipeline>);
static_assert(pb::valid<OpaqueErrorPipeline>);
static_assert(pb::valid<DiagnosticErrorPipeline>);
static_assert(pb::valid<ResultMoveOnlyErrorPipeline>);
static_assert(pb::valid<VoidExpectedPipeline>);
static_assert(pb::valid<VoidMoveOnlyExpectedPipeline>);
static_assert(pb::valid<DirectMemberVoidExpectedPipeline>);
static_assert(pb::valid<LvalueRefQualifiedMemberPipeline>);
static_assert(pb::valid<NoexceptLvalueRefQualifiedMemberPipeline>);

struct recording_observer final : pb::runtime::observer {
  std::vector<std::string> events{};

  void on_stage_start(const pb::runtime::stage_id& stage) override {
    events.push_back("start:" + stage.name + "/" + stage.key);
  }

  void on_stage_exception(const pb::runtime::stage_id& stage, const pb::runtime::error& error) override {
    events.push_back("exception:" + stage.name + "/" + stage.key + ":" + error.message);
  }

  void on_stage_success(const pb::runtime::stage_id& stage) override {
    events.push_back("success:" + stage.name + "/" + stage.key);
  }

  void on_stage_failure(const pb::runtime::stage_id& stage, const pb::runtime::error& error) override {
    events.push_back("failure:" + stage.name + "/" + stage.key + ":" + pb::runtime::describe(error));
  }
};

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto ok = engine.run(Input{4});
  pb_test_require(ok.has_value());
  pb_test_require(ok.value().value == 11);

  auto direct_member_engine = pb::compile<DirectMemberPipeline>(pb::runtime::sequential{});
  auto direct_member_output = direct_member_engine.run(Input{5});
  pb_test_require(direct_member_output.value == 12);

  auto noexcept_member_output = NoexceptDirectMemberAdapter{}(Parsed{5});
  pb_test_require(noexcept_member_output.value == 11);

  auto lvalue_ref_member_engine = pb::compile<LvalueRefQualifiedMemberPipeline>(pb::runtime::sequential{});
  auto lvalue_ref_member_output = lvalue_ref_member_engine.run(Input{5});
  pb_test_require(lvalue_ref_member_output.value == 28);

  auto noexcept_lvalue_ref_member_engine =
      pb::compile<NoexceptLvalueRefQualifiedMemberPipeline>(pb::runtime::sequential{});
  auto noexcept_lvalue_ref_member_output = noexcept_lvalue_ref_member_engine.run(Input{5});
  pb_test_require(noexcept_lvalue_ref_member_output.value == 30);

  auto direct_expected_member_engine = pb::compile<DirectExpectedMemberPipeline>(pb::runtime::sequential{});
  recording_observer expected_observer{};
  direct_expected_member_engine.set_observer(&expected_observer);

  auto direct_expected_failed = direct_expected_member_engine.try_run(Input{-5});
  pb_test_require(!direct_expected_failed.has_value());
  pb_test_require(direct_expected_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(direct_expected_failed.error().stage.key == "parse_member");
  pb_test_require(direct_expected_failed.error().stage.name == "parse_member");
  pb_test_require(direct_expected_failed.error().message == "member parse failed");
  pb_test_require(pb::runtime::describe(direct_expected_failed.error()) ==
         "expected_error at parse_member: member parse failed");
  pb_test_require((expected_observer.events == std::vector<std::string>{
                                          "start:parse_member/parse_member",
                                          "failure:parse_member/parse_member:expected_error at parse_member: "
                                          "member parse failed",
                                      }));

  auto direct_member_move_only_diagnostic_engine =
      pb::compile<DirectMemberMoveOnlyDiagnosticPipeline>(pb::runtime::sequential{});
  recording_observer direct_member_move_only_diagnostic_observer{};
  direct_member_move_only_diagnostic_engine.set_observer(&direct_member_move_only_diagnostic_observer);

  auto direct_member_move_only_diagnostic_failed =
      direct_member_move_only_diagnostic_engine.try_run(Input{-5});
  pb_test_require(!direct_member_move_only_diagnostic_failed.has_value());
  pb_test_require(direct_member_move_only_diagnostic_failed.error().category ==
         pb::runtime::error_category::expected_error);
  pb_test_require(direct_member_move_only_diagnostic_failed.error().stage.key ==
         "parse_member_move_only_diagnostic");
  pb_test_require(direct_member_move_only_diagnostic_failed.error().stage.name ==
         "parse_member_move_only_diagnostic");
  pb_test_require(direct_member_move_only_diagnostic_failed.error().message == "move-only member diagnostic failed");
  pb_test_require(pb::runtime::describe(direct_member_move_only_diagnostic_failed.error()) ==
         "expected_error at parse_member_move_only_diagnostic: move-only member diagnostic failed");
  pb_test_require((direct_member_move_only_diagnostic_observer.events ==
          std::vector<std::string>{
              "start:parse_member_move_only_diagnostic/parse_member_move_only_diagnostic",
              "failure:parse_member_move_only_diagnostic/parse_member_move_only_diagnostic:"
              "expected_error at parse_member_move_only_diagnostic: move-only member diagnostic failed",
          }));

  direct_member_move_only_diagnostic_observer.events.clear();
  auto direct_member_move_only_diagnostic_raw_failed =
      direct_member_move_only_diagnostic_engine.run(Input{-5});
  pb_test_require(!direct_member_move_only_diagnostic_raw_failed.has_value());
  pb_test_require(direct_member_move_only_diagnostic_raw_failed.error().category ==
         pb::runtime::error_category::expected_error);
  pb_test_require(direct_member_move_only_diagnostic_raw_failed.error().stage.key ==
         "parse_member_move_only_diagnostic");
  pb_test_require(direct_member_move_only_diagnostic_raw_failed.error().stage.name ==
         "parse_member_move_only_diagnostic");
  pb_test_require(direct_member_move_only_diagnostic_raw_failed.error().message ==
         "move-only member diagnostic failed");
  pb_test_require(pb::runtime::describe(direct_member_move_only_diagnostic_raw_failed.error()) ==
         "expected_error at parse_member_move_only_diagnostic: move-only member diagnostic failed");
  pb_test_require((direct_member_move_only_diagnostic_observer.events ==
          std::vector<std::string>{
              "start:parse_member_move_only_diagnostic/parse_member_move_only_diagnostic",
              "failure:parse_member_move_only_diagnostic/parse_member_move_only_diagnostic:"
              "expected_error at parse_member_move_only_diagnostic: move-only member diagnostic failed",
          }));

  auto direct_member_move_only_value_engine = pb::compile<DirectMemberMoveOnlyValuePipeline>(pb::runtime::sequential{});
  recording_observer direct_member_move_only_value_observer{};
  direct_member_move_only_value_engine.set_observer(&direct_member_move_only_value_observer);

  auto direct_member_move_only_value_ok = direct_member_move_only_value_engine.try_run(Input{5});
  pb_test_require(direct_member_move_only_value_ok.has_value());
  pb_test_require(!direct_member_move_only_value_ok.has_error());
  pb_test_require(direct_member_move_only_value_ok.value().value != nullptr);
  pb_test_require(*direct_member_move_only_value_ok.value().value == 22);

  auto direct_member_move_only_value_failed = direct_member_move_only_value_engine.try_run(Input{-5});
  pb_test_require(!direct_member_move_only_value_failed.has_value());
  pb_test_require(direct_member_move_only_value_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(direct_member_move_only_value_failed.error().stage.key == "parse_member_move_only_value");
  pb_test_require(direct_member_move_only_value_failed.error().stage.name == "parse_member_move_only_value");
  pb_test_require(direct_member_move_only_value_failed.error().message == "member move-only value failed");
  pb_test_require(pb::runtime::describe(direct_member_move_only_value_failed.error()) ==
         "expected_error at parse_member_move_only_value: member move-only value failed");
  pb_test_require((direct_member_move_only_value_observer.events ==
          std::vector<std::string>{
              "start:parse_member_move_only_value/parse_member_move_only_value",
              "success:parse_member_move_only_value/parse_member_move_only_value",
              "start:parse_member_move_only_value/parse_member_move_only_value",
              "failure:parse_member_move_only_value/parse_member_move_only_value:"
              "expected_error at parse_member_move_only_value: member move-only value failed",
          }));

  auto direct_member_move_only_value_raw_ok = direct_member_move_only_value_engine.run(Input{7});
  pb_test_require(direct_member_move_only_value_raw_ok.has_value());
  pb_test_require(!direct_member_move_only_value_raw_ok.has_error());
  pb_test_require(direct_member_move_only_value_raw_ok.value().value != nullptr);
  pb_test_require(*direct_member_move_only_value_raw_ok.value().value == 24);

  auto direct_member_move_only_handoff_engine =
      pb::compile<DirectMemberMoveOnlyValueHandoffPipeline>(pb::runtime::sequential{});
  recording_observer direct_member_move_only_handoff_observer{};
  direct_member_move_only_handoff_engine.set_observer(&direct_member_move_only_handoff_observer);

  auto direct_member_move_only_handoff_ok = direct_member_move_only_handoff_engine.try_run(Input{5});
  pb_test_require(direct_member_move_only_handoff_ok.has_value());
  pb_test_require(!direct_member_move_only_handoff_ok.has_error());
  pb_test_require(direct_member_move_only_handoff_ok.value().value != nullptr);
  pb_test_require(*direct_member_move_only_handoff_ok.value().value == 26);

  auto direct_member_move_only_handoff_failed = direct_member_move_only_handoff_engine.try_run(Input{-5});
  pb_test_require(!direct_member_move_only_handoff_failed.has_value());
  pb_test_require(direct_member_move_only_handoff_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(direct_member_move_only_handoff_failed.error().stage.key == "parse_member_move_only_value");
  pb_test_require(direct_member_move_only_handoff_failed.error().stage.name == "parse_member_move_only_value");
  pb_test_require(direct_member_move_only_handoff_failed.error().message == "member move-only value failed");
  pb_test_require((direct_member_move_only_handoff_observer.events ==
          std::vector<std::string>{
              "start:parse_member_move_only_value/parse_member_move_only_value",
              "success:parse_member_move_only_value/parse_member_move_only_value",
              "start:direct_member_emit_move_only/direct_member_emit_move_only",
              "success:direct_member_emit_move_only/direct_member_emit_move_only",
              "start:parse_member_move_only_value/parse_member_move_only_value",
              "failure:parse_member_move_only_value/parse_member_move_only_value:"
              "expected_error at parse_member_move_only_value: member move-only value failed",
          }));

  auto direct_member_move_only_handoff_raw_ok = direct_member_move_only_handoff_engine.run(Input{7});
  pb_test_require(direct_member_move_only_handoff_raw_ok.has_value());
  pb_test_require(!direct_member_move_only_handoff_raw_ok.has_error());
  pb_test_require(direct_member_move_only_handoff_raw_ok.value().value != nullptr);
  pb_test_require(*direct_member_move_only_handoff_raw_ok.value().value == 28);

  auto expected_functor_engine = pb::compile<ExpectedFunctorPipeline>(pb::runtime::sequential{});
  recording_observer functor_observer{};
  expected_functor_engine.set_observer(&functor_observer);

  auto expected_functor_failed = expected_functor_engine.try_run(Input{-5});
  pb_test_require(!expected_functor_failed.has_value());
  pb_test_require(expected_functor_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(expected_functor_failed.error().stage.key == "parse_functor");
  pb_test_require(expected_functor_failed.error().stage.name == "parse_functor");
  pb_test_require(expected_functor_failed.error().message == "functor parse failed");
  pb_test_require(pb::runtime::describe(expected_functor_failed.error()) ==
         "expected_error at parse_functor: functor parse failed");
  pb_test_require((functor_observer.events == std::vector<std::string>{
                                         "start:parse_functor/parse_functor",
                                         "failure:parse_functor/parse_functor:expected_error at parse_functor: "
                                         "functor parse failed",
                                     }));

  auto functor_move_only_diagnostic_engine =
      pb::compile<FunctorMoveOnlyDiagnosticPipeline>(pb::runtime::sequential{});
  recording_observer functor_move_only_diagnostic_observer{};
  functor_move_only_diagnostic_engine.set_observer(&functor_move_only_diagnostic_observer);

  auto functor_move_only_diagnostic_failed = functor_move_only_diagnostic_engine.try_run(Input{-5});
  pb_test_require(!functor_move_only_diagnostic_failed.has_value());
  pb_test_require(functor_move_only_diagnostic_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(functor_move_only_diagnostic_failed.error().stage.key == "parse_functor_move_only_diagnostic");
  pb_test_require(functor_move_only_diagnostic_failed.error().stage.name == "parse_functor_move_only_diagnostic");
  pb_test_require(functor_move_only_diagnostic_failed.error().message == "move-only functor diagnostic failed");
  pb_test_require(pb::runtime::describe(functor_move_only_diagnostic_failed.error()) ==
         "expected_error at parse_functor_move_only_diagnostic: move-only functor diagnostic failed");
  pb_test_require((functor_move_only_diagnostic_observer.events ==
          std::vector<std::string>{
              "start:parse_functor_move_only_diagnostic/parse_functor_move_only_diagnostic",
              "failure:parse_functor_move_only_diagnostic/parse_functor_move_only_diagnostic:"
              "expected_error at parse_functor_move_only_diagnostic: move-only functor diagnostic failed",
          }));

  auto functor_move_only_diagnostic_raw_failed = functor_move_only_diagnostic_engine.run(Input{-5});
  pb_test_require(!functor_move_only_diagnostic_raw_failed.has_value());
  pb_test_require(functor_move_only_diagnostic_raw_failed.error().category ==
         pb::runtime::error_category::expected_error);
  pb_test_require(functor_move_only_diagnostic_raw_failed.error().stage.key ==
         "parse_functor_move_only_diagnostic");
  pb_test_require(functor_move_only_diagnostic_raw_failed.error().stage.name ==
         "parse_functor_move_only_diagnostic");
  pb_test_require(functor_move_only_diagnostic_raw_failed.error().message ==
         "move-only functor diagnostic failed");

  auto opaque_error_engine = pb::compile<OpaqueErrorPipeline>(pb::runtime::sequential{});
  recording_observer opaque_observer{};
  opaque_error_engine.set_observer(&opaque_observer);

  auto opaque_failed = opaque_error_engine.try_run(Input{-5});
  pb_test_require(!opaque_failed.has_value());
  pb_test_require(opaque_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(opaque_failed.error().stage.key == "parse_opaque");
  pb_test_require(opaque_failed.error().stage.name == "parse_opaque");
  pb_test_require(opaque_failed.error().message == "expected-like object reported an error");
  pb_test_require(pb::runtime::describe(opaque_failed.error()) ==
         "expected_error at parse_opaque: expected-like object reported an error");
  pb_test_require((opaque_observer.events == std::vector<std::string>{
                                           "start:parse_opaque/parse_opaque",
                                           "failure:parse_opaque/parse_opaque:expected_error at parse_opaque: "
                                           "expected-like object reported an error",
                                       }));

  auto diagnostic_error_engine = pb::compile<DiagnosticErrorPipeline>(pb::runtime::sequential{});
  recording_observer diagnostic_observer{};
  diagnostic_error_engine.set_observer(&diagnostic_observer);

  auto diagnostic_failed = diagnostic_error_engine.try_run(Input{-5});
  pb_test_require(!diagnostic_failed.has_value());
  pb_test_require(diagnostic_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(diagnostic_failed.error().stage.key == "parse_diagnostic");
  pb_test_require(diagnostic_failed.error().stage.name == "parse_diagnostic");
  pb_test_require(diagnostic_failed.error().message == "diagnostic parse failed");
  pb_test_require(pb::runtime::describe(diagnostic_failed.error()) ==
         "expected_error at parse_diagnostic: diagnostic parse failed");
  pb_test_require((diagnostic_observer.events == std::vector<std::string>{
                                               "start:parse_diagnostic/parse_diagnostic",
                                               "failure:parse_diagnostic/parse_diagnostic:expected_error at "
                                               "parse_diagnostic: diagnostic parse failed",
                                           }));

  auto result_move_only_error_engine = pb::compile<ResultMoveOnlyErrorPipeline>(pb::runtime::sequential{});
  recording_observer result_move_only_error_observer{};
  result_move_only_error_engine.set_observer(&result_move_only_error_observer);

  auto result_move_only_error_try_failed = result_move_only_error_engine.try_run(Input{-5});
  pb_test_require(!result_move_only_error_try_failed.has_value());
  pb_test_require(result_move_only_error_try_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(result_move_only_error_try_failed.error().stage.key == "parse_result_move_only_error");
  pb_test_require(result_move_only_error_try_failed.error().stage.name == "parse_result_move_only_error");
  pb_test_require(result_move_only_error_try_failed.error().message == "move-only result diagnostic failed");
  pb_test_require(pb::runtime::describe(result_move_only_error_try_failed.error()) ==
         "expected_error at parse_result_move_only_error: move-only result diagnostic failed");
  pb_test_require((result_move_only_error_observer.events ==
          std::vector<std::string>{
              "start:parse_result_move_only_error/parse_result_move_only_error",
              "failure:parse_result_move_only_error/parse_result_move_only_error:"
              "expected_error at parse_result_move_only_error: move-only result diagnostic failed",
          }));

  auto result_move_only_error_raw_failed = result_move_only_error_engine.run(Input{-5});
  pb_test_require(!result_move_only_error_raw_failed.has_value());
  pb_test_require(result_move_only_error_raw_failed.error().diagnostic.stage.key == "external.result");
  pb_test_require(result_move_only_error_raw_failed.error().diagnostic.stage.name == "ExternalResult");
  pb_test_require(result_move_only_error_raw_failed.error().diagnostic.category ==
         pb::runtime::error_category::stage_failure);
  pb_test_require(result_move_only_error_raw_failed.error().diagnostic.message == "move-only result diagnostic failed");
  pb_test_require(result_move_only_error_raw_failed.error().token != nullptr);
  pb_test_require(*result_move_only_error_raw_failed.error().token == 31);

  auto void_expected_engine = pb::compile<VoidExpectedPipeline>(pb::runtime::sequential{});
  recording_observer void_observer{};
  void_expected_engine.set_observer(&void_observer);

  auto void_ok = void_expected_engine.try_run(Input{5});
  pb_test_require(void_ok.has_value());
  pb_test_require(!void_ok.has_error());

  auto void_failed = void_expected_engine.try_run(Input{-5});
  pb_test_require(!void_failed.has_value());
  pb_test_require(void_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(void_failed.error().stage.key == "consume_void");
  pb_test_require(void_failed.error().stage.name == "consume_void");
  pb_test_require(void_failed.error().message == "expected-like object reported an error");
  pb_test_require(pb::runtime::describe(void_failed.error()) ==
         "expected_error at consume_void: expected-like object reported an error");
  pb_test_require((void_observer.events == std::vector<std::string>{
                                        "start:consume_void/consume_void",
                                        "success:consume_void/consume_void",
                                        "start:consume_void/consume_void",
                                        "failure:consume_void/consume_void:expected_error at consume_void: "
                                        "expected-like object reported an error",
                                    }));

  auto void_raw_ok = void_expected_engine.run(Input{5});
  pb_test_require(void_raw_ok.has_value());
  pb_test_require(!void_raw_ok.has_error());

  auto void_raw_failed = void_expected_engine.run(Input{-5});
  pb_test_require(!void_raw_failed.has_value());
  pb_test_require(void_raw_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(void_raw_failed.error().stage.key == "consume_void");
  pb_test_require(void_raw_failed.error().stage.name == "consume_void");
  pb_test_require(void_raw_failed.error().message == "expected-like object reported an error");

  auto void_move_only_engine = pb::compile<VoidMoveOnlyExpectedPipeline>(pb::runtime::sequential{});
  recording_observer void_move_only_observer{};
  void_move_only_engine.set_observer(&void_move_only_observer);

  auto void_move_only_failed = void_move_only_engine.try_run(Input{-5});
  pb_test_require(!void_move_only_failed.has_value());
  pb_test_require(void_move_only_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(void_move_only_failed.error().stage.key == "consume_void_move_only");
  pb_test_require(void_move_only_failed.error().stage.name == "consume_void_move_only");
  pb_test_require(void_move_only_failed.error().message == "expected-like object reported an error");
  pb_test_require(pb::runtime::describe(void_move_only_failed.error()) ==
         "expected_error at consume_void_move_only: expected-like object reported an error");
  pb_test_require((void_move_only_observer.events == std::vector<std::string>{
                                                  "start:consume_void_move_only/consume_void_move_only",
                                                  "failure:consume_void_move_only/consume_void_move_only:expected_error "
                                                  "at consume_void_move_only: expected-like object reported an error",
                                              }));

  auto void_move_only_raw_failed = void_move_only_engine.run(Input{-5});
  pb_test_require(!void_move_only_raw_failed.has_value());
  pb_test_require(void_move_only_raw_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(void_move_only_raw_failed.error().stage.key == "consume_void_move_only");
  pb_test_require(void_move_only_raw_failed.error().stage.name == "consume_void_move_only");
  pb_test_require(void_move_only_raw_failed.error().message == "expected-like object reported an error");

  auto direct_member_void_engine = pb::compile<DirectMemberVoidExpectedPipeline>(pb::runtime::sequential{});
  recording_observer direct_member_void_observer{};
  direct_member_void_engine.set_observer(&direct_member_void_observer);

  auto direct_member_void_ok = direct_member_void_engine.try_run(Input{5});
  pb_test_require(direct_member_void_ok.has_value());
  pb_test_require(!direct_member_void_ok.has_error());

  auto direct_member_void_failed = direct_member_void_engine.try_run(Input{-5});
  pb_test_require(!direct_member_void_failed.has_value());
  pb_test_require(direct_member_void_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(direct_member_void_failed.error().stage.key == "direct_member_consume_void");
  pb_test_require(direct_member_void_failed.error().stage.name == "direct_member_consume_void");
  pb_test_require(direct_member_void_failed.error().message == "member consume failed");
  pb_test_require(pb::runtime::describe(direct_member_void_failed.error()) ==
         "expected_error at direct_member_consume_void: member consume failed");
  pb_test_require((direct_member_void_observer.events == std::vector<std::string>{
                                                   "start:direct_member_consume_void/direct_member_consume_void",
                                                   "success:direct_member_consume_void/direct_member_consume_void",
                                                   "start:direct_member_consume_void/direct_member_consume_void",
                                                   "failure:direct_member_consume_void/direct_member_consume_void:"
                                                   "expected_error at direct_member_consume_void: member consume failed",
                                               }));

  auto direct_member_void_raw_ok = direct_member_void_engine.run(Input{5});
  pb_test_require(direct_member_void_raw_ok.has_value());
  pb_test_require(!direct_member_void_raw_ok.has_error());

  auto direct_member_void_raw_failed = direct_member_void_engine.run(Input{-5});
  pb_test_require(!direct_member_void_raw_failed.has_value());
  pb_test_require(direct_member_void_raw_failed.error().category == pb::runtime::error_category::expected_error);
  pb_test_require(direct_member_void_raw_failed.error().stage.key == "direct_member_consume_void");
  pb_test_require(direct_member_void_raw_failed.error().stage.name == "direct_member_consume_void");
  pb_test_require(direct_member_void_raw_failed.error().message == "member consume failed");

  auto failed = engine.run(Input{-2});
  pb_test_require(!failed.has_value());
  pb_test_require(failed.error().stage.key == "emit");
  pb_test_require(failed.error().stage.name == "emit_result");
  pb_test_require(failed.error().category == pb::runtime::error_category::stage_failure);
  pb_test_require(failed.error().message == "invalid parsed value");

  auto throwing_result_engine = pb::compile<ThrowingPipeline>(pb::runtime::sequential{});
  recording_observer observer{};
  throwing_result_engine.set_observer(&observer);

  auto result_caught = throwing_result_engine.try_run(Input{-200});
  pb_test_require(!result_caught.has_value());
  pb_test_require(result_caught.error().category == pb::runtime::error_category::exception);
  pb_test_require(result_caught.error().stage.name == "parse_input");
  pb_test_require(result_caught.error().stage.key == "parse_input");
  pb_test_require(result_caught.error().message == "parse_input_throwing failed");

  auto throwing_raw_engine = pb::compile<ThrowingPipelineRaw>(pb::runtime::sequential{});
  throwing_raw_engine.set_observer(&observer);

  try {
    (void)throwing_raw_engine.run(Input{-200});
    return 1;
  } catch (const std::runtime_error& error) {
    pb_test_require(std::string_view{error.what()} == "parse_input_throwing failed");
  }

  pb_test_require((observer.events == std::vector<std::string>{
              "start:parse_input/parse_input",
              "exception:parse_input/parse_input:parse_input_throwing failed",
              "start:parse_input/parse_input",
              "exception:parse_input/parse_input:parse_input_throwing failed",
          }));

  return 0;
}
