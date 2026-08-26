#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "pb/core/pipeline_state.hpp"

/// @file
/// @brief Fan-in clone / projection policies.
///
/// Fan-in branches need a story for non-copyable input values: each passing
/// case receives the input, but std::unique_ptr-like inputs cannot be
/// duplicated implicitly.  The runtime supports two strategies today:
///
///   - **Copy.**  When the pipeline's input type is copy-constructible, each
///     case receives its own per-case copy of the input.  Default for
///     trivially copyable, by-value POD types.
///   - **Borrow.**  When the pipeline's input type is move-only and every
///     branch case stage accepts `const input_type&`, the runtime passes a
///     reference into each case stage instead of cloning.
///
/// Both strategies are deduced from the stage signatures.  This header adds
/// the public-facing policy surface that lets users opt into a *third*
/// strategy without rewriting their case stages:
///
///   - **Shared view.**  `pb::shared_view<T>` wraps an owned non-copyable
///     value behind a `std::shared_ptr<const T>` so the wrapper itself is
///     trivially copyable.  Users construct it once at the boundary and pass
///     it as the pipeline input; every fan-in case receives a copy of the
///     wrapper (cheap) and accesses the underlying value via `operator*` or
///     `operator->`.
///
/// The introspection variable templates expose which strategy a particular
/// fan-in branch_node uses, so static_asserts in user code can pin the
/// expected lowering.

namespace pb {

/// Copyable wrapper for owned non-copyable values that need to participate
/// in fan-in or other multi-case execution.  The wrapper itself is cheap to
/// copy (shared_ptr); the underlying T is owned by the first instance and
/// shared with every copy.  Const-only access keeps the wrapper safe under
/// concurrent fan-in cases.
template <class T>
class shared_view {
public:
  using value_type = T;

  /// Default-constructed shared_view has no underlying value.
  shared_view() = default;

  /// Take ownership of a value.  Moves into a shared_ptr so subsequent copies
  /// of the shared_view are cheap.
  explicit shared_view(T value) : owned_{std::make_shared<T>(std::move(value))} {}

  /// Adopt an existing shared_ptr.  Useful when the caller already manages
  /// shared ownership and just wants to hand a view to a pipeline.
  explicit shared_view(std::shared_ptr<const T> owned) noexcept : owned_{std::move(owned)} {}

  shared_view(const shared_view&) = default;
  shared_view(shared_view&&) noexcept = default;
  shared_view& operator=(const shared_view&) = default;
  shared_view& operator=(shared_view&&) noexcept = default;
  ~shared_view() = default;

  [[nodiscard]] auto get() const noexcept -> const T* { return owned_.get(); }
  [[nodiscard]] auto operator*() const noexcept -> const T& { return *owned_; }
  [[nodiscard]] auto operator->() const noexcept -> const T* { return owned_.get(); }
  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(owned_); }

  /// Number of shared owners of the underlying value, for diagnostics only.
  [[nodiscard]] auto use_count() const noexcept -> long { return owned_.use_count(); }

  [[nodiscard]] friend auto operator==(const shared_view& lhs, const shared_view& rhs) noexcept -> bool {
    return lhs.owned_ == rhs.owned_;
  }

private:
  std::shared_ptr<const T> owned_{};
};

/// Convenience factory.  Lets users write `pb::make_shared_view(std::move(unique))`
/// without spelling the type parameter.
template <class T>
[[nodiscard]] auto make_shared_view(T value) -> shared_view<std::remove_cvref_t<T>> {
  return shared_view<std::remove_cvref_t<T>>{std::move(value)};
}

namespace detail {

template <class T>
struct is_shared_view : std::false_type {};

template <class T>
struct is_shared_view<shared_view<T>> : std::true_type {};

} // namespace detail

/// True when `T` is some instantiation of `pb::shared_view`.
template <class T>
inline constexpr bool is_shared_view_v = detail::is_shared_view<std::remove_cvref_t<T>>::value;

/// Default clone policy for `pb::unique_clone`.  Deep-copies a value by
/// invoking `T`'s copy constructor.  Specialise / replace with a custom
/// functor when `T` is move-only-but-cloneable (e.g. carries a
/// `std::unique_ptr` it can deep-copy on demand).
template <class T>
struct copy_clone {
  [[nodiscard]] auto operator()(const T& value) const -> T { return T{value}; }
};

