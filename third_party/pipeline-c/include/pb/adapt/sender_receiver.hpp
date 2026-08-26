#pragma once

#include <exception>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#include "pb/core/concepts.hpp"

/// @file
/// @brief Dependency-free synchronous sender/receiver adapter scaffold.
///
/// This header intentionally does **not** implement a full stdexec/P2300
/// backend.  It provides the smallest standard-library-only seam Pipeline-c++
/// can test today: a sender-like object that can be connected to a receiver and
/// started synchronously, plus a pipeline stage adapter that unwraps exactly one
/// `set_value` into the stage output.  Real asynchronous scheduling, completion
/// schedulers, environments, and multi-value senders remain roadmap-only.
namespace pb {

/// Exception thrown when a synchronous sender completes with `set_stopped()`.
class sender_stopped : public std::runtime_error {
public:
  sender_stopped() : std::runtime_error{"pb::sync_sender_stage: sender stopped"} {}
};

/// Exception thrown when a synchronous sender starts but never calls
/// `set_value`, `set_error`, or `set_stopped`.
class sender_no_value : public std::runtime_error {
public:
  sender_no_value() : std::runtime_error{"pb::sync_sender_stage: sender produced no value"} {}
};

namespace adapt_detail {

template <class Operation>
concept sync_operation_state = requires(Operation op) {
  op.start();
};

template <class Sender, class Receiver>
concept sync_sender_to = requires(Sender sender, Receiver receiver) {
  { std::move(sender).connect(std::move(receiver)) } -> sync_operation_state;
};

template <class Factory, class Input>
concept sync_sender_factory =
    std::default_initializable<Factory> &&
    requires { typename Factory::output_type; } &&
    requires(Factory factory, Input input) {
      factory(std::move(input));
    };

} // namespace adapt_detail

/// Minimal sender that synchronously completes with one value.
///
/// This is a testable building block for adapter authors and examples; it is
/// deliberately value-only and single-shot.  Use `pb::sync_just(value)` to build
/// one without naming the class template.
template <class T>
class sync_value_sender {
public:
  using value_type = T;

  explicit sync_value_sender(T value) : value_{std::move(value)} {}

  template <class Receiver>
  struct operation {
    T value;
    Receiver receiver;

    void start() noexcept(noexcept(receiver.set_value(std::move(value)))) {
      receiver.set_value(std::move(value));
    }
  };

  template <class Receiver>
  [[nodiscard]] auto connect(Receiver receiver) && -> operation<std::remove_cvref_t<Receiver>> {
    return operation<std::remove_cvref_t<Receiver>>{std::move(value_), std::move(receiver)};
  }

private:
  T value_;
};

/// Build a synchronous one-value sender.
template <class T>
[[nodiscard]] auto sync_just(T&& value) -> sync_value_sender<std::remove_cvref_t<T>> {
  return sync_value_sender<std::remove_cvref_t<T>>{std::forward<T>(value)};
}

/// Adapt a synchronous sender factory into a Pipeline-c++ stage.
///
/// `Factory` must be default-constructible, expose `using output_type = ...`,
/// and return a sender-like object from `Factory{}(Input)`.  The sender is
/// connected to a receiver with `set_value(output_type)`, `set_error`, and
/// `set_stopped`; `operation.start()` must complete before returning.
///
/// This is a **scaffold** for dependency-free sender/receiver experimentation,
/// not a claim of full stdexec/P2300 backend support.
template <class Factory, class Input>
struct sync_sender_stage {
  static_assert(adapt_detail::sync_sender_factory<Factory, Input>,
                "pb::sync_sender_stage: Factory must be default-constructible, expose "
                "output_type, and be invocable with Input to produce a sender-like object");
  static_assert(!std::is_void_v<typename Factory::output_type>,
                "pb::sync_sender_stage: void-valued senders are not supported by this "
                "value-only scaffold");

  using input_type = Input;
  using output_type = typename Factory::output_type;
  using error_type = no_error;

  static constexpr std::string_view stage_name() noexcept { return "sync_sender_stage"; }

  [[nodiscard]] output_type operator()(input_type input) const {
    std::optional<output_type> value;
    std::exception_ptr error;
    bool stopped = false;

    struct receiver {
      std::optional<output_type>* value;
      std::exception_ptr* error;
      bool* stopped;

      void set_value(output_type output) { value->emplace(std::move(output)); }
      void set_error(std::exception_ptr incoming) noexcept { *error = std::move(incoming); }
      void set_stopped() noexcept { *stopped = true; }
    };

    auto sender = Factory{}(std::move(input));
    auto operation = std::move(sender).connect(receiver{&value, &error, &stopped});
    operation.start();

    if (error) {
      std::rethrow_exception(error);
    }
    if (stopped) {
      throw sender_stopped{};
    }
    if (!value.has_value()) {
      throw sender_no_value{};
    }
    return std::move(*value);
  }
};

} // namespace pb
