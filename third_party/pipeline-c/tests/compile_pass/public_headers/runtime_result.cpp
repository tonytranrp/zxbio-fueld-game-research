#include <pb/runtime/result.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

static_assert(pb::runtime::is_result_v<pb::runtime::result<int>>);
static_assert(pb::runtime::expected_like<pb::runtime::result<int>>);
static_assert(pb::runtime::expected_like<pb::runtime::result<void>>);
static_assert(std::is_same_v<pb::runtime::result<int>::value_type, int>);
static_assert(std::is_same_v<pb::runtime::result<int>::error_type, pb::runtime::error>);
static_assert(std::is_same_v<pb::runtime::result<void>::value_type, void>);
static_assert(std::is_same_v<pb::runtime::result<void>::error_type, pb::runtime::error>);

namespace {

struct public_expected {
  using value_type = int;
  using error_type = std::string;

  bool ok{};
  int value_{};
  std::string error_{};

  [[nodiscard]] bool has_value() const { return ok; }
  [[nodiscard]] int value() const { return value_; }
  [[nodiscard]] const std::string& error() const { return error_; }
};

} // namespace

int pb_public_header_runtime_result() {
  pb::runtime::result<int> ok{7};
  const pb::runtime::result<int>& const_ok = ok;
  if (!const_ok.has_value() || const_ok.has_error() || !static_cast<bool>(const_ok)) return 1;
  if (const_ok.value() != 7 || const_ok.value_or(99) != 7) return 2;
  if (const_ok.error_or(pb::runtime::error{.message = "fallback"}).message != "fallback") return 3;

  pb::runtime::result<int> failed{pb::runtime::error{.message = "bad"}};
  if (failed.has_value() || !failed.has_error() || static_cast<bool>(failed)) return 4;
  if (failed.value_or(99) != 99 || failed.error().message != "bad") return 5;
  if (std::move(failed).error_or(pb::runtime::error{.message = "fallback"}).message != "bad") return 6;

  auto moved_value = pb::runtime::result<std::unique_ptr<int>>{std::make_unique<int>(21)};
  if (*std::move(moved_value).value_or(std::make_unique<int>(0)) != 21) return 7;

  auto moved_fallback = pb::runtime::result<std::unique_ptr<int>>{pb::runtime::error{.message = "missing"}};
  if (*std::move(moved_fallback).value_or(std::make_unique<int>(34)) != 34) return 8;

  auto void_ok = pb::runtime::result<void>{};
  if (!void_ok.has_value() || void_ok.has_error()) return 9;
  if (void_ok.error_or(pb::runtime::error{.message = "void fallback"}).message != "void fallback") return 10;

  auto expected_ok = pb::runtime::to_result(public_expected{.ok = true, .value_ = 44});
  if (!expected_ok.has_value() || expected_ok.value() != 44) return 11;

  auto expected_failed = pb::runtime::make_result(public_expected{.ok = false, .error_ = "expected bad"});
  if (!expected_failed.has_error()) return 12;
  if (expected_failed.error().category != pb::runtime::error_category::expected_error) return 13;
  if (expected_failed.error().message != "expected bad") return 14;

  return 0;
}
