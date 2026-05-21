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
struct ScreenSpec<ExampleScreen> {
    static constexpr ScreenId ID = ScreenId::Slot3;
    static constexpr std::string_view NAME = "ExampleScreen";
};

template<>
struct RenderLayers<ExampleScreen> {
    using Type = RenderLayerList<ExampleScreen, RenderElementList<ExampleHudElement>>;
};
```

## Render pipeline

`RenderPipeline<TScreen>` runs named render layers and elements. Layer names can
be toggled through screen override events, which is useful for debug tools.

`ScreenValidation.hpp` checks registered screens at compile time:

- screen IDs and names must be present and unique.
- render layers must use `RenderLayerList<TScreen, ...>`.
- named render layers/elements must have non-empty `NAME` values.
- every registered screen must be represented by the policy switch helpers.

## Coding standards

- Screen IDs are stable; do not reorder existing IDs casually.
- New `ScreenId` values must be reviewed against game-level global input policy; the engine registry should
  stay policy-neutral.
- Game-specific ID names should live outside engine headers; keep this enum
  limited to generic stable slots or opaque IDs.
- Keep stack policy explicit in specs.
- Render elements are compile-time types, not heap widgets.
- Prefer `RenderContext` over global lookups inside typed render elements.

## Audit notes

- `ScreenRegistry`, `ScreenValidation`, `ScreenDispatch`, and `RenderPipeline`
  form a small typed screen system. The duplicated lifecycle bridge functions
  are visible, but they preserve straightforward call paths and readable
  validation errors.
- Boost.SML is not justified for the current screen stack. Transitions are a
  simple `None` / `TransitionIn` / `TransitionOut` flow owned by
  `ScreenSlot` and `ScreenManager`; an SML graph would add dependency and
  indirection without deleting enough boilerplate.
- `magic_enum` or `frozen` may be useful later for enum names or constexpr
  policy lookup tables, but current policy switches are game-owned and guarded
  by compile-time validators.
- Boost.PFR, `glaze`, `fmt`, and Highway/SIMD do not target current UI typed
  code: there is no aggregate reflection, JSON serialization, direct formatting
  gap, or measured vectorizable hot loop in this layer.
