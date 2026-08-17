# engine/graphics/components/Camera

Shader camera components smoothly animate camera transformations for
shader-based raymarchers. `CameraComponent` implements `ComponentModule` so it
can be updated and applied through a common interface.

## Current contents

```text
engine/graphics/components/Camera/
|-- CameraComponent.hpp
|-- ShaderCamera.hpp
|-- ShaderCamera.cpp
`-- README.md
```

## Core types

`ShaderCameraState` stores camera offset, yaw, and pitch values that map to GLSL
uniforms. `ShaderCameraController` interpolates between states using easing
functions. `CameraComponent` wraps that controller and applies the uniforms to a
shader.

## How to use it

There is no component manager — own a `CameraComponent` directly (e.g. as a
screen member) and drive it each frame:

```cpp
#include "engine/graphics/components/Camera/CameraComponent.hpp"

CameraComponent m_cameraComponent;

// when starting a move:
m_cameraComponent.controller().setTarget(
    ShaderCameraState{.yaw = -0.30f},
    2.0f,
    Easing::easeInOutCubic);

// per frame:
m_cameraComponent.update(dt);
m_cameraComponent.apply(shader);
```

## GLSL integration

```glsl
uniform float uCameraYaw = 0.0;

void main() {
    vec3 rd = normalize(vec3(uv, 1.0));

    if (abs(uCameraYaw) > 0.0001) {
        float cy = cos(uCameraYaw);
        float sy = sin(uCameraYaw);
        rd.xz = mat2(cy, -sy, sy, cy) * rd.xz;
    }

    rd += vec3(rd.x==0.0, rd.y==0.0, rd.z==0.0) * 1e-5;
}
```

## Coding standards

- Keep shader uniform names centralized in the component.
- Camera animation should be deterministic and driven by explicit `dt`.
- Do not read screen state from this component; callers set targets.
- Keep this component focused on shader cameras, not Raylib `Camera3D`.
