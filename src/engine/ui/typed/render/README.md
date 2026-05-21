# engine/ui/typed/render

Reusable typed render element definitions live here.

## Current contents

```text
engine/ui/typed/render/
|-- RenderElements.hpp
`-- EffectElements.hpp
```

## How to use it

Render elements are small compile-time nodes consumed by `RenderPipeline`.
Define a `NAME` when tooling should be able to toggle the element.

```cpp
struct ExampleHudElement {
    static constexpr std::string_view NAME = "example.hud";

    static void render(ExampleScreen& screen, RenderContext& context) {
        // draw using screen state and context dimensions
    }
};
```

## Coding standards

- Elements should be stateless and use screen state passed to `render()`.
- Avoid raw service lookups when `RenderContext` already supplies what you need.
- Keep reusable generic elements here; screen-only draw code stays with the
  screen.
- Names use lowercase dotted labels.
