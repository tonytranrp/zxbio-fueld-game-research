#include "ModelControllerOverlay.hpp"
#include "Utils/render/Render.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <spdlog/spdlog.h>

namespace biofuel::animation::screen {

namespace {

constexpr f32 POINT_RADIUS = 7.0f;
constexpr f32 PICK_RADIUS = 15.0f;
constexpr f32 AXIS_PICK_RADIUS = 8.0f;
constexpr f32 AXIS_WORLD_LENGTH = 0.22f;
constexpr f32 SCREEN_DRAG_SCALE = 0.006f;
constexpr f32 DEPTH_DRAG_SCALE = 0.004f;

[[nodiscard]] f32 lengthSquared(const Vector2 value) noexcept {
    return value.x * value.x + value.y * value.y;
}

[[nodiscard]] Vector2 subtract(const Vector2 a, const Vector2 b) noexcept {
    return Vector2{a.x - b.x, a.y - b.y};
}

[[nodiscard]] Color withAlpha(const Color color, const u8 alpha) noexcept {
    return Color{color.r, color.g, color.b, alpha};
}

} // namespace

void ModelControllerOverlay::update(std::span<ModelControlTarget> targets, const Camera3D& camera) noexcept {
    if (targets.empty()) {
        reset();
        return;
    }

    if (m_selected >= static_cast<i32>(targets.size())) {
        m_selected = -1;
        m_dragAxis = Axis::None;
    }

    const Vector2 mouse = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        m_dragAxis = Axis::None;
        if (m_selected >= 0) {
            m_dragAxis = pickAxis(targets[static_cast<size_t>(m_selected)], camera, mouse);
        }

        if (m_dragAxis == Axis::None) {
            m_selected = pickTarget(targets, camera, mouse);
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        m_dragAxis = Axis::None;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && m_selected >= 0 && m_dragAxis != Axis::None) {
        applyDrag(targets[static_cast<size_t>(m_selected)], mouse);
    }

    if (IsKeyPressed(KEY_C)) {
        copySelected(targets);
    }

    m_lastMouse = mouse;
}

void ModelControllerOverlay::render(std::span<const ModelControlTarget> targets, const Camera3D& camera) const noexcept {
    using utils::render::Renderer;

    for (size_t index = 0; index < targets.size(); ++index) {
        const auto& target = targets[index];
        const Vector2 screen = GetWorldToScreen(worldPosition(target), camera);
        const bool selected = static_cast<i32>(index) == m_selected;
        const f32 radius = selected ? POINT_RADIUS + 3.0f : POINT_RADIUS;
        DrawCircleV(screen, radius + 3.0f, Color{0, 0, 0, 150});
        DrawCircleV(screen, radius, selected ? YELLOW : target.color);
        DrawCircleLines(static_cast<i32>(screen.x), static_cast<i32>(screen.y), radius + 2.0f, WHITE);
    }

    if (m_selected >= 0 && m_selected < static_cast<i32>(targets.size())) {
        const auto& target = targets[static_cast<size_t>(m_selected)];
        const Vector3 originWorld = worldPosition(target);
        const Vector2 origin = GetWorldToScreen(originWorld, camera);
        const Vector2 xEnd = GetWorldToScreen(Vector3{originWorld.x + AXIS_WORLD_LENGTH, originWorld.y, originWorld.z}, camera);
        const Vector2 yEnd = GetWorldToScreen(Vector3{originWorld.x, originWorld.y + AXIS_WORLD_LENGTH, originWorld.z}, camera);
        const Vector2 zEnd = GetWorldToScreen(Vector3{originWorld.x, originWorld.y, originWorld.z + AXIS_WORLD_LENGTH}, camera);

        if (target.editableX) {
            DrawLineEx(origin, xEnd, 3.0f, RED);
            DrawCircleV(xEnd, 5.0f, RED);
        }
        if (target.editableY) {
            DrawLineEx(origin, yEnd, 3.0f, GREEN);
            DrawCircleV(yEnd, 5.0f, GREEN);
        }
        if (target.editableZ) {
            DrawLineEx(origin, zEnd, 3.0f, BLUE);
            DrawCircleV(zEnd, 5.0f, BLUE);
        }

        const i32 panelX = 18;
        const i32 panelY = 44;
        DrawRectangle(panelX - 8, panelY - 8, 500, 118, Color{8, 10, 18, 210});
        DrawRectangleLines(panelX - 8, panelY - 8, 500, 118, Color{90, 120, 180, 180});

        char line[192]{};
        const Vector3 offset = target.runtimeOffset ? *target.runtimeOffset : Vector3{0.0f, 0.0f, 0.0f};
        std::snprintf(line, sizeof(line), "Model Controller: %.*s", static_cast<int>(target.name.size()), target.name.data());
        Renderer::drawText(line, panelX, panelY, 14, WHITE);
        std::snprintf(line, sizeof(line), "offset = Vector3{%.3ff, %.3ff, %.3ff}", offset.x, offset.y, offset.z);
        Renderer::drawText(line, panelX, panelY + 22, 14, Color{210, 226, 255, 255});
        std::snprintf(line, sizeof(line), "axis: %s | drag arrows | C copies value", axisName(m_dragAxis));
        Renderer::drawText(line, panelX, panelY + 44, 14, Color{160, 178, 210, 255});

        const char* halfText = "Any";
        if (target.screenHalf == ModelControlScreenHalf::Left) {
            halfText = "Left half";
        } else if (target.screenHalf == ModelControlScreenHalf::Right) {
            halfText = "Right half";
        }
        std::snprintf(line, sizeof(line), "instance: %llu | half: %s", static_cast<unsigned long long>(target.instanceId), halfText);
        Renderer::drawText(line, panelX, panelY + 66, 14, Color{160, 178, 210, 255});
    }

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();
    DrawLine(sw / 2, 0, sw / 2, sh, withAlpha(WHITE, 54));
}

void ModelControllerOverlay::reset() noexcept {
    m_selected = -1;
    m_dragAxis = Axis::None;
    m_lastMouse = Vector2{0.0f, 0.0f};
}

Vector3 ModelControllerOverlay::worldPosition(const ModelControlTarget& target) noexcept {
    const Vector3 offset = target.runtimeOffset ? *target.runtimeOffset : Vector3{0.0f, 0.0f, 0.0f};
    return Vector3{
        target.baseWorldPosition.x + offset.x,
        target.baseWorldPosition.y + offset.y,
        target.baseWorldPosition.z + offset.z,
    };
}

bool ModelControllerOverlay::acceptsMouseHalf(const ModelControlTarget& target, const Vector2 mouse) noexcept {
    const f32 halfX = static_cast<f32>(GetScreenWidth()) * 0.5f;
    if (target.screenHalf == ModelControlScreenHalf::Left) {
        return mouse.x <= halfX;
    }
    if (target.screenHalf == ModelControlScreenHalf::Right) {
        return mouse.x >= halfX;
    }
    return true;
}

f32 ModelControllerOverlay::distanceToSegment(const Vector2 point, const Vector2 a, const Vector2 b) noexcept {
    const Vector2 ab = subtract(b, a);
    const f32 denom = std::max(lengthSquared(ab), 0.0001f);
    const Vector2 ap = subtract(point, a);
    const f32 t = std::clamp((ap.x * ab.x + ap.y * ab.y) / denom, 0.0f, 1.0f);
    const Vector2 closest{a.x + ab.x * t, a.y + ab.y * t};
    return std::sqrt(lengthSquared(subtract(point, closest)));
}

const char* ModelControllerOverlay::axisName(const Axis axis) noexcept {
    switch (axis) {
    case Axis::X: return "X";
    case Axis::Y: return "Y";
    case Axis::Z: return "Z";
    case Axis::None: return "none";
    }
    return "none";
}

i32 ModelControllerOverlay::pickTarget(
    const std::span<const ModelControlTarget> targets,
    const Camera3D& camera,
    const Vector2 mouse) const noexcept
{
    i32 selected = -1;
    f32 bestDistance = PICK_RADIUS;
    for (size_t index = 0; index < targets.size(); ++index) {
        const auto& target = targets[index];
        if (!acceptsMouseHalf(target, mouse)) {
            continue;
        }

        const Vector2 screen = GetWorldToScreen(worldPosition(target), camera);
        const f32 distance = std::sqrt(lengthSquared(subtract(mouse, screen)));
        if (distance <= bestDistance) {
            bestDistance = distance;
            selected = static_cast<i32>(index);
        }
    }
    return selected;
}

ModelControllerOverlay::Axis ModelControllerOverlay::pickAxis(
    const ModelControlTarget& target,
    const Camera3D& camera,
    const Vector2 mouse) const noexcept
{
    if (!acceptsMouseHalf(target, mouse)) {
        return Axis::None;
    }

    const Vector3 originWorld = worldPosition(target);
    const Vector2 origin = GetWorldToScreen(originWorld, camera);
    const Vector2 xEnd = GetWorldToScreen(Vector3{originWorld.x + AXIS_WORLD_LENGTH, originWorld.y, originWorld.z}, camera);
    const Vector2 yEnd = GetWorldToScreen(Vector3{originWorld.x, originWorld.y + AXIS_WORLD_LENGTH, originWorld.z}, camera);
    const Vector2 zEnd = GetWorldToScreen(Vector3{originWorld.x, originWorld.y, originWorld.z + AXIS_WORLD_LENGTH}, camera);

    Axis bestAxis = Axis::None;
    f32 bestDistance = AXIS_PICK_RADIUS;
    if (target.editableX) {
        const f32 distance = distanceToSegment(mouse, origin, xEnd);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestAxis = Axis::X;
        }
    }
    if (target.editableY) {
        const f32 distance = distanceToSegment(mouse, origin, yEnd);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestAxis = Axis::Y;
        }
    }
    if (target.editableZ) {
        const f32 distance = distanceToSegment(mouse, origin, zEnd);
        if (distance < bestDistance) {
            bestAxis = Axis::Z;
        }
    }
    return bestAxis;
}

