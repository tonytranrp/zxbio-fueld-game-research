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
struct ScreenSpec<GamePlayScreen> {
    static constexpr ScreenId ID = ScreenId::Slot3;
    static constexpr std::string_view NAME = "GamePlayScreen";
};

template<>
struct RenderLayers<GamePlayScreen> {
    using Type = RenderLayerList<GamePlayScreen, RenderElementList<GamePlayHudElement>>;
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
- New `ScreenId` values must be reviewed against game-level global input policy
  such as pause eligibility; the engine registry should stay policy-neutral.
- Architecture follow-up: `ScreenId` currently lives in `engine/ui/typed` for
  this single-game vertical slice. If the engine is reused by multiple games,
  move game-specific IDs behind a game-owned or opaque registry boundary.
- Keep stack policy explicit in specs.
- Render elements are compile-time types, not heap widgets.
- Prefer `RenderContext` over global lookups inside typed render elements.
