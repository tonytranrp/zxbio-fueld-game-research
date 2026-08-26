#pragma once

/// @file  cancellation.hpp
/// @brief Cooperative cancellation token for the thread-pool fan-in backend.
///
/// @note Cancellation in this library is **COOPERATIVE**. A
/// @ref pb::cancellation_source signals an atomic flag; cooperating
/// scheduling code (the thread-pool fan-in backend) checks that flag at safe
/// observation points — specifically *before* a case stage begins executing —
/// and skips not-yet-started work when a stop has been requested.
///
/// Truly *preemptive* interruption of a stage that is already executing is
/// **out of scope**: portable standard C++ provides no way to forcibly abort a
/// running function on another thread without cooperation. A long-running stage
/// that has already begun will therefore run to completion even after a stop is
/// requested; only cases that have not yet started are cancelled.

#include <atomic>
#include <memory>
#include <string_view>

namespace pb {

/// Schema/version tag for the cooperative cancellation surface.
///
/// @note Cancellation is COOPERATIVE; preemptive interruption of an
/// already-running stage is out of scope (see @ref cancellation.hpp).
inline constexpr std::string_view cancellation_schema_version = "pb.cancel.v1";

/// A cheap, copyable view onto a cancellation flag.
///
/// A default-constructed token holds no flag: @ref can_be_cancelled returns
/// `false` and @ref stop_requested always returns `false`, so a default token
/// is *never* considered stopped. Tokens are lock-free and thread-safe to
/// observe from any thread.
class cancellation_token {
public:
  cancellation_token() noexcept = default;

  /// @return `true` once a stop has been requested through the owning
  /// @ref cancellation_source. Always `false` for a default-constructed token.
  [[nodiscard]] bool stop_requested() const noexcept {
    return flag_ != nullptr && flag_->load(std::memory_order_acquire);
  }

  /// @return `true` if this token is associated with a cancellation source and
  /// could therefore ever observe a stop request.
  [[nodiscard]] bool can_be_cancelled() const noexcept { return flag_ != nullptr; }

private:
  friend class cancellation_source;
  explicit cancellation_token(std::shared_ptr<std::atomic<bool>> flag) noexcept : flag_{std::move(flag)} {}

  std::shared_ptr<std::atomic<bool>> flag_{};
};

/// Owns a cancellation flag and hands out cheap @ref cancellation_token views.
///
/// All operations are lock-free and safe to invoke concurrently with token
/// observation on other threads.
class cancellation_source {
public:
  cancellation_source() : flag_{std::make_shared<std::atomic<bool>>(false)} {}

  /// Request cooperative cancellation. Idempotent.
  void request_stop() noexcept { flag_->store(true, std::memory_order_release); }

  /// @return `true` once @ref request_stop has been called.
  [[nodiscard]] bool stop_requested() const noexcept { return flag_->load(std::memory_order_acquire); }

  /// @return a cheap, copyable token sharing this source's flag.
  [[nodiscard]] cancellation_token token() const noexcept { return cancellation_token{flag_}; }

private:
  std::shared_ptr<std::atomic<bool>> flag_;
};

} // namespace pb
