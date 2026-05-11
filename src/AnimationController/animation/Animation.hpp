#pragma once

#include "Core/Types.hpp"
#include "Easing.hpp"
#include <entt/signal/dispatcher.hpp>
#include <raylib.h>
#include <string>
#include <functional>
#include <type_traits>

namespace biofuel::animation {

// ------------------------------------------------------------------------------
// Animation Events
// Fired on the global event bus so screens can subscribe via entt sinks.
// ------------------------------------------------------------------------------

struct AnimationUpdateEvent {
    std::string name;
    f32 progress;   // normalized [0, 1]
    f32 value;      // for f32 animations only
};

struct AnimationCompleteEvent {
    std::string name;
};

struct AnimationCancelEvent {
    std::string name;
};

// ------------------------------------------------------------------------------
// AnimationUtils - Type-safe lerp for Animation<T>
// Specialize Lerp<T> for any type you want to animate.
// ------------------------------------------------------------------------------

namespace AnimationUtils {

template<typename T>
struct Lerp {
    static T call(const T& a, const T& b, f32 t) {
        static_assert(sizeof(T) == 0,
            "Lerp<T>: no specialization for this type. "
            "Add a Lerp<YourType> specialization in Animation.hpp.");
        return a;  // unreachable, silences return-value warnings
    }
};

template<>
struct Lerp<f32> {
    [[nodiscard]] static f32 call(f32 a, f32 b, f32 t) noexcept {
        return a + (b - a) * t;
    }
};

template<>
struct Lerp<Color> {
    [[nodiscard]] static Color call(const Color& a, const Color& b, f32 t) noexcept {
        return Color{
            static_cast<u8>(Lerp<f32>::call(static_cast<f32>(a.r), static_cast<f32>(b.r), t)),
            static_cast<u8>(Lerp<f32>::call(static_cast<f32>(a.g), static_cast<f32>(b.g), t)),
            static_cast<u8>(Lerp<f32>::call(static_cast<f32>(a.b), static_cast<f32>(b.b), t)),
            static_cast<u8>(Lerp<f32>::call(static_cast<f32>(a.a), static_cast<f32>(b.a), t)),
        };
    }
};

template<>
struct Lerp<Vector2> {
    [[nodiscard]] static Vector2 call(const Vector2& a, const Vector2& b, f32 t) noexcept {
        return Vector2{
            Lerp<f32>::call(a.x, b.x, t),
            Lerp<f32>::call(a.y, b.y, t),
        };
    }
};

template<>
struct Lerp<Vector3> {
    [[nodiscard]] static Vector3 call(const Vector3& a, const Vector3& b, f32 t) noexcept {
        return Vector3{
            Lerp<f32>::call(a.x, b.x, t),
            Lerp<f32>::call(a.y, b.y, t),
            Lerp<f32>::call(a.z, b.z, t),
        };
    }
};

template<>
struct Lerp<Rectangle> {
    [[nodiscard]] static Rectangle call(const Rectangle& a, const Rectangle& b, f32 t) noexcept {
        return Rectangle{
            Lerp<f32>::call(a.x, b.x, t),
            Lerp<f32>::call(a.y, b.y, t),
            Lerp<f32>::call(a.width, b.width, t),
            Lerp<f32>::call(a.height, b.height, t),
        };
    }
};

} // namespace AnimationUtils

// ------------------------------------------------------------------------------
// Animation<T> - Core animation template
// Interpolates a value of type T from start to end over a given duration,
// using a configurable easing function. Fires events on the global entt dispatcher.
// ------------------------------------------------------------------------------

template<typename T>
class Animation {
public:
    using UpdateCb   = std::function<void(Animation<T>*)>;
    using CompleteCb = std::function<void(Animation<T>*)>;
    using CancelCb  = std::function<void(Animation<T>*)>;