/// Copyable wrapper for an *owned* value that participates in fan-in (or other
/// multi-case execution) via the runtime's **copy** strategy while handing each
/// passing case its own **independent** value.
///
/// Where `pb::shared_view<T>` makes the wrapper copyable by *sharing* one
/// underlying value behind a `std::shared_ptr<const T>`, `unique_clone<T>`
/// makes the wrapper copyable by *deep-copying* its underlying value on every
/// copy.  Each copy therefore owns a fully independent `T`: mutating one case's
/// value never aliases another's.  This suits a `T` that is expensive- or
/// unsafe-to-share, or a `T` that is move-only-but-cloneable (supply a custom
/// `Clone` functor whose `operator()(const T&) -> T` produces the deep copy).
///
/// The wrapper's COPY constructor produces an independent deep copy by
/// invoking `Clone{}(const T&) -> T`; its MOVE constructor moves the owned `T`
/// cheaply without cloning.  Because the wrapper is copy-constructible the
/// fan-in runtime selects its **copy** strategy
/// (`pb::fan_in_uses_copy_v == true`), giving every passing case its own
/// owned value.
template <class T, class Clone = copy_clone<T>>
class unique_clone {
public:
  using value_type  = T;
  using clone_type  = Clone;

  /// Take ownership of a value by moving it into the wrapper.
  explicit unique_clone(T value) : owned_{std::move(value)} {}

  /// Copy constructor: produce an INDEPENDENT deep copy of the owned value via
  /// `Clone`.  This is what flows a fresh, independently-owned `T` into each
  /// passing fan-in case under the copy strategy.
  unique_clone(const unique_clone& other) : owned_{Clone{}(other.owned_)} {}

  /// Move constructor: move the owned value cheaply, no clone performed.
  unique_clone(unique_clone&&) noexcept(std::is_nothrow_move_constructible_v<T>) = default;

  /// Copy assignment: replace the owned value with an independent deep copy.
  unique_clone& operator=(const unique_clone& other) {
    if (this != &other) {
      owned_ = Clone{}(other.owned_);
    }
    return *this;
  }

  /// Move assignment: move the owned value cheaply, no clone performed.
  unique_clone& operator=(unique_clone&&) noexcept(std::is_nothrow_move_assignable_v<T>) = default;

  ~unique_clone() = default;

  [[nodiscard]] auto get() noexcept -> T& { return owned_; }
  [[nodiscard]] auto get() const noexcept -> const T& { return owned_; }
  [[nodiscard]] auto operator*() noexcept -> T& { return owned_; }
  [[nodiscard]] auto operator*() const noexcept -> const T& { return owned_; }
  [[nodiscard]] auto operator->() noexcept -> T* { return std::addressof(owned_); }
  [[nodiscard]] auto operator->() const noexcept -> const T* { return std::addressof(owned_); }

  /// Produce an independent deep copy of the owned value through `Clone`.
  [[nodiscard]] auto clone() const -> T { return Clone{}(owned_); }

private:
  T owned_;
};

/// Convenience factory using the default deep-copy clone policy.  Lets users
/// write `pb::make_unique_clone(std::move(heavy))` without spelling the type.
template <class T>
[[nodiscard]] auto make_unique_clone(T value) -> unique_clone<std::remove_cvref_t<T>> {
  return unique_clone<std::remove_cvref_t<T>>{std::move(value)};
}

/// Convenience factory selecting an explicit `Clone` policy, for move-only or
/// otherwise non-copy-constructible `T` that nonetheless knows how to deep-copy
/// itself.  Usage: `pb::make_unique_clone<MyClone>(std::move(value))`.
template <class Clone, class T>
[[nodiscard]] auto make_unique_clone(T value) -> unique_clone<std::remove_cvref_t<T>, Clone> {
  return unique_clone<std::remove_cvref_t<T>, Clone>{std::move(value)};
}

namespace detail {

template <class T>
struct is_unique_clone : std::false_type {};

template <class T, class Clone>
struct is_unique_clone<unique_clone<T, Clone>> : std::true_type {};

} // namespace detail

/// True when `T` is some instantiation of `pb::unique_clone`.
template <class T>
inline constexpr bool is_unique_clone_v = detail::is_unique_clone<std::remove_cvref_t<T>>::value;

