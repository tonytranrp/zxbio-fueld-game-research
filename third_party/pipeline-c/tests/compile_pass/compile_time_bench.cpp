#include <pb/pipeline.hpp>
#include <pb/core/compile_time_bench.hpp>

#include <concepts>

// ─────────────────────────────────────────────────────────────
// Shared value type for all benchmark stages.
// ─────────────────────────────────────────────────────────────
struct bench_value {
  int n{};
};

// ─────────────────────────────────────────────────────────────
// 5-stage chain: the "trivial" compile-time baseline.
// ─────────────────────────────────────────────────────────────

struct b0 {
  using input_type  = bench_value;
  using output_type = bench_value;
  [[nodiscard]] constexpr bench_value
  operator()(bench_value v) const noexcept {
    return {v.n + 1};
  }
};
struct b1 {
  using input_type  = bench_value;
  using output_type = bench_value;
  [[nodiscard]] constexpr bench_value
  operator()(bench_value v) const noexcept {
    return {v.n + 1};
  }
};
struct b2 {
  using input_type  = bench_value;
  using output_type = bench_value;
  [[nodiscard]] constexpr bench_value
  operator()(bench_value v) const noexcept {
    return {v.n + 1};
  }
};
struct b3 {
  using input_type  = bench_value;
  using output_type = bench_value;
  [[nodiscard]] constexpr bench_value
  operator()(bench_value v) const noexcept {
    return {v.n + 1};
  }
};
struct b4 {
  using input_type  = bench_value;
  using output_type = bench_value;
  [[nodiscard]] constexpr bench_value
  operator()(bench_value v) const noexcept {
    return {v.n + 1};
  }
};

using Chain5 =
    pb::from<bench_value>::then<b0>::then<b1>::then<b2>::then<b3>::then<b4>::to<bench_value>;

// ─────────────────────────────────────────────────────────────
// Verify 5-stage pipeline compiles and is valid.
// ─────────────────────────────────────────────────────────────
static_assert(pb::valid<Chain5>);
static_assert(pb::pipeline_size_v<Chain5> == 5);
static_assert(std::same_as<pb::pipeline_input_t<Chain5>, bench_value>);
static_assert(std::same_as<pb::pipeline_output_t<Chain5>, bench_value>);
static_assert(pb::descriptor_view<Chain5>().stage_records().size() == 5);
static_assert(pb::descriptor_view<Chain5>().edge_records().size() == 4);

// Verify baseline measurement matches 5-stage expectation
static_assert(pb::bench::stage_chain_bench<5>::template_depth == 5);
static_assert(pb::bench::stage_chain_bench<5>::is_trivial);
static_assert(pb::bench::verify_chain_budget<5>());

// ─────────────────────────────────────────────────────────────
// 50-stage chain: the "heavy" compile-time stress test.
// Uses 50 individually-named stages s0..s49 to exercise
// template instantiation at realistic scale.
// ─────────────────────────────────────────────────────────────

struct s0  { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s1  { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s2  { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s3  { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s4  { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s5  { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s6  { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s7  { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s8  { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s9  { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s10 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s11 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s12 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s13 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s14 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s15 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s16 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s17 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s18 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s19 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s20 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s21 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s22 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s23 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s24 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s25 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s26 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s27 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s28 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s29 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s30 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s31 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s32 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s33 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s34 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s35 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s36 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s37 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s38 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s39 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s40 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s41 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s42 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s43 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s44 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s45 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s46 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s47 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s48 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };
struct s49 { using input_type = bench_value; using output_type = bench_value; [[nodiscard]] constexpr bench_value operator()(bench_value v) const noexcept { return {v.n + 1}; } };

using Chain50 =
    pb::from<bench_value>::then<s0>::then<s1>::then<s2>::then<s3>::then<s4>::then<s5>::then<s6>::then<s7>::then<s8>::then<s9>::then<s10>::then<s11>::then<s12>::then<s13>::then<s14>::then<s15>::then<s16>::then<s17>::then<s18>::then<s19>::then<s20>::then<s21>::then<s22>::then<s23>::then<s24>::then<s25>::then<s26>::then<s27>::then<s28>::then<s29>::then<s30>::then<s31>::then<s32>::then<s33>::then<s34>::then<s35>::then<s36>::then<s37>::then<s38>::then<s39>::then<s40>::then<s41>::then<s42>::then<s43>::then<s44>::then<s45>::then<s46>::then<s47>::then<s48>::then<s49>::to<bench_value>;

// ─────────────────────────────────────────────────────────────
// Verify 50-stage pipeline compiles and is valid.
// ─────────────────────────────────────────────────────────────
static_assert(pb::valid<Chain50>);
static_assert(pb::pipeline_size_v<Chain50> == 50);
static_assert(std::same_as<pb::pipeline_input_t<Chain50>, bench_value>);
static_assert(std::same_as<pb::pipeline_output_t<Chain50>, bench_value>);
static_assert(pb::descriptor_view<Chain50>().stage_records().size() == 50);
static_assert(pb::descriptor_view<Chain50>().edge_records().size() == 49);

// Verify baseline measurement matches 50-stage expectation
static_assert(pb::bench::stage_chain_bench<50>::template_depth == 50);
static_assert(pb::bench::stage_chain_bench<50>::is_heavy);
static_assert(!pb::bench::stage_chain_bench<50>::is_trivial);
static_assert(pb::bench::verify_chain_budget<50>());

// ─────────────────────────────────────────────────────────────
// Verify the baseline record constants match expected sizes.
// ─────────────────────────────────────────────────────────────
static_assert(pb::bench::trivial_baseline::stages == 5);
static_assert(pb::bench::trivial_baseline::depth == 5);
static_assert(pb::bench::heavy_baseline::stages == 50);
static_assert(pb::bench::heavy_baseline::depth == 50);

// ─────────────────────────────────────────────────────────────
// Verify compile_counter instantiation depth tracking works.
// ─────────────────────────────────────────────────────────────
static_assert(pb::bench::compile_counter<0>::value == 0);
static_assert(pb::bench::compile_counter<50>::value == 50);
static_assert(pb::bench::instantiation_depth_v<pb::bench::compile_counter<50>> == 50);

int main() { return 0; }
