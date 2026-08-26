#pragma once

#include <concepts>
#include <type_traits>

#include "pb/core/stage_traits.hpp"

namespace pb::core {

template <class T>
concept Stage = stage_traits<T>::is_valid_stage;

template <class T>
concept stage = Stage<T>;

template <class From, class To>
concept Connectable = Stage<To> && std::same_as<From, stage_input_t<To>>;

template <class From, class To>
inline constexpr bool connectable_v = Connectable<From, To>;

template <class A, class B>
concept AdjacentStages = Stage<A> && Stage<B> && std::same_as<stage_output_t<A>, stage_input_t<B>>;

template <class A, class B>
inline constexpr bool adjacent_stages_v = AdjacentStages<A, B>;

template <class StageType, class Input>
concept RunnableStage = Stage<StageType> && requires(StageType stage_value, Input input) {
  stage_value(input);
};

template <class StageType, class Input>
inline constexpr bool runnable_stage_v = RunnableStage<StageType, Input>;

} // namespace pb::core