    Animation(
        std::string name,
        T start,
        T end,
        f32 duration,
        Easing::Fn easing = Easing::linear,
        entt::dispatcher* dispatcher = nullptr
    ) noexcept
        : m_name{std::move(name)}
        , m_start{start}
        , m_end{end}
        , m_current{start}
        , m_duration{duration}
        , m_elapsed{0.0f}
        , m_easing{easing}
        , m_dispatcher{dispatcher}
        , m_cancelled{false}
        , m_done{false}
        , m_reversing{false}
    {}

    // ---- Per-frame update ----
    void update(f32 dt) {
        if (m_cancelled || m_done) {
            return;
        }

        m_elapsed += dt;
        f32 rawT = (m_duration > 0.0f) ? (m_elapsed / m_duration) : 1.0f;

        if (rawT >= 1.0f) {
            rawT = 1.0f;
            m_done = true;
            m_current = m_reversing ? m_start : m_end;
        } else {
            m_current = AnimationUtils::Lerp<T>::call(m_start, m_end, m_easing(rawT));
        }

        // Fire update callback
        if (m_onUpdate) {
            m_onUpdate(this);
        }

        // Fire global event
        if (m_dispatcher && !m_done) {
            AnimationUpdateEvent ev{.name = m_name, .progress = rawT, .value = currentRaw()};
            m_dispatcher->trigger(ev);
        }

        // Fire complete
        if (m_done) {
            if (m_onComplete) {
                m_onComplete(this);
            }
            if (m_dispatcher) {
                m_dispatcher->trigger(AnimationCompleteEvent{.name = m_name});
            }
        }
    }

    // ---- Cancel this animation ----
    void cancel() noexcept {
        if (m_cancelled) return;
        m_cancelled = true;
        if (m_onCancel) {
            m_onCancel(this);
        }
        if (m_dispatcher) {
            m_dispatcher->trigger(AnimationCancelEvent{.name = m_name});
        }
    }

    // ---- Reverse direction (swap start/end, play backward) ----
    void reverse() noexcept {
        std::swap(m_start, m_end);
        m_reversing = !m_reversing;
        m_elapsed = 0.0f;
        m_cancelled = false;
        m_done = false;
    }

    // ---- Accessors ----
    [[nodiscard]] T current() const noexcept { return m_current; }
    [[nodiscard]] f32 progress() const noexcept {
        return m_duration > 0.0f ? (m_elapsed / m_duration) : 1.0f;
    }
    [[nodiscard]] bool isDone() const noexcept { return m_done; }
    [[nodiscard]] bool isCancelled() const noexcept { return m_cancelled; }
    [[nodiscard]] bool shouldRemove() const noexcept { return m_cancelled || m_done; }
    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    [[nodiscard]] f32 duration() const noexcept { return m_duration; }
    [[nodiscard]] f32 elapsed() const noexcept { return m_elapsed; }

    // ---- Setters for chaining ----
    void setEasing(Easing::Fn fn) noexcept { m_easing = fn; }

    Animation& onUpdate(UpdateCb cb) noexcept {
        m_onUpdate = std::move(cb);
        return *this;
    }
    Animation& onComplete(CompleteCb cb) noexcept {
        m_onComplete = std::move(cb);
        return *this;
    }
    Animation& onCancel(CancelCb cb) noexcept {
        m_onCancel = std::move(cb);
        return *this;
    }

private:
    [[nodiscard]] f32 currentRaw() const noexcept {
        if constexpr (std::is_same_v<T, f32>) {
            return m_current;
        } else {
            return 0.0f;
        }
    }

    std::string m_name;
    T m_start;
    T m_end;
    T m_current;
    f32 m_duration;
    f32 m_elapsed;
    Easing::Fn m_easing;
    entt::dispatcher* m_dispatcher = nullptr;

    UpdateCb   m_onUpdate;
    CompleteCb m_onComplete;
    CancelCb  m_onCancel;

    bool m_cancelled : 1;
    bool m_done      : 1;
    bool m_reversing : 1;
};

} // namespace biofuel::animation