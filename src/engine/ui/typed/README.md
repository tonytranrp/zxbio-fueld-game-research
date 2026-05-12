# engine/ui/typed

Typed screen registration, stack policy, render pipeline, and lifecycle adapters
live here.

## Current contents

```text
engine/ui/typed/
|-- ScreenTypes.hpp / ScreenSpec.hpp / ScreenRegistry.hpp
|-- ScreenModule.hpp / ScreenLifecycle.hpp / ScreenRuntime.hpp
|-- TypedScreenManager.hpp / TypedScreenStack.hpp / ScreenSlot.hpp
|-- ScreenCommandQueue.hpp / ScreenCatalog.hpp / ScreenValidation.hpp
|-- RenderContext.hpp/.cpp / RenderLayers.hpp / RenderPipeline.hpp
`-- render/
```

## How to add a screen

1. Create the screen under `game/screens/<name>/`.
2. Add a `*ScreenModule.hpp` next to the screen.
3. Define the `ScreenSpec<TScreen>` with ID, name, transition policy, and stack
   policy.
4. Register the module so the generated screen registry can see it.

```cpp
template<>
struct ScreenSpec<FarmScreen> {
    static constexpr ScreenId Id = ScreenId::Farm;
    static constexpr std::string_view Name = "Farm";
    static constexpr TransitionPolicyData Transition{};
    static constexpr StackPolicyData Stack{};
};
```

## Render pipeline

`RenderPipeline<TScreen>` runs named render layers and elements. Layer names can
be toggled through screen override events, which is useful for debug tools.

## Coding standards

- Screen IDs are stable; do not reorder existing IDs casually.
- Keep stack policy explicit in specs.
- Render elements are compile-time types, not heap widgets.
- Prefer `RenderContext` over global lookups inside typed render elements.
