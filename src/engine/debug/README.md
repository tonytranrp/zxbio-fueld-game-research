# engine/debug

Debug overlay and telemetry helpers live here. `MemoryTelemetry`'s tracking is
always compiled in, including Release builds — it's a handful of relaxed
atomics touched only on resource add/remove, so the runtime cost is
negligible and the in-game Memory overlay shows real numbers everywhere.
"Debug" describes the overlay UI, not when the data is collected.

## Current contents

```text
engine/debug/
|-- DebugOverlayService.hpp/.cpp
|-- DebugOverlayServiceModule.hpp
|-- MemoryTelemetry.hpp
`-- MemoryTelemetry.cpp
```

## How to use it

`MemoryTelemetry` tracks live counts and estimated bytes for engine-owned
resources. Managers and caches should report ownership changes at the point
where Raylib or heap resources are acquired/released.

```cpp
MemoryTelemetry::add(ResourceKind::ModelInstance, 1, estimatedBytes);
MemoryTelemetry::remove(ResourceKind::ModelInstance, 1, estimatedBytes);
MemoryTelemetry::snapshot("model.shutdown");
```

`DebugOverlayService` renders typed panels through one engine-owned overlay.
Use it instead of drawing one-off debug HUDs inside screens:

```cpp
auto& overlay = Runtime::debugOverlay();
overlay.setEnabled(true);
overlay.setPanelEnabled<biofuel::engine::debug::PhysicsDebugPanel>(true);
```

Add new panels by defining a panel tag, specializing `DebugPanelSpec<TPanel>`,
and adding the tag to `DebugPanelRegistry`.

## Coding standards

- Keep debug code safe to compile out or leave dormant in release builds.
- Do not make gameplay behavior depend on telemetry.
- Panels may read engine state through `Runtime::...`, but they must not own or
  mutate gameplay state.
- Resource add/remove calls should be paired near ownership boundaries.
- Prefer explicit `ResourceKind` entries over generic labels.
