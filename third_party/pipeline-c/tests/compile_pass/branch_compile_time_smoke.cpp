#include <pb/pipeline.hpp>

#include <concepts>
#include <type_traits>

// ─────────────────────────────────────────────────────────────
// Compile-pass smoke for the branch / fan-in compile-time
// benchmark surface. Mirrors bench/compile_time/branch_fan_in_10
// at a smaller scale so the branch compile-time coverage also
// runs inside the default test suite (the bench TUs build but are
// not part of every ctest run). Static-asserts the basic shape.
// ─────────────────────────────────────────────────────────────

namespace {

struct request {
  int route{};
  int value{};
};
struct result {
  int value{};
};
struct outcome {
  int value{};
};

struct p0 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 0; } };
struct p1 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 1; } };
struct p2 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 2; } };

struct r0 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 0}; } };
struct r1 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 1}; } };
struct r2 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 2}; } };

using c0 = pb::case_<p0>::then<r0>;
using c1 = pb::case_<p1>::then<r1>;
using c2 = pb::case_<p2>::then<r2>;

using fan_in_input =
    pb::fan_in_results<pb::fan_in_case_result<0, result>,
                       pb::fan_in_case_result<1, result>,
                       pb::fan_in_case_result<2, result>>;

struct fan_in_join {
  using input_type = fan_in_input;
  using output_type = outcome;
  outcome operator()(fan_in_input in) const {
    int total = 0;
    if (in.template get<0>().has_value()) total += in.template get<0>().get().value;
    if (in.template get<1>().has_value()) total += in.template get<1>().get().value;
    if (in.template get<2>().has_value()) total += in.template get<2>().get().value;
    return {total};
  }
};

using expected_fan_in = pb::fan_in_output_t<c0, c1, c2>;

using branch_pipeline =
    pb::from<request>::branch<c0, c1, c2>::fan_in<fan_in_join>::to<outcome>;
using join_all_pipeline =
    pb::from<request>::branch<c0, c1, c2>::join_all<fan_in_join>::to<outcome>;

// Basic branch + fan-in shape.
static_assert(pb::valid<branch_pipeline>);
static_assert(pb::valid<join_all_pipeline>);
static_assert(std::same_as<expected_fan_in, fan_in_input>);
static_assert(fan_in_input::case_count == 3);
static_assert(std::same_as<pb::pipeline_input_t<branch_pipeline>, request>);
static_assert(std::same_as<pb::pipeline_output_t<branch_pipeline>, outcome>);
static_assert(std::same_as<pb::pipeline_output_t<join_all_pipeline>, outcome>);
static_assert(pb::descriptor_view<branch_pipeline>().stage_records().size() >= 1);

} // namespace

int main() { return 0; }
