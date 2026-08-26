/// @file coroutine_stage_demo.cpp
/// @brief Demonstrates the synchronous coroutine stage adapter.
///
/// A `co_return`-style coroutine callable returning `pb::coro::sync_task<T>`
/// is adapted into a normal pipeline stage via `pb::coroutine_stage` /
/// `pb::adapt_coroutine`, then composed with ordinary functor stages in the
/// usual `from<>::then<>::to<>` DSL and executed on the sequential engine.
///
/// The adapter is SYNCHRONOUS: `sync_task<T>` is an eager task whose body runs
/// to completion immediately, so the value is available without any async
/// scheduler. (True async / sender-receiver backends remain roadmap-only.)
///
/// Runs as the `pb_coroutine_stage_demo` example target. main() returns 0.

#include <pb/pipeline.hpp>

#include <iostream>

namespace {

struct Text {
  int length{};
};

struct Parsed {
  int tokens{};
};

struct Scored {
  int score{};
};

struct Report {
  int score{};
};

// A coroutine stage: Text -> Parsed.
struct ParseCoroutine {
  pb::coro::sync_task<Parsed> operator()(Text in) const {
    co_return Parsed{in.length / 2};
  }
};

// A second coroutine stage: Parsed -> Scored.
struct ScoreCoroutine {
  pb::coro::sync_task<Scored> operator()(Parsed p) const {
    co_return Scored{p.tokens * 10};
  }
};

// An ordinary (non-coroutine) functor stage interleaved at the end.
struct Finalize {
  using input_type = Scored;
  using output_type = Report;
  static constexpr auto stage_name() noexcept { return "finalize"; }
  Report operator()(Scored s) const { return Report{s.score + 1}; }
};

using ParseStage = pb::coroutine_stage<ParseCoroutine, Text>;
using ScoreStage = pb::adapt_coroutine<ScoreCoroutine, Parsed>;

// Two coroutine stages composed with a normal stage in the type-level DSL.
using Pipeline =
    pb::from<Text>::then<ParseStage>::then<ScoreStage>::then<Finalize>::to<Report>;
static_assert(pb::valid<Pipeline>);

} // namespace

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  const auto report = engine.run(Text{.length = 20});
  // 20 -> Parsed{10} -> Scored{100} -> Report{101}
  std::cout << "coroutine pipeline report score = " << report.score << "\n";
  return 0;
}
