# engine/debug

Debug-only and telemetry helpers live here.

## Current contents

```text
engine/debug/
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

## Coding standards

- Keep debug code safe to compile out or leave dormant in release builds.
- Do not make gameplay behavior depend on telemetry.
- Resource add/remove calls should be paired near ownership boundaries.
- Prefer explicit `ResourceKind` entries over generic labels.
