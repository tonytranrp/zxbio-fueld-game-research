# Components/Camera — Shader Camera System

Smoothly animates camera transformations in shader-based raymarchers. The
`CameraComponent` implements the `ComponentModule` interface so it integrates
with `ComponentManager` for bulk update/apply operations.

---

## Files

```text
Components/Camera/
├── CameraComponent.hpp    ← ComponentModule implementation (uniform apply)
├── ShaderCamera.hpp       ← ShaderCameraState + ShaderCameraController
├── ShaderCamera.cpp       ← Controller animation logic
└── README.md              ← This file
```

## Core Types

### `ShaderCameraState`

Raw camera parameters mapped to GLSL uniforms:

| Field    | Type  | GLSL Uniform      | Description                        |
|----------|-------|--------------------|------------------------------------|
| offsetX  | f32   | uCameraOffsetX     | Lateral camera position shift      |
| offsetY  | f32   | uCameraOffsetY     | Vertical camera position shift     |
| yaw      | f32   | uCameraYaw         | Horizontal look rotation (radians) |
| pitch    | f32   | *(reserved)*       | Vertical look rotation (radians)   |

### `ShaderCameraController`

Smoothly interpolates between `ShaderCameraState` values using configurable
easing functions. Supports `isComplete()` polling for chaining multi-phase
animation sequences.

### `CameraComponent`

`ComponentModule` implementation that wraps a `ShaderCameraController`. When
`apply(shader)` is called, it writes all camera uniforms automatically.

## Usage

### Standalone

```cpp
#include "engine/graphics/components/Camera/CameraComponent.hpp"

CameraComponent cam;
cam.controller().setTarget(
    ShaderCameraState{.yaw = -0.30f},
    2.0f,
    Easing::easeInOutCubic
);

// Per frame:
cam.update(dt);
cam.apply(shader);
```

### Via ComponentManager

```cpp
auto camera = std::make_unique<CameraComponent>();
camera->controller().setTarget(...);
components.add(std::move(camera));

// Per frame:
components.updateAll(dt);
components.applyAll(shader);

// Retrieve later:
auto* cam = components.getAs<CameraComponent>("camera");
cam->controller().setTarget(...);
```

### Multi-Phase Sequencing

```cpp
enum class CameraPhase { Idle, SweepToLeft, ReturnToCenter, Done };

void advanceCameraSequence() {
    auto* cam = m_components.getAs<CameraComponent>("camera");
    if (!cam->controller().isComplete()) return;

    switch (m_phase) {
    case CameraPhase::SweepToLeft:
        cam->controller().setTarget(
            ShaderCameraState{.yaw = -0.30f}, 2.0f
        );
        m_phase = CameraPhase::ReturnToCenter;
        break;
    // ...
    }
}
```

## GLSL Integration

```glsl
uniform float uCameraYaw = 0.0;

void main() {
    vec3 rd = normalize(vec3(uv, 1.0));

    // Apply yaw rotation
    if (abs(uCameraYaw) > 0.0001) {
        float cy = cos(uCameraYaw);
        float sy = sin(uCameraYaw);
        rd.xz = mat2(cy, -sy, sy, cy) * rd.xz;
    }

    // Epsilon fix AFTER rotation
    rd += vec3(rd.x==0.0, rd.y==0.0, rd.z==0.0) * 1e-5;
}
```
