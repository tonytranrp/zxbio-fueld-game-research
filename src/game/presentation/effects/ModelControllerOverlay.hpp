#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>
#include <span>
#include <string_view>

namespace biofuel::game::presentation::effects {

enum class ModelControlScreenHalf : u8 {
    Any,
    Left,
    Right,
};

struct ModelControlTarget {
    std::string_view name;
    u64 instanceId = 0;
    std::string_view boneName;
    Vector3 baseWorldPosition{0.0f, 0.0f, 0.0f};
    Vector3* runtimeOffset = nullptr;
    ModelControlScreenHalf screenHalf = ModelControlScreenHalf::Any;
    bool editableX = true;
    bool editableY = true;
    bool editableZ = true;
    Color color{255, 220, 90, 255};
};

class ModelControllerOverlay final {
public:
    void update(std::span<ModelControlTarget> targets, const Camera3D& camera) noexcept;
    void render(std::span<const ModelControlTarget> targets, const Camera3D& camera) const noexcept;
    void reset() noexcept;

private:
    enum class Axis : u8 {
        None,
        X,
        Y,
        Z,
    };

    [[nodiscard]] static Vector3 worldPosition(const ModelControlTarget& target) noexcept;
    [[nodiscard]] static bool acceptsMouseHalf(const ModelControlTarget& target, Vector2 mouse) noexcept;
    [[nodiscard]] static f32 distanceToSegment(Vector2 point, Vector2 a, Vector2 b) noexcept;
    [[nodiscard]] static const char* axisName(Axis axis) noexcept;

    [[nodiscard]] i32 pickTarget(std::span<const ModelControlTarget> targets, const Camera3D& camera, Vector2 mouse) const noexcept;
    [[nodiscard]] Axis pickAxis(const ModelControlTarget& target, const Camera3D& camera, Vector2 mouse) const noexcept;
    void applyDrag(ModelControlTarget& target, Vector2 mouse) noexcept;
    void copySelected(std::span<const ModelControlTarget> targets) const noexcept;

    i32 m_selected = -1;
    Axis m_dragAxis = Axis::None;
    Vector2 m_lastMouse{0.0f, 0.0f};
};

} // namespace biofuel::game::presentation::effects