void ModelControllerOverlay::applyDrag(ModelControlTarget& target, const Vector2 mouse) noexcept {
    if (target.runtimeOffset == nullptr) {
        return;
    }

    const Vector2 delta = subtract(mouse, m_lastMouse);
    switch (m_dragAxis) {
    case Axis::X:
        if (target.editableX) {
            target.runtimeOffset->x += delta.x * SCREEN_DRAG_SCALE;
        }
        break;
    case Axis::Y:
        if (target.editableY) {
            target.runtimeOffset->y -= delta.y * SCREEN_DRAG_SCALE;
        }
        break;
    case Axis::Z:
        if (target.editableZ) {
            target.runtimeOffset->z += (delta.x - delta.y) * DEPTH_DRAG_SCALE;
        }
        break;
    case Axis::None:
        break;
    }
}

void ModelControllerOverlay::copySelected(std::span<const ModelControlTarget> targets) const noexcept {
    if (m_selected < 0 || m_selected >= static_cast<i32>(targets.size())) {
        return;
    }

    const auto& target = targets[static_cast<size_t>(m_selected)];
    const Vector3 offset = target.runtimeOffset ? *target.runtimeOffset : Vector3{0.0f, 0.0f, 0.0f};
    char text[256]{};
    std::snprintf(
        text,
        sizeof(text),
        "%.*s offset = Vector3{%.3ff, %.3ff, %.3ff};",
        static_cast<int>(target.name.size()),
        target.name.data(),
        offset.x,
        offset.y,
        offset.z
    );
    SetClipboardText(text);
    spdlog::info("ModelControllerOverlay: {}", text);
}

} // namespace biofuel::animation::screen
