#pragma once

#include "engine/animation/Animation.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace biofuel::engine::animation {

// ------------------------------------------------------------------------------
// AnimationManager - Singleton that owns and updates all active animations
// ------------------------------------------------------------------------------
class AnimationManager {
public:
    [[nodiscard]] static AnimationManager& instance() noexcept;

    void init();
    void shutdown();

    // ---- Add an animation (takes ownership) ----
    template<typename T>
    void add(std::unique_ptr<Animation<T>> anim) {
        static_assert(
            std::is_same_v<T, f32> ||
            std::is_same_v<T, Color> ||
            std::is_same_v<T, Vector2> ||
            std::is_same_v<T, Rectangle>,
            "AnimationManager only supports Animation<f32>, Animation<Color>, "
            "Animation<Vector2>, Animation<Rectangle>");
        appendTypedAnimation(std::move(anim));
    }

    void add(std::unique_ptr<Animation<f32>> anim);
    void add(std::unique_ptr<Animation<Color>> anim);
    void add(std::unique_ptr<Animation<Vector2>> anim);
    void add(std::unique_ptr<Animation<Rectangle>> anim);

    // ---- Per-frame update ----
    void update(f32 dt);

    // ---- Queries ----
    [[nodiscard]] size_t count() const noexcept { return m_animations.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_animations.empty(); }

    // ---- Cancel all animations with a given name ----
    void cancelAll(const std::string& name);

    // ---- Cancel all animations ----
    void cancelAll();

    // ---- Immediately remove cancelled/done animations ----
    void prune();

    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;
    AnimationManager(AnimationManager&&) = delete;
    AnimationManager& operator=(AnimationManager&&) = delete;

private:
    AnimationManager() = default;
    ~AnimationManager() = default;

    // ---- Polymorphic base for heterogeneous storage ----
    struct IAnimation {
        virtual ~IAnimation() = default;
        virtual void update(f32 dt) = 0;
        virtual void cancel() = 0;
        [[nodiscard]] virtual bool shouldRemove() const = 0;
        [[nodiscard]] virtual const std::string& name() const = 0;
    };

    template<typename T>
    struct AnimationWrapper final : IAnimation {
        std::unique_ptr<Animation<T>> anim;
        explicit AnimationWrapper(std::unique_ptr<Animation<T>> a) : anim(std::move(a)) {}
        void update(f32 dt) override { anim->update(dt); }
        void cancel() override { anim->cancel(); }
        [[nodiscard]] bool shouldRemove() const override { return anim->shouldRemove(); }
        [[nodiscard]] const std::string& name() const override { return anim->name(); }
    };

    void beginDispatch() noexcept;
    void endDispatch();
    void flushDeferredChanges();
    void appendAnimation(std::unique_ptr<IAnimation> anim);

    template<typename T>
    void appendTypedAnimation(std::unique_ptr<Animation<T>> anim) {
        appendAnimation(std::make_unique<AnimationWrapper<T>>(std::move(anim)));
        ::biofuel::engine::debug::MemoryTelemetry::add(
            ::biofuel::engine::debug::ResourceKind::Animation,
            1,
            0);
    }

    std::vector<std::unique_ptr<IAnimation>> m_animations;
    std::vector<std::unique_ptr<IAnimation>> m_pendingAnimations;
    u32 m_dispatchDepth = 0U;
    bool m_pruneRequested = false;
};

} // namespace biofuel::engine::animation
