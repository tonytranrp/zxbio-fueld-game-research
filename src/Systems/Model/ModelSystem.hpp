#pragma once

#include "AnimationController/animation/ModelKeyframe.hpp"
#include "Core/Types.hpp"
#include "Data/event/model/ModelEvents.hpp"
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <raylib.h>

namespace biofuel::systems::model {

enum class ModelAssetId : u32 {
    MenuTransitionHands = 0,
};

struct ModelAnimationStateSpec {
    std::string_view name;
    i32 clipIndex = -1;
    bool loop = true;
    std::string_view returnState;
    f32 durationSeconds = 0.0f;
};

struct ModelAssetSpec {
    using KeyframeClipFactory = std::vector<animation::model::KeyframeClip>(*)();

    ModelAssetId id{};
    std::string_view debugName;
    std::string_view assetPath;
    std::string_view shaderName;
    std::string_view shaderVertexPath;
    std::string_view shaderFragmentPath;
    bool preloadOnStartup = false;
    bool loadAnimations = false;
    KeyframeClipFactory keyframeClipFactory = nullptr;
    std::string_view defaultIdleState;
    const ModelAnimationStateSpec* animationStates = nullptr;
    i32 animationStateCount = 0;
};

struct ModelAssetMetrics {
    BoundingBox localBounds{};
    Vector3 size{};
    Vector3 center{};
    f32 maxAxis = 0.0f;
    f32 unitScale = 1.0f;
};

struct ModelRenderState {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 rotationAxis{0.0f, 1.0f, 0.0f};
    Vector3 scale{1.0f, 1.0f, 1.0f};
    f32 rotationDegrees = 0.0f;
    Color tint{255, 255, 255, 255};
    bool visible = true;
};

struct SharedAssetData {
    ModelAssetSpec spec{};
    Model prototype{};
    bool prototypeLoaded = false;
    ModelAnimation* animations = nullptr;
    i32 animationCount = 0;
    ModelAssetMetrics metrics{};
    Shader shader{};
    animation::model::ModelRigBinding rigBinding{};
    std::vector<animation::model::KeyframeClip> keyframeClips;
};

class ModelAnimator final {
public:
    void configure(
        std::span<const ModelAnimationStateSpec> states,
        std::string_view defaultIdleState,
        i32 clipCount) noexcept;

    void reset() noexcept;
    void setState(std::string_view stateName, f32 transitionSeconds = 0.0f) noexcept;
    void playAction(std::string_view stateName, f32 transitionSeconds = 0.0f) noexcept;
    void update(Model& model, const ModelAnimation* clips, i32 clipCount, f32 dt) noexcept;

    [[nodiscard]] bool hasState(std::string_view stateName) const noexcept;
    [[nodiscard]] bool empty() const noexcept { return m_states.empty(); }
    [[nodiscard]] std::string_view currentState() const noexcept { return m_currentState; }
    [[nodiscard]] f32 transitionProgress() const noexcept;
    [[nodiscard]] f32 stateProgress() const noexcept;

private:
    struct StateConfig {
        std::string name;
        i32 clipIndex = -1;
        bool loop = true;
        std::string returnState;
        f32 durationSeconds = 0.0f;
    };

    [[nodiscard]] const StateConfig* findState(std::string_view stateName) const noexcept;
    [[nodiscard]] f32 resolveDurationSeconds(const StateConfig& state, i32 clipCount, const ModelAnimation* clips) const noexcept;
    void beginState(const StateConfig& state, f32 transitionSeconds) noexcept;
    void scheduleReturn(const StateConfig& state) noexcept;

    std::vector<StateConfig> m_states;
    std::string m_defaultState;
    std::string m_currentState;
    std::string m_pendingReturnState;
    f32 m_stateElapsed = 0.0f;
    f32 m_stateProgress = 0.0f;
    f32 m_transitionDuration = 0.0f;
    f32 m_transitionElapsed = 0.0f;
    i32 m_lastFrame = 0;
};

class ModelInstance final {
public:
    ~ModelInstance() noexcept;

