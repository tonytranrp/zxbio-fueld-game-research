#include "ModelSystem.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/events/model/ModelEvents.hpp"
#include "engine/runtime/typed/Events.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace biofuel::engine::models {

namespace {

constexpr std::array<ModelAssetSpec, 0> BUILT_IN_MODELS{};

} // namespace

ModelInstance::ModelInstance(
    ConstructionToken,
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
    const bool needsIndependentModel =
        m_sharedAsset->animationCount > 0 ||
        m_sharedAsset->prototype.boneCount > 0 ||
        !m_sharedAsset->keyframeClips.empty();
    if (needsIndependentModel) {
        const std::string assetPath{m_sharedAsset->spec.assetPath};
        m_ownedModel = LoadModel(assetPath.c_str());
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
        m_staticModelView = m_sharedAsset->prototype;
        m_model = &m_staticModelView;
        m_ownsModel = false;
    }

    m_animator.configure(
        std::span<const ModelAnimationStateSpec>(
            m_sharedAsset->spec.animationStates,
            static_cast<size_t>(m_sharedAsset->spec.animationStateCount)),
        m_sharedAsset->spec.defaultIdleState,
        m_sharedAsset->animationCount
    );

    if (!m_sharedAsset->keyframeClips.empty() && m_model != nullptr && m_model->boneCount > 0) {
        m_keyframePlayer.configure(m_sharedAsset->rigBinding, m_sharedAsset->keyframeClips);
        m_poseBuffer.resize(static_cast<size_t>(m_model->boneCount));
    }
}

