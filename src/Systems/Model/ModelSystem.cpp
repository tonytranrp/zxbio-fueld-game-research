#include "ModelSystem.hpp"
#include "Data/Data.hpp"
#include "Data/event/model/ModelEvents.hpp"
#include "Utils/render/ShaderManager.hpp"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <raymath.h>

namespace biofuel::systems::model {

namespace {

constexpr ModelAnimationStateSpec MENU_HANDS_STATES[] = {
    {.name = "idle", .clipIndex = -1, .loop = true, .returnState = {}, .durationSeconds = 0.0f},
    {.name = "action", .clipIndex = -1, .loop = false, .returnState = "idle", .durationSeconds = 0.95f},
};

constexpr ModelAssetSpec BUILT_IN_MODELS[] = {
    {
        .id = ModelAssetId::MenuTransitionHands,
        .debugName = "Menu Transition Hands",
        .assetPath = "assets/models/menu_transition_hands/low_poly_hands.glb",
        .shaderName = "menu_hands",
        .shaderVertexPath = "assets/shaders/menu_hands.vs",
        .shaderFragmentPath = "assets/shaders/menu_hands.fs",
        .preloadOnStartup = true,
        .loadAnimations = false,
        .defaultIdleState = "idle",
        .animationStates = MENU_HANDS_STATES,
        .animationStateCount = static_cast<i32>(std::size(MENU_HANDS_STATES)),
    },
};

} // namespace

void ModelAnimator::configure(
    const std::span<const ModelAnimationStateSpec> states,
    const std::string_view defaultIdleState,
    const i32 clipCount) noexcept
{
    m_states.clear();
    m_states.reserve(states.size());
    for (const auto& state : states) {
        if (state.name.empty()) {
            continue;
        }

        if (state.clipIndex >= clipCount && state.clipIndex >= 0) {
            continue;
        }

        m_states.push_back(StateConfig{
            .name = std::string{state.name},
            .clipIndex = state.clipIndex,
            .loop = state.loop,
            .returnState = std::string{state.returnState},
            .durationSeconds = state.durationSeconds,
        });
    }

    m_defaultState = std::string{defaultIdleState};
    reset();
    if (!m_defaultState.empty()) {
        setState(m_defaultState);
    }
}

void ModelAnimator::reset() noexcept {
    m_currentState.clear();
    m_pendingReturnState.clear();
    m_stateElapsed = 0.0f;
    m_stateProgress = 0.0f;
    m_transitionDuration = 0.0f;
    m_transitionElapsed = 0.0f;
    m_lastFrame = 0;
}

void ModelAnimator::setState(const std::string_view stateName, const f32 transitionSeconds) noexcept {
    const StateConfig* state = findState(stateName);
    if (state == nullptr) {
        return;
    }

    beginState(*state, transitionSeconds);
}

void ModelAnimator::playAction(const std::string_view stateName, const f32 transitionSeconds) noexcept {
    const StateConfig* state = findState(stateName);
    if (state == nullptr) {
        return;
    }

    beginState(*state, transitionSeconds);
    scheduleReturn(*state);
}

void ModelAnimator::update(Model& model, const ModelAnimation* clips, const i32 clipCount, const f32 dt) noexcept {
    if (m_currentState.empty()) {
        return;
    }

    const StateConfig* state = findState(m_currentState);
    if (state == nullptr) {
        return;
    }

    m_stateElapsed += dt;
    m_transitionElapsed = std::min(m_transitionElapsed + dt, m_transitionDuration);
    const f32 duration = std::max(resolveDurationSeconds(*state, clipCount, clips), 0.0f);
    m_stateProgress = (duration > 0.0f)
        ? std::clamp(m_stateElapsed / duration, 0.0f, 1.0f)
        : 0.0f;

    if (state->clipIndex >= 0 && state->clipIndex < clipCount && clips != nullptr) {
        const ModelAnimation& clip = clips[state->clipIndex];
        if (clip.frameCount > 0) {
            const f32 clipDuration = std::max(duration, 0.001f);
            f32 clipTime = m_stateElapsed;
            if (state->loop) {
                clipTime = std::fmod(clipTime, clipDuration);
            } else {
                clipTime = std::min(clipTime, clipDuration);
            }

            const f32 normalized = std::clamp(clipTime / clipDuration, 0.0f, 1.0f);
            const i32 frame = std::clamp(
                static_cast<i32>(normalized * static_cast<f32>(std::max(clip.frameCount - 1, 0))),
                0,
                std::max(clip.frameCount - 1, 0)
            );
            if (frame != m_lastFrame || m_stateElapsed <= dt) {
                UpdateModelAnimation(model, clip, frame);
                m_lastFrame = frame;
            }
        }
    }

    if (!state->loop) {
        if (duration > 0.0f && m_stateElapsed >= duration) {
            if (!m_pendingReturnState.empty()) {
                setState(m_pendingReturnState, 0.14f);
                m_pendingReturnState.clear();
            } else if (!state->returnState.empty()) {
                setState(state->returnState, 0.14f);
            } else if (!m_defaultState.empty() && m_currentState != m_defaultState) {
                setState(m_defaultState, 0.14f);
            }
        }
    }
}

bool ModelAnimator::hasState(const std::string_view stateName) const noexcept {
    return findState(stateName) != nullptr;
}

f32 ModelAnimator::transitionProgress() const noexcept {
    if (m_transitionDuration <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(m_transitionElapsed / m_transitionDuration, 0.0f, 1.0f);
}

f32 ModelAnimator::stateProgress() const noexcept {
    return std::clamp(m_stateProgress, 0.0f, 1.0f);
}

const ModelAnimator::StateConfig* ModelAnimator::findState(const std::string_view stateName) const noexcept {
    const auto it = std::find_if(m_states.begin(), m_states.end(),
        [stateName](const StateConfig& state) { return state.name == stateName; });
    if (it == m_states.end()) {
        return nullptr;
    }
    return &(*it);
}

f32 ModelAnimator::resolveDurationSeconds(
    const StateConfig& state,
    const i32 clipCount,
    const ModelAnimation* clips) const noexcept
{
    if (state.durationSeconds > 0.0f) {
        return state.durationSeconds;
    }

    if (state.clipIndex >= 0 && state.clipIndex < clipCount && clips != nullptr) {
        const i32 frameCount = clips[state.clipIndex].frameCount;
        if (frameCount > 0) {
            return static_cast<f32>(frameCount) / 24.0f;
        }
    }

    return 0.0f;
}

void ModelAnimator::beginState(const StateConfig& state, const f32 transitionSeconds) noexcept {
    m_currentState = state.name;
    m_stateElapsed = 0.0f;
    m_stateProgress = 0.0f;
    m_transitionDuration = std::max(transitionSeconds, 0.0f);
    m_transitionElapsed = 0.0f;
    m_lastFrame = 0;
}

void ModelAnimator::scheduleReturn(const StateConfig& state) noexcept {
    if (!state.returnState.empty()) {
        m_pendingReturnState = state.returnState;
        return;
    }

    if (!m_defaultState.empty() && state.name != m_defaultState) {
        m_pendingReturnState = m_defaultState;
        return;
    }

    m_pendingReturnState.clear();
}

ModelInstance::ModelInstance(
    const u64 instanceId,
    const ModelAssetId assetId,
    const std::string_view debugName,
    std::shared_ptr<const SharedAssetData> sharedAsset)
    : m_instanceId(instanceId)
    , m_assetId(assetId)
    , m_debugName(debugName)
    , m_sharedAsset(std::move(sharedAsset))
{
    if (!m_sharedAsset || !m_sharedAsset->prototypeLoaded) {
        return;
    }

    m_metrics = m_sharedAsset->metrics;
    const bool needsIndependentModel = m_sharedAsset->animationCount > 0;
    if (needsIndependentModel) {
        m_ownedModel = LoadModel(m_sharedAsset->spec.assetPath.data());
        if (m_ownedModel.meshCount <= 0) {
            spdlog::warn("ModelInstance: failed to create independent model for '{}'", m_debugName);
            m_ownedModel = {};
            m_model = nullptr;
            return;
        }

        applyNormalization(m_ownedModel, m_metrics);
        applyShaderToMaterials(m_ownedModel, m_sharedAsset->shader);
        m_model = &m_ownedModel;
        m_ownsModel = true;
    } else {
        m_model = const_cast<Model*>(&m_sharedAsset->prototype);
        m_ownsModel = false;
    }

    m_animator.configure(
        std::span<const ModelAnimationStateSpec>(
            m_sharedAsset->spec.animationStates,
            static_cast<size_t>(m_sharedAsset->spec.animationStateCount)),
        m_sharedAsset->spec.defaultIdleState,
        m_sharedAsset->animationCount
    );
}

ModelInstance::~ModelInstance() noexcept {
    if (m_ownsModel && m_model != nullptr) {
        UnloadModel(m_ownedModel);
        m_ownedModel = {};
        m_model = nullptr;
    }
}

Shader ModelInstance::shader() const noexcept {
    if (m_model == nullptr || m_model->materialCount <= 0) {
        return Shader{};
    }
    return m_model->materials[0].shader;
}

void ModelInstance::setAnimationState(const std::string_view stateName, const f32 transitionSeconds) noexcept {
    m_animator.setState(stateName, transitionSeconds);
}

void ModelInstance::playAction(const std::string_view stateName, const f32 transitionSeconds) noexcept {
    m_animator.playAction(stateName, transitionSeconds);
}

void ModelInstance::resetAnimation() noexcept {
    m_animator.reset();
    m_animator.configure(
        std::span<const ModelAnimationStateSpec>(
            m_sharedAsset->spec.animationStates,
            static_cast<size_t>(m_sharedAsset->spec.animationStateCount)),
        m_sharedAsset->spec.defaultIdleState,
        m_sharedAsset->animationCount
    );
}

void ModelInstance::update(const f32 dt) noexcept {
    if (!ready()) {
        return;
    }

    m_animator.update(*m_model, m_sharedAsset->animations, m_sharedAsset->animationCount, dt);
}

void ModelInstance::draw(const ModelRenderState& state) const noexcept {
    if (!ready() || !m_visible || !state.visible) {
        return;
    }

    DrawModelEx(*m_model, state.position, state.rotationAxis, state.rotationDegrees, state.scale, state.tint);
}

void ModelInstance::applyShaderToMaterials(Model& model, const Shader shader) noexcept {
    if (!IsShaderValid(shader)) {
        return;
    }

    for (i32 materialIndex = 0; materialIndex < model.materialCount; ++materialIndex) {
        model.materials[materialIndex].shader = shader;
    }
}

void ModelInstance::applyNormalization(Model& model, const ModelAssetMetrics& metrics) noexcept {
    model.transform = MatrixTranslate(-metrics.center.x, -metrics.center.y, -metrics.center.z);
}

ModelSystem& ModelSystem::instance() {
    static ModelSystem system;
    return system;
}

void ModelSystem::init() {
    if (m_initialized) {
        return;
    }

    m_registry.assign(builtInRegistry().begin(), builtInRegistry().end());
    m_nextInstanceId = 1;

    if (!m_eventSinksConnected) {
        auto& bus = Data::eventBus();
        bus.sink<event::model::ModelSetStateEvent>().connect<&ModelSystem::onSetState>(*this);
        bus.sink<event::model::ModelPlayActionEvent>().connect<&ModelSystem::onPlayAction>(*this);
        m_eventSinksConnected = true;
    }

    m_initialized = true;
}

void ModelSystem::shutdown() {
    if (!m_initialized && !m_eventSinksConnected) {
        return;
    }

    if (m_eventSinksConnected) {
        auto& bus = Data::eventBus();
        bus.sink<event::model::ModelSetStateEvent>().disconnect<&ModelSystem::onSetState>(*this);
        bus.sink<event::model::ModelPlayActionEvent>().disconnect<&ModelSystem::onPlayAction>(*this);
        m_eventSinksConnected = false;
    }

    m_instances.clear();
    for (auto& [assetId, asset] : m_assets) {
        if (asset) {
            unloadAsset(*asset);
        }
    }
    m_assets.clear();
    m_registry.clear();
    m_initialized = false;
}

void ModelSystem::update(const f32 dt) {
    pruneInstances();
    for (auto it = m_instances.begin(); it != m_instances.end(); ++it) {
        if (auto instance = it->second.lock()) {
            instance->update(dt);
        }
    }
}

std::shared_ptr<ModelInstance> ModelSystem::createInstance(const ModelAssetId assetId) {
    if (!preload(assetId)) {
        return nullptr;
    }

    const auto assetIt = m_assets.find(assetId);
    if (assetIt == m_assets.end() || !assetIt->second) {
        return nullptr;
    }

    const u64 instanceId = m_nextInstanceId++;
    auto instance = std::shared_ptr<ModelInstance>(new ModelInstance(
        instanceId,
        assetId,
        assetIt->second->spec.debugName,
        assetIt->second
    ));

    if (!instance->ready()) {
        return nullptr;
    }

    m_instances[instanceId] = instance;
    return instance;
}

bool ModelSystem::preload(const ModelAssetId assetId) {
    if (isReady(assetId)) {
        return true;
    }

    const ModelAssetSpec* spec = findSpec(assetId);
    if (spec == nullptr) {
        spdlog::warn("ModelSystem: unknown asset id {}", static_cast<u32>(assetId));
        return false;
    }

    std::shared_ptr<SharedAssetData> asset = loadAsset(*spec);
    if (!asset || !asset->prototypeLoaded) {
        return false;
    }

    m_assets[assetId] = std::move(asset);
    return true;
}

bool ModelSystem::has(const ModelAssetId assetId) const noexcept {
    return findSpec(assetId) != nullptr;
}

bool ModelSystem::isReady(const ModelAssetId assetId) const noexcept {
    const auto it = m_assets.find(assetId);
    return it != m_assets.end() && it->second && it->second->prototypeLoaded;
}

std::string_view ModelSystem::debugName(const ModelAssetId assetId) const noexcept {
    const ModelAssetSpec* spec = findSpec(assetId);
    if (spec == nullptr) {
        return {};
    }
    return spec->debugName;
}

std::span<const ModelAssetSpec> ModelSystem::registry() const noexcept {
    if (m_registry.empty()) {
        return builtInRegistry();
    }
    return m_registry;
}

std::span<const ModelAssetSpec> ModelSystem::builtInRegistry() noexcept {
    return std::span{BUILT_IN_MODELS};
}

const ModelAssetSpec* ModelSystem::findSpec(const ModelAssetId assetId) const noexcept {
    const auto specs = registry();
    const auto it = std::find_if(specs.begin(), specs.end(),
        [assetId](const ModelAssetSpec& spec) { return spec.id == assetId; });
    if (it == specs.end()) {
        return nullptr;
    }
    return &(*it);
}

std::shared_ptr<SharedAssetData> ModelSystem::loadAsset(const ModelAssetSpec& spec) {
    auto asset = std::make_shared<SharedAssetData>();
    asset->spec = spec;

    if (!FileExists(spec.assetPath.data())) {
        spdlog::warn("ModelSystem: model '{}' not found at '{}'", spec.debugName, spec.assetPath);
        return asset;
    }

    asset->prototype = LoadModel(spec.assetPath.data());
    if (asset->prototype.meshCount <= 0) {
        spdlog::warn("ModelSystem: failed to load model '{}'", spec.debugName);
        asset->prototype = {};
        return asset;
    }

    asset->metrics = computeMetrics(asset->prototype);
    ModelInstance::applyNormalization(asset->prototype, asset->metrics);

    if (!spec.shaderName.empty()) {
        auto& shaderManager = utils::render::ShaderManager::instance();
        if (!shaderManager.has(spec.shaderName)) {
            shaderManager.load(spec.shaderName, spec.shaderVertexPath, spec.shaderFragmentPath);
        }

        asset->shader = shaderManager.tryGet(spec.shaderName);
        if (asset->shader.id == 0) {
            spdlog::warn("ModelSystem: shader '{}' failed for '{}'", spec.shaderName, spec.debugName);
        } else {
            ModelInstance::applyShaderToMaterials(asset->prototype, asset->shader);
        }
    }

    if (spec.loadAnimations) {
        i32 rawCount = 0;
        asset->animations = LoadModelAnimations(spec.assetPath.data(), &rawCount);
        asset->animationCount = rawCount;
        if (asset->animations == nullptr || asset->animationCount <= 0) {
            asset->animations = nullptr;
            asset->animationCount = 0;
        }
    }

    asset->prototypeLoaded = true;
    spdlog::info("ModelSystem: preloaded model '{}'", spec.debugName);
    return asset;
}

ModelAssetMetrics ModelSystem::computeMetrics(Model& model) noexcept {
    const BoundingBox bounds = GetModelBoundingBox(model);
    const Vector3 size = {
        bounds.max.x - bounds.min.x,
        bounds.max.y - bounds.min.y,
        bounds.max.z - bounds.min.z
    };
    const Vector3 center = {
        bounds.min.x + size.x * 0.5f,
        bounds.min.y + size.y * 0.5f,
        bounds.min.z + size.z * 0.5f
    };
    const f32 maxAxis = std::max(size.x, std::max(size.y, size.z));
    const BoundingBox localBounds = {
        .min = Vector3{bounds.min.x - center.x, bounds.min.y - center.y, bounds.min.z - center.z},
        .max = Vector3{bounds.max.x - center.x, bounds.max.y - center.y, bounds.max.z - center.z},
    };

    return ModelAssetMetrics{
        .localBounds = localBounds,
        .size = size,
        .center = center,
        .maxAxis = maxAxis,
        .unitScale = (maxAxis > 0.0001f) ? (1.0f / maxAxis) : 1.0f,
    };
}

void ModelSystem::unloadAsset(SharedAssetData& asset) noexcept {
    if (asset.animations != nullptr && asset.animationCount > 0) {
        UnloadModelAnimations(asset.animations, static_cast<unsigned int>(asset.animationCount));
        asset.animations = nullptr;
        asset.animationCount = 0;
    }

    if (asset.prototypeLoaded) {
        UnloadModel(asset.prototype);
        asset.prototype = {};
        asset.prototypeLoaded = false;
    }
}

void ModelSystem::pruneInstances() {
    for (auto it = m_instances.begin(); it != m_instances.end(); ) {
        if (it->second.expired()) {
            it = m_instances.erase(it);
        } else {
            ++it;
        }
    }
}

std::shared_ptr<ModelInstance> ModelSystem::findInstance(const u64 instanceId) const {
    const auto it = m_instances.find(instanceId);
    if (it == m_instances.end()) {
        return nullptr;
    }
    return it->second.lock();
}

void ModelSystem::onSetState(const event::model::ModelSetStateEvent& event) {
    if (auto instance = findInstance(event.instanceId)) {
        instance->setAnimationState(event.stateName, event.transitionSeconds);
    }
}

void ModelSystem::onPlayAction(const event::model::ModelPlayActionEvent& event) {
    if (auto instance = findInstance(event.instanceId)) {
        instance->playAction(event.actionState, event.transitionSeconds);
    }
}

} // namespace biofuel::systems::model
