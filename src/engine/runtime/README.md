# engine/runtime

The runtime facade lives here. It is the typed access point for engine services.

## Current contents

```text
engine/runtime/
|-- Runtime.hpp
`-- typed/
```

## How to use it

Use `Runtime` from screens, app code, and systems that need registered services:

```cpp
auto& screens = biofuel::engine::runtime::Runtime::screen();
auto& models = biofuel::engine::runtime::Runtime::model();
auto& events = biofuel::engine::runtime::Runtime::events();
```

For uncommon services, use the templated form:

```cpp
auto& service = Runtime::service<biofuel::engine::runtime::typed::AudioService>();
```

## Coding standards

- Do not add another global service locator.
- Convenience accessors are allowed for commonly used services only.
- Optional service accessors must be feature-flag guarded.
- Keep registration mechanics in `runtime/typed/`.