ModelInstance::~ModelInstance() noexcept {
    if (m_ownsModel && m_model != nullptr) {
        UnloadModel(m_ownedModel);
        m_ownedModel = {};
        m_model = nullptr;
    }
    if (m_telemetryCounted) {
        ::biofuel::engine::debug::MemoryTelemetry::remove(
            ::biofuel::engine::debug::ResourceKind::ModelInstance,
            1,
            0);
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
    m_keyframePlayer.reset();
}

void ModelInstance::update(const f32 dt) noexcept {
    if (!ready()) {
        return;
    }

    m_animator.update(*m_model, m_sharedAsset->animations, m_sharedAsset->animationCount, dt);

    if (!m_keyframePlayer.empty() && !m_poseBuffer.empty()) {
        m_keyframePlayer.syncState(
            m_animator.currentState(),
            m_animator.stateProgress(),
            m_animator.transitionProgress());

        const std::span<const Transform> bindPose =
            !m_sharedAsset->bindPoseCopy.empty()
                ? std::span<const Transform>(m_sharedAsset->bindPoseCopy.data(), m_sharedAsset->bindPoseCopy.size())
                : std::span<const Transform>(
                    m_sharedAsset->prototype.bindPose,
                    static_cast<size_t>(m_sharedAsset->prototype.boneCount));

        m_keyframePlayer.apply(
            bindPose,
            std::span<Transform>(m_poseBuffer.data(), m_poseBuffer.size()));

        for (const auto& [boneName, offset] : m_boneTranslationOffsets) {
            const i32 boneIndex = m_sharedAsset->rigBinding.findBoneIndex(boneName);
            if (boneIndex < 0 || boneIndex >= static_cast<i32>(m_poseBuffer.size())) {
                continue;
            }

            auto& translation = m_poseBuffer[static_cast<size_t>(boneIndex)].translation;
            translation.x += offset.x;
            translation.y += offset.y;
            translation.z += offset.z;
        }

        Transform* framePoses[] = { m_poseBuffer.data() };
        ModelAnimation scratch{
            .boneCount = m_model->boneCount,
            .frameCount = 1,
            .bones = m_model->bones,
            .framePoses = framePoses,
            .name = {'K', 'E', 'Y', 'F', 'R', 'A', 'M', 'E', '\0'},
        };
        UpdateModelAnimation(*m_model, scratch, 0);
    }
}

void ModelInstance::draw(const ModelRenderState& state) const noexcept {
    if (!ready() || !m_visible || !state.visible) {
        return;
    }

    DrawModelEx(*m_model, state.position, state.rotationAxis, state.rotationDegrees, state.scale, state.tint);
}

void ModelInstance::setBoneTranslationOffset(const std::string_view boneName, const Vector3 offset) {
    if (boneName.empty()) {
        return;
    }

    if (m_boneTranslationOffsets.empty()) {
        m_boneTranslationOffsets.reserve(12);
    }

    m_boneTranslationOffsets[std::string{boneName}] = offset;
}

f32 ModelInstance::keyframeScalar(const std::string_view channelName, const f32 fallback) const noexcept {
    return m_keyframePlayer.scalar(channelName, fallback);
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

ModelSystem& ModelSystem::instance() noexcept {
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
        ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::model::SetState>().connect<&ModelSystem::onSetState>(*this);
        ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::model::PlayAction>().connect<&ModelSystem::onPlayAction>(*this);
        m_eventSinksConnected = true;
    }

    m_initialized = true;
}

void ModelSystem::shutdown() {
    if (!m_initialized && !m_eventSinksConnected) {
        return;
    }

    if (m_eventSinksConnected) {
        ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::model::SetState>().disconnect<&ModelSystem::onSetState>(*this);
        ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::model::PlayAction>().disconnect<&ModelSystem::onPlayAction>(*this);
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
    if (const ModelAssetSpec* spec = findSpec(assetId);
        spec != nullptr && spec->singleResidentInstance) {
        if (auto instance = findLiveInstance(assetId)) {
            return instance;
        }
    }

    if (!preload(assetId)) {
        return nullptr;
    }

    const auto assetIt = m_assets.find(assetId);
    if (assetIt == m_assets.end() || !assetIt->second) {
        return nullptr;
    }

    const u64 instanceId = m_nextInstanceId++;
    auto instance = std::make_shared<ModelInstance>(
        ModelInstance::ConstructionToken{},
        instanceId,
        assetId,
        assetIt->second->spec.debugName,
        assetIt->second
    );

    if (!instance->ready()) {
        return nullptr;
    }

    m_instances[instanceId] = instance;
    ::biofuel::engine::debug::MemoryTelemetry::add(
        ::biofuel::engine::debug::ResourceKind::ModelInstance,
        1,
        0);
    instance->m_telemetryCounted = true;

    if (assetIt->second->spec.releasePrototypeAfterInstance && instance->ownsIndependentModel()) {
        unloadPrototype(*assetIt->second);
    }
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

    const std::string assetPath{spec.assetPath};
    if (!FileExists(assetPath.c_str())) {
        spdlog::warn("ModelSystem: model '{}' not found at '{}'", spec.debugName, spec.assetPath);
        return asset;
    }

    asset->prototype = LoadModel(assetPath.c_str());
    if (asset->prototype.meshCount <= 0) {
        spdlog::warn("ModelSystem: failed to load model '{}'", spec.debugName);
        asset->prototype = {};
        return asset;
    }

    asset->metrics = computeMetrics(asset->prototype);
    ModelInstance::applyNormalization(asset->prototype, asset->metrics);
    asset->rigBinding = buildRigBinding(asset->prototype);
    if (asset->prototype.bindPose != nullptr && asset->prototype.boneCount > 0) {
        asset->bindPoseCopy.assign(
            asset->prototype.bindPose,
            asset->prototype.bindPose + asset->prototype.boneCount);
    }
    {
        std::error_code error;
        const auto bytes = std::filesystem::file_size(std::filesystem::path{assetPath}, error);
        asset->estimatedBytes = error ? 0 : static_cast<i64>(bytes);
    }

    if (!spec.shaderName.empty()) {
        auto& shaderManager = ::biofuel::engine::runtime::Runtime::shader();
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
        asset->animations = LoadModelAnimations(assetPath.c_str(), &rawCount);
        asset->animationCount = rawCount;
        if (asset->animations == nullptr || asset->animationCount <= 0) {
            asset->animations = nullptr;
            asset->animationCount = 0;
        }
    }

    if (spec.keyframeClipFactory != nullptr) {
        asset->keyframeClips = spec.keyframeClipFactory();
    }

    asset->prototypeLoaded = true;
    ::biofuel::engine::debug::MemoryTelemetry::add(
        ::biofuel::engine::debug::ResourceKind::ModelAsset,
        1,
        asset->estimatedBytes);
    spdlog::info("ModelSystem: preloaded model '{}'", spec.debugName);
    return asset;
}

::biofuel::engine::animation::model::ModelRigBinding ModelSystem::buildRigBinding(const Model& model) noexcept {
    ::biofuel::engine::animation::model::ModelRigBinding binding;
    if (model.boneCount <= 0 || model.bones == nullptr) {
        return binding;
    }

    binding.boneNames.reserve(static_cast<size_t>(model.boneCount));
    for (i32 boneIndex = 0; boneIndex < model.boneCount; ++boneIndex) {
        const std::string boneName = model.bones[boneIndex].name;
        binding.boneIndices.emplace(boneName, boneIndex);
        binding.boneNames.push_back(boneName);
    }

    return binding;
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

    unloadPrototype(asset);
    asset.bindPoseCopy.clear();
    asset.keyframeClips.clear();
}

void ModelSystem::unloadPrototype(SharedAssetData& asset) noexcept {
    if (!asset.prototypeLoaded) {
        return;
    }

    UnloadModel(asset.prototype);
    asset.prototype = {};
    asset.prototypeLoaded = false;
    ::biofuel::engine::debug::MemoryTelemetry::remove(
        ::biofuel::engine::debug::ResourceKind::ModelAsset,
        1,
        asset.estimatedBytes);
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

std::shared_ptr<ModelInstance> ModelSystem::findLiveInstance(const ModelAssetId assetId) const {
    for (const auto& [_, weakInstance] : m_instances) {
        auto instance = weakInstance.lock();
        if (instance && instance->assetId() == assetId && instance->ready()) {
            return instance;
        }
    }
    return nullptr;
}

void ModelSystem::onSetState(const ::biofuel::engine::events::model::ModelSetStateEvent& event) {
    if (auto instance = findInstance(event.instanceId)) {
        instance->setAnimationState(event.stateName, event.transitionSeconds);
    }
}

void ModelSystem::onPlayAction(const ::biofuel::engine::events::model::ModelPlayActionEvent& event) {
    if (auto instance = findInstance(event.instanceId)) {
        instance->playAction(event.actionState, event.transitionSeconds);
    }
}

} // namespace biofuel::engine::models
