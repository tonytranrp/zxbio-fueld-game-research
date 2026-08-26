#include <pb/pipeline.hpp>

#include <cassert>
#include <string>

namespace {

struct Input {
  int route{};
  int payload{};
};

struct Parsed {
  int value{};
};

struct Reviewed {
  int value{};
};

struct Done {
  std::string text{};
};

struct IsParseA {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return input.route == 0; }
};

struct IsReview {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return input.route == 1; }
};

struct IsParseB {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return input.route == 2; }
};

struct ParseA {
  using input_type = Input;
  using output_type = Parsed;
  static constexpr auto stage_key() noexcept { return "parse-a"; }
  static constexpr auto stage_name() noexcept { return "Parse A"; }
  Parsed operator()(Input input) const { return Parsed{input.payload + 100}; }
};

struct Review {
  using input_type = Input;
  using output_type = Reviewed;
  static constexpr auto stage_key() noexcept { return "review"; }
  static constexpr auto stage_name() noexcept { return "Review"; }
  Reviewed operator()(Input input) const { return Reviewed{input.payload + 200}; }
};

struct ParseB {
  using input_type = Input;
  using output_type = Parsed;
  static constexpr auto stage_key() noexcept { return "parse-b"; }
  static constexpr auto stage_name() noexcept { return "Parse B"; }
  Parsed operator()(Input input) const { return Parsed{input.payload + 300}; }
};

enum class JoinOverload { none, parsed, reviewed };

struct TypeListJoin {
  using input_type = pb::meta::type_list<Parsed, Reviewed, Parsed>;
  using output_type = Done;
  static constexpr auto stage_key() noexcept { return "type-list-join"; }
  static constexpr auto stage_name() noexcept { return "Type-list join"; }

  Done operator()(Parsed parsed) const {
    last_overload() = JoinOverload::parsed;
    ++parsed_calls();
    return Done{"parsed-shared-overload:" + std::to_string(parsed.value)};
  }

  Done operator()(Reviewed reviewed) const {
    last_overload() = JoinOverload::reviewed;
    ++reviewed_calls();
    return Done{"reviewed:" + std::to_string(reviewed.value)};
  }

  static int& parsed_calls() {
    static int count = 0;
    return count;
  }

  static int& reviewed_calls() {
    static int count = 0;
    return count;
  }

  static JoinOverload& last_overload() {
    static auto overload = JoinOverload::none;
    return overload;
  }
};

using CaseA = pb::case_<IsParseA>::then<ParseA>;
using CaseB = pb::case_<IsReview>::then<Review>;
using CaseC = pb::case_<IsParseB>::then<ParseB>;
using Pipeline = pb::from<Input>::branch<CaseA, CaseB, CaseC>::join<TypeListJoin>::to<Done>;

static_assert(pb::valid<Pipeline>);

void reset_counts() {
  TypeListJoin::parsed_calls() = 0;
  TypeListJoin::reviewed_calls() = 0;
  TypeListJoin::last_overload() = JoinOverload::none;
}

} // namespace

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  reset_counts();
  auto first = engine.run(Input{0, 1});
  assert(first.has_value());
  assert(first.value().text == "parsed-shared-overload:101");
  assert(TypeListJoin::parsed_calls() == 1);
  assert(TypeListJoin::reviewed_calls() == 0);

  auto second = engine.run(Input{1, 2});
  assert(second.has_value());
  assert(second.value().text == "reviewed:202");
  assert(TypeListJoin::last_overload() == JoinOverload::reviewed);
  assert(TypeListJoin::parsed_calls() == 1);
  assert(TypeListJoin::reviewed_calls() == 1);

  auto third = engine.run(Input{2, 3});
  assert(third.has_value());
  assert(third.value().text == "parsed-shared-overload:303");
  assert(TypeListJoin::parsed_calls() == 2);
  assert(TypeListJoin::reviewed_calls() == 1);

  auto no_match = engine.try_run(Input{99, 4});
  assert(!no_match.has_value());

  return 0;
}
