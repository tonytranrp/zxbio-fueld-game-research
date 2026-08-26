#include <pb/pipeline.hpp>

#include <concepts>
#include <string_view>
#include <type_traits>

// ─────────────────────────────────────────────────────────────
// Compile-time benchmark: branch + fan-in heavy pipeline.
//
// The existing chain_5 / chain_50 translation units only cover
// LINEAR pipelines. This TU instantiates a representative
// BRANCH (~10 cases) followed by a FAN-IN join over every case,
// plus descriptor-view and export-helper instantiation, so the
// build-time cost of the branch / join / fan-in machinery is
// measured specifically rather than only the sequential path.
//
// Like the chain_N benches this is a "compile-pass" style TU:
// the metric is *build time*, the static_asserts merely keep the
// instantiated shape honest, and main() is trivial.
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

// ── Ten branch predicates p0..p9 ──────────────────────────────
struct p0 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 0; } };
struct p1 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 1; } };
struct p2 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 2; } };
struct p3 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 3; } };
struct p4 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 4; } };
struct p5 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 5; } };
struct p6 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 6; } };
struct p7 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 7; } };
struct p8 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 8; } };
struct p9 { using input_type = request; using output_type = bool; bool operator()(const request& r) const { return r.route == 9; } };

// ── Ten branch case stages r0..r9 (all request -> result) ─────
struct r0 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 0}; } };
struct r1 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 1}; } };
struct r2 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 2}; } };
struct r3 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 3}; } };
struct r4 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 4}; } };
struct r5 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 5}; } };
struct r6 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 6}; } };
struct r7 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 7}; } };
struct r8 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 8}; } };
struct r9 { using input_type = request; using output_type = result; result operator()(request r) const { return {r.value + 9}; } };

using c0 = pb::case_<p0>::then<r0>;
using c1 = pb::case_<p1>::then<r1>;
using c2 = pb::case_<p2>::then<r2>;
using c3 = pb::case_<p3>::then<r3>;
using c4 = pb::case_<p4>::then<r4>;
using c5 = pb::case_<p5>::then<r5>;
using c6 = pb::case_<p6>::then<r6>;
using c7 = pb::case_<p7>::then<r7>;
using c8 = pb::case_<p8>::then<r8>;
using c9 = pb::case_<p9>::then<r9>;

// ── Fan-in join over all ten case results ─────────────────────
using fan_in_input =
    pb::fan_in_results<pb::fan_in_case_result<0, result>,
                       pb::fan_in_case_result<1, result>,
                       pb::fan_in_case_result<2, result>,
                       pb::fan_in_case_result<3, result>,
                       pb::fan_in_case_result<4, result>,
                       pb::fan_in_case_result<5, result>,
                       pb::fan_in_case_result<6, result>,
                       pb::fan_in_case_result<7, result>,
                       pb::fan_in_case_result<8, result>,
                       pb::fan_in_case_result<9, result>>;

struct fan_in_join {
  using input_type = fan_in_input;
  using output_type = outcome;
  outcome operator()(fan_in_input in) const {
    int total = 0;
    if (in.template get<0>().has_value()) total += in.template get<0>().get().value;
    if (in.template get<1>().has_value()) total += in.template get<1>().get().value;
    if (in.template get<2>().has_value()) total += in.template get<2>().get().value;
    if (in.template get<3>().has_value()) total += in.template get<3>().get().value;
    if (in.template get<4>().has_value()) total += in.template get<4>().get().value;
    if (in.template get<5>().has_value()) total += in.template get<5>().get().value;
    if (in.template get<6>().has_value()) total += in.template get<6>().get().value;
    if (in.template get<7>().has_value()) total += in.template get<7>().get().value;
    if (in.template get<8>().has_value()) total += in.template get<8>().get().value;
    if (in.template get<9>().has_value()) total += in.template get<9>().get().value;
    return {total};
  }
};

using expected_fan_in =
    pb::fan_in_output_t<c0, c1, c2, c3, c4, c5, c6, c7, c8, c9>;

// ── The branch + fan-in pipeline under measurement ────────────
using branch_pipeline =
    pb::from<request>::branch<c0, c1, c2, c3, c4, c5, c6, c7, c8, c9>
        ::fan_in<fan_in_join>::to<outcome>;

// Alias-spelling variant exercises the join_all DSL surface too.
using join_all_pipeline =
    pb::from<request>::branch<c0, c1, c2, c3, c4, c5, c6, c7, c8, c9>
        ::join_all<fan_in_join>::to<outcome>;

// ─────────────────────────────────────────────────────────────
// Verify the instantiated shape (keeps the benchmark honest).
// ─────────────────────────────────────────────────────────────
static_assert(pb::valid<branch_pipeline>);
static_assert(pb::valid<join_all_pipeline>);
static_assert(std::same_as<expected_fan_in, fan_in_input>);
static_assert(fan_in_input::case_count == 10);
static_assert(std::same_as<pb::pipeline_input_t<branch_pipeline>, request>);
static_assert(std::same_as<pb::pipeline_output_t<branch_pipeline>, outcome>);
static_assert(std::same_as<pb::pipeline_output_t<join_all_pipeline>, outcome>);

// ─────────────────────────────────────────────────────────────
// Exercise descriptor-view + export-helper instantiation so the
// branch/join/fan-in metadata machinery is measured, not just the
// runtime topology.
// ─────────────────────────────────────────────────────────────
static_assert(pb::descriptor_view<branch_pipeline>().stage_records().size() >= 1);

[[maybe_unused]] inline void exercise_exports() {
  const auto json = pb::to_json<branch_pipeline>();
  const auto dot = pb::to_dot<branch_pipeline>();
  const auto text = pb::to_text<branch_pipeline>();
  (void)std::string_view{json};
  (void)std::string_view{dot};
  (void)std::string_view{text};
}

} // namespace

int main() { return 0; }