namespace detail {

template <class BranchNode, bool IsFanIn>
struct fan_in_clone_strategy {
  static constexpr bool uses_copy   = false;
  static constexpr bool uses_borrow = false;
};

template <class BranchNode>
struct fan_in_clone_strategy<BranchNode, true> {
  using input_type = typename BranchNode::input_type;
  static constexpr bool uses_copy   = std::copy_constructible<input_type>;
  static constexpr bool uses_borrow = !uses_copy;
};

} // namespace detail

/// True when `BranchNode` lowers its fan-in cases by copying the input once
/// per passing case.  False otherwise (including for non-fan-in nodes).
template <class BranchNode>
inline constexpr bool fan_in_uses_copy_v = detail::fan_in_clone_strategy<
    BranchNode,
    pb::core::detail::is_fan_in_branch_node<BranchNode>::value>::uses_copy;

/// True when `BranchNode` lowers its fan-in cases by passing the input as a
/// `const input_type&` to every branch stage instead of cloning.
template <class BranchNode>
inline constexpr bool fan_in_uses_borrow_v = detail::fan_in_clone_strategy<
    BranchNode,
    pb::core::detail::is_fan_in_branch_node<BranchNode>::value>::uses_borrow;

// ---------------------------------------------------------------------------
// pb::projected<From, Projection, Stage> — user-supplied projection adapter
// ---------------------------------------------------------------------------
///
/// Wrap an existing stage so its declared `input_type` becomes `From`, and
/// the wrapped stage receives `Projection{}(source)` instead of the raw
/// input.  Lets users plug arbitrary "borrow / project / extract a view"
/// strategies into fan-in branches and linear chains without rewriting the
/// underlying stage.
///
/// Required:
///   * `Projection` is a default-constructible callable whose `operator()`
///     accepts `const From&` (or `From` by value) and returns a value
///     convertible to the wrapped stage's `input_type`.
///   * `Stage` satisfies `pb::Stage` (declares input_type / output_type)
///     and is default-constructible.
///
/// Compile-fail diagnostics fire when:
///   * `Projection` is not invocable with `const From&`;
///   * the projection's return type isn't convertible to
///     `stage_traits<Stage>::input_type`.
///
/// Example:
///
/// @code
///   struct Order { std::unique_ptr<Header> header; };
///
///   struct HeaderTag {                          // existing stage
///     using input_type = Header;
///     using output_type = Tag;
///     Tag operator()(Header) const;
///   };
///
///   struct ProjectHeader {                      // user-supplied projection
///     Header operator()(const Order& order) const { return *order.header; }
///   };
///
///   using TagOrder = pb::projected<Order, ProjectHeader, HeaderTag>;
///   // TagOrder::input_type  == Order
///   // TagOrder::output_type == Tag
/// @endcode

namespace detail {

template <class Projection, class From>
concept ProjectionInvocableConst = std::is_invocable_v<const Projection&, const From&>;

} // namespace detail

template <class From, class Projection, class Stage>
struct projected {
  static_assert(std::is_default_constructible_v<Projection>,
                "pb::projected requires a default-constructible Projection — "
                "pass an empty function-object type, not a lambda capture");
  static_assert(std::is_default_constructible_v<Stage>,
                "pb::projected requires a default-constructible Stage — "
                "use the stateful_sequential storage policy if Stage carries state");
  static_assert(detail::ProjectionInvocableConst<Projection, From>,
                "pb::projected: Projection must be callable with const From&");

  using projection_type = Projection;
  using stage_type      = Stage;
  using input_type      = From;
  using output_type     = core::stage_output_t<Stage>;

private:
  using projection_result_type =
      std::invoke_result_t<const Projection&, const From&>;

  static_assert(std::is_convertible_v<projection_result_type, core::stage_input_t<Stage>>,
                "pb::projected: Projection return type must be convertible to the "
                "wrapped Stage's input_type");

public:
  [[nodiscard]] static constexpr auto stage_key()  noexcept { return core::stage_key<Stage>(); }
  [[nodiscard]] static constexpr auto stage_name() noexcept { return core::stage_name<Stage>(); }

  auto operator()(const From& source) const -> output_type {
    auto projected_value = Projection{}(source);
    return Stage{}(std::move(projected_value));
  }
};

} // namespace pb
