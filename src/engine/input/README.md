# engine/input

Input polling service code lives here.

## Current contents

```text
engine/input/
|-- InputSystem.hpp
|-- InputSystem.cpp
`-- InputServiceModule.hpp
```

## How to use it

The application calls the typed input service once per frame before screen input
handling:

```cpp
Runtime::service<typed::InputService>().poll();
Runtime::screen().handleInput();
```

Screens still own their UI-specific key handling in `onInput()`. This service is
the place for shared input polling, translation, or future event publishing.

## Coding standards

- Keep polling side effects explicit and frame-bounded.
- Do not put screen navigation policy in the input system.
- Register the service through `InputServiceModule.hpp`.
- If this grows into action mapping, keep raw input and gameplay action layers
  separate.