    [[nodiscard]] u64 instanceId() const noexcept { return m_instanceId; }
    [[nodiscard]] ModelAssetId assetId() const noexcept { return m_assetId; }
    [[nodiscard]] bool ready() const noexcept { return m_model != nullptr; }
    [[nodiscard]] const ModelAssetMetrics& metrics() const noexcept { return m_metrics; }
    [[nodiscard]] Shader shader() const noexcept;
    [[nodiscard]] ModelAnimator& animator() noexcept { return m_animator; }
    [[nodiscard]] const ModelAnimator& animator() const noexcept { return m_animator; }
    [[nodiscard]] const animation::model::ModelKeyframeState& keyframeState() const noexcept { return m_keyframePlayer.state(); }
    [[nodiscard]] f32 keyframeScalar(std::string_view channelName, f32 fallback = 0.0f) const noexcept;
    [[nodiscard]] std::string_view debugName() const noexcept { return m_debugName; }

    void setVisible(bool visible) noexcept { m_visible = visible; }
    [[nodiscard]] bool visible() const noexcept { return m_visible; }

    void setAnimationState(std::string_view stateName, f32 transitionSeconds = 0.0f) noexcept;
    void playAction(std::string_view stateName, f32 transitionSeconds = 0.0f) noexcept;
    void resetAnimation() noexcept;
    void update(f32 dt) noexcept;
    void draw(const ModelRenderState& state) const noexcept;
    void setBoneTranslationOffset(std::string_view boneName, Vector3 offset);

private:
    friend class ModelSystem;
    ModelInstance(
        u64 instanceId,
        ModelAssetId assetId,
        std::string_view debugName,
        std::shared_ptr<const SharedAssetData> sharedAsset);

    static void applyShaderToMaterials(Model& model, Shader shader) noexcept;
    static void applyNormalization(Model& model, const ModelAssetMetrics& metrics) noexcept;

    u64 m_instanceId = 0;
    ModelAssetId m_assetId{};
    std::string m_debugName;
    std::shared_ptr<const SharedAssetData> m_sharedAsset;
    Model* m_model = nullptr;
    Model m_ownedModel{};
    bool m_ownsModel = false;
    bool m_visible = true;
    ModelAssetMetrics m_metrics{};
    ModelAnimator m_animator;
    animation::model::ModelKeyframePlayer m_keyframePlayer;
    std::vector<Transform> m_poseBuffer;
    std::unordered_map<std::string, Vector3> m_boneTranslationOffsets;
};

class ModelSystem final {
public:
    [[nodiscard]] static ModelSystem& instance() noexcept;

    void init();
    void shutdown();
    void update(f32 dt);

    [[nodiscard]] std::shared_ptr<ModelInstance> createInstance(ModelAssetId assetId);
    [[nodiscard]] bool preload(ModelAssetId assetId);
    [[nodiscard]] bool has(ModelAssetId assetId) const noexcept;
    [[nodiscard]] bool isReady(ModelAssetId assetId) const noexcept;
    [[nodiscard]] std::string_view debugName(ModelAssetId assetId) const noexcept;
    [[nodiscard]] std::span<const ModelAssetSpec> registry() const noexcept;

    ModelSystem(const ModelSystem&) = delete;
    ModelSystem& operator=(const ModelSystem&) = delete;
    ModelSystem(ModelSystem&&) = delete;
    ModelSystem& operator=(ModelSystem&&) = delete;

private:
    ModelSystem() = default;
    ~ModelSystem() = default;

    [[nodiscard]] static std::span<const ModelAssetSpec> builtInRegistry() noexcept;
    [[nodiscard]] const ModelAssetSpec* findSpec(ModelAssetId assetId) const noexcept;
    [[nodiscard]] std::shared_ptr<SharedAssetData> loadAsset(const ModelAssetSpec& spec);
    [[nodiscard]] static ModelAssetMetrics computeMetrics(Model& model) noexcept;
    [[nodiscard]] static animation::model::ModelRigBinding buildRigBinding(const Model& model) noexcept;
    static void unloadAsset(SharedAssetData& asset) noexcept;
    void pruneInstances();
    [[nodiscard]] std::shared_ptr<ModelInstance> findInstance(u64 instanceId) const;
    void onSetState(const event::model::ModelSetStateEvent& event);
    void onPlayAction(const event::model::ModelPlayActionEvent& event);

    std::unordered_map<ModelAssetId, std::shared_ptr<SharedAssetData>> m_assets;
    std::unordered_map<u64, std::weak_ptr<ModelInstance>> m_instances;
    std::vector<ModelAssetSpec> m_registry;
    u64 m_nextInstanceId = 1;
    bool m_initialized = false;
    bool m_eventSinksConnected = false;
};

} // namespace biofuel::systems::model
