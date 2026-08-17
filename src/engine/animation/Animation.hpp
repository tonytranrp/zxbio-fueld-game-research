#pragma once

#include "engine/core/Types.hpp"
#include "Easing.hpp"
#include <algorithm>
#include <raylib.h>
#include <string>
#include <functional>

namespace biofuel::engine::animation {

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
        const auto channel = [t](const u8 lhs, const u8 rhs) noexcept -> u8 {
            const f32 value = Lerp<f32>::call(static_cast<f32>(lhs), static_cast<f32>(rhs), t);
            return static_cast<u8>(std::clamp(value, 0.0f, 255.0f));
        };
        return Color{
            channel(a.r, b.r),
            channel(a.g, b.g),
            channel(a.b, b.b),
            channel(a.a, b.a),
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
// using a configurable easing function.
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
        Easing::Fn easing = Easing::linear
    ) noexcept
        : m_name{std::move(name)}
        , m_start{start}
        , m_end{end}
        , m_current{start}
        , m_duration{duration}
        , m_elapsed{0.0f}
        , m_easing{easing != nullptr ? easing : Easing::linear}
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
            const f32 easedT = m_easing != nullptr ? m_easing(rawT) : Easing::linear(rawT);
            m_current = AnimationUtils::Lerp<T>::call(m_start, m_end, easedT);
        }

        // Fire update callback
        if (m_onUpdate) {
            m_onUpdate(this);
        }

        // Fire complete
        if (m_done) {
            if (m_onComplete) {
                m_onComplete(this);
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
    void setEasing(Easing::Fn fn) noexcept { m_easing = fn != nullptr ? fn : Easing::linear; }

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
    std::string m_name;
    T m_start;
    T m_end;
    T m_current;
    f32 m_duration;
    f32 m_elapsed;
    Easing::Fn m_easing;

    UpdateCb   m_onUpdate;
    CompleteCb m_onComplete;
    CancelCb  m_onCancel;

    bool m_cancelled : 1;
    bool m_done      : 1;
    bool m_reversing : 1;
};

} // namespace biofuel::engine::animation
