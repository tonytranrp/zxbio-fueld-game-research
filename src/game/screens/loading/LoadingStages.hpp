#pragma once

#include "engine/core/LoadingTask.hpp"
#include <pb/pipeline.hpp>
#include <pb/runtime/sequential.hpp>
#include <string>

namespace biofuel::game::screens::loading {

struct CompileShaderStage {
    using input_type = void;
    using output_type = void;
    using error_type = pb::no_error;

    void operator()() const {}
};

struct LoadModelStage {
    using input_type = void;
    using output_type = void;
    using error_type = pb::no_error;

    void operator()() const {}
};

struct InitSystemStage {
    using input_type = void;
    using output_type = void;
    using error_type = pb::no_error;

    void operator()() const {}
};

template<typename Pipeline>
[[nodiscard]] ::biofuel::LoadingTask makeLoadingTask(std::string name, f32 weight) {
    return ::biofuel::LoadingTask{
        .name = std::move(name),
        .weight = weight,
        .work = []() {
            auto engine = pb::runtime::compile<Pipeline>(pb::runtime::sequential{});
            engine.run();
        },
    };
}

} // namespace biofuel::game::screens::loading
