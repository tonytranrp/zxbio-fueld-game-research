#include "AppLifecycle.hpp"
#include "engine/debug/DebugOverlayService.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include "engine/graphics/shaders/BlurCompositeModule.hpp"
#include "engine/graphics/shaders/BlurHModule.hpp"
#include "engine/graphics/shaders/BlurVModule.hpp"
#include "engine/graphics/shaders/CrossfadeModule.hpp"
#include "engine/graphics/shaders/LoadingPreludeModule.hpp"
#include "engine/graphics/shaders/ProceduralBackdropModule.hpp"
#include "engine/graphics/shaders/MenuOptionModule.hpp"
#include "engine/graphics/shaders/TypedShaderModule.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/runtime/typed/AssetCatalog.hpp"
#include "engine/runtime/typed/Assets.hpp"
#include "engine/tasks/TaskModuleRegistry.hpp"
#include "engine/ui/ScreenManager.hpp"
#include <raylib.h>
#include <filesystem>
#include <stdexcept>
#include <stop_token>
#include <string>

namespace biofuel::engine::app {

namespace {

template<typename TShader>
void ensureShaderLoaded(::biofuel::engine::graphics::ShaderManager& shaderManager) {
    if (::biofuel::engine::runtime::typed::Shaders::loaded<TShader>(shaderManager)) {
        return;
    }
    ::biofuel::engine::runtime::typed::Shaders::load<TShader>(shaderManager);
}

} // namespace

void AppLifecycle::openWindow(const WindowLifecycleConfig config) {
    if (config.resizable) {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    }
    if (config.fullscreen) {
        SetConfigFlags(FLAG_FULLSCREEN_MODE);
    }

    const std::string title{config.title};
    InitWindow(config.width, config.height, title.c_str());
}

void AppLifecycle::prepareLoadingPrelude() {
    auto& shaderManager = ::biofuel::engine::runtime::Runtime::shader();
    shaderManager.init();
    ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::LoadingPrelude>(shaderManager);
    ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::MenuOption>(shaderManager);
}

void AppLifecycle::addStartupTasks(LoadingTaskQueue& tasks, const StartupLifecycleConfig config) {
    tasks.add({"Configuring input...", 0.3f, []() {
        SetExitKey(KEY_NULL);
    }});
    tasks.add({"Setting window constraints...", 0.3f, [config]() {
        SetWindowMinSize(config.width, config.height);
    }});
    tasks.add({"Setting target framerate...", 0.3f, [config]() {
        SetTargetFPS(config.targetFps);
    }});

    // Auto-registered engine service init tasks (compile-time validated)
    ::biofuel::engine::tasks::EngineStartupModules::populate(tasks);

    tasks.add(LoadingTask::async("Preflighting startup assets...", 0.8f, [](std::stop_token token) {
        if (token.stop_requested()) {
            return;
        }
        if (!std::filesystem::is_directory("assets")) {
            throw std::runtime_error{"assets directory is missing"};
        }
        if (token.stop_requested()) {
            return;
        }
        if (!std::filesystem::is_directory("assets/shaders")) {
            throw std::runtime_error{"assets/shaders directory is missing"};
        }
    }));

    auto& shaderManager = ::biofuel::engine::runtime::Runtime::shader();
    tasks.add({"Compiling blur horizontal shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::BlurH>(shaderManager);
    }});
    tasks.add({"Compiling blur vertical shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::BlurV>(shaderManager);
    }});
    tasks.add({"Compiling blur composite shader...", 1.2f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::BlurComposite>(shaderManager);
    }});
    tasks.add({"Compiling crossfade shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::Crossfade>(shaderManager);
    }});
    tasks.add({"Compiling loading prelude shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::LoadingPrelude>(shaderManager);
    }});
    tasks.add({"Compiling menu option shader...", 1.3f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::MenuOption>(shaderManager);
    }});
    tasks.add({"Compiling background shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::ProceduralBackdrop>(shaderManager);
    }});

    auto& modelService = ::biofuel::engine::runtime::Runtime::model();
    for (const auto& modelSpec : modelService.registry()) {
        if (!modelSpec.preloadOnStartup) {
            continue;
        }

        std::string taskName = "Loading model asset: ";
        taskName += modelSpec.debugName;
        if (!modelSpec.shaderName.empty()) {
            taskName += " (model + shader)";
        }

        tasks.add({std::move(taskName), 1.6f, [assetId = modelSpec.id]() {
            (void)::biofuel::engine::runtime::Runtime::model().preload(assetId);
        }});
    }

    tasks.add({"Caching transition shader...", 1.0f, []() {
        ::biofuel::engine::runtime::Runtime::screen().preloadCrossfadeShader();
    }});
}

void AppLifecycle::shutdownCoreServices() noexcept {
    auto& services = ::biofuel::engine::runtime::Runtime::services();
    services.get<::biofuel::engine::runtime::typed::DebugOverlayService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::TaskService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::AnimationService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::ScreenService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::ModelService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::PhysicsService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::VideoService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::AudioService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::ShaderService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::FontService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::EventService>().shutdown();
}

} // namespace biofuel::engine::app
