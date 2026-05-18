#pragma once

#include "engine/runtime/Runtime.hpp"
#include "engine/tasks/TaskModule.hpp"
#include "engine/ui/ScreenManager.hpp"

namespace biofuel::engine::tasks {

using ::biofuel::engine::runtime::Runtime;

// ---------------------------------------------------------------------------
// Pipeline stage stubs — compile-time type metadata for each init task.
// ---------------------------------------------------------------------------
#define BIOFUEL_TASK_STAGE(Name)              \
    struct Name {                              \
        using input_type = InitToken;          \
        using output_type = InitResult;        \
        InitResult operator()(InitToken) const noexcept { return {}; } \
    }

BIOFUEL_TASK_STAGE(EventInit);
BIOFUEL_TASK_STAGE(TaskManagerInit);
BIOFUEL_TASK_STAGE(ScreenInit);
BIOFUEL_TASK_STAGE(AnimationInit);
BIOFUEL_TASK_STAGE(PhysicsInit);
BIOFUEL_TASK_STAGE(HandPoseInit);
BIOFUEL_TASK_STAGE(ModelInit);
BIOFUEL_TASK_STAGE(AudioInit);
BIOFUEL_TASK_STAGE(VideoInit);
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
BIOFUEL_TASK_STAGE(HandTrackingInit);
#endif

#undef BIOFUEL_TASK_STAGE

// Pipeline alias: from<InitToken>::then<Stage>::to<InitResult>
template <typename Stage>
using InitPipeline = pb::core::pipeline<
    InitToken,
    InitResult,
    pb::meta::type_list<Stage>
>;

// ---------------------------------------------------------------------------
// TaskModule definitions — each maps a compile-time pipeline to runtime init work
// ---------------------------------------------------------------------------
#define BIOFUEL_TASK_MODULE(ClassName, StageName, Label, Weight, InitCall) \
    struct ClassName {                                                       \
        using pipeline = InitPipeline<StageName>;                            \
        static constexpr std::string_view task_label() { return Label; }     \
        static constexpr f32 task_weight() { return Weight; }                \
        static std::function<void()> init_work() {                           \
            return []() { Runtime::InitCall; };                              \
        }                                                                    \
    }

BIOFUEL_TASK_MODULE(EventTaskModule,           EventInit,        "Initializing event bus...",         0.5f, events().init());
BIOFUEL_TASK_MODULE(TaskManagerTaskModule,     TaskManagerInit,  "Initializing task manager...",      0.4f, tasks().init());
BIOFUEL_TASK_MODULE(ScreenTaskModule,          ScreenInit,       "Initializing screen stack...",      0.5f, screen().init());
BIOFUEL_TASK_MODULE(AnimationTaskModule,       AnimationInit,    "Initializing animation system...",   0.5f, animation().init());
BIOFUEL_TASK_MODULE(PhysicsTaskModule,         PhysicsInit,      "Initializing physics engine...",     0.5f, physics().init());
BIOFUEL_TASK_MODULE(HandPoseTaskModule,        HandPoseInit,     "Initializing hand pose system...",   0.2f, handPose().init());
BIOFUEL_TASK_MODULE(ModelTaskModule,           ModelInit,        "Initializing model system...",       0.4f, model().init());
BIOFUEL_TASK_MODULE(AudioTaskModule,           AudioInit,        "Initializing audio device...",       0.5f, audio().init());
BIOFUEL_TASK_MODULE(VideoTaskModule,           VideoInit,        "Initializing video system...",       0.4f, video().init());
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
BIOFUEL_TASK_MODULE(HandTrackingTaskModule,    HandTrackingInit, "Initializing hand tracking bridge...", 0.2f, handTracking().init());
#endif

#undef BIOFUEL_TASK_MODULE

// ---------------------------------------------------------------------------
// Master compile-time task module list — order matters
// ---------------------------------------------------------------------------
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
using EngineStartupModules = TaskModuleList<
    EventTaskModule,
    TaskManagerTaskModule,
    ScreenTaskModule,
    AnimationTaskModule,
    PhysicsTaskModule,
    HandTrackingTaskModule,
    HandPoseTaskModule,
    ModelTaskModule,
    AudioTaskModule,
    VideoTaskModule
>;
#else
using EngineStartupModules = TaskModuleList<
    EventTaskModule,
    TaskManagerTaskModule,
    ScreenTaskModule,
    AnimationTaskModule,
    PhysicsTaskModule,
    HandPoseTaskModule,
    ModelTaskModule,
    AudioTaskModule,
    VideoTaskModule
>;
#endif

// Compile-time validation: every module is a valid TaskModule + pipeline
static_assert(TaskModule<EventTaskModule>);
static_assert(TaskModule<TaskManagerTaskModule>);
static_assert(TaskModule<ScreenTaskModule>);
static_assert(TaskModule<AnimationTaskModule>);
static_assert(TaskModule<PhysicsTaskModule>);
static_assert(TaskModule<HandPoseTaskModule>);
static_assert(TaskModule<ModelTaskModule>);
static_assert(TaskModule<AudioTaskModule>);
static_assert(TaskModule<VideoTaskModule>);
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
static_assert(TaskModule<HandTrackingTaskModule>);
#endif

} // namespace biofuel::engine::tasks
