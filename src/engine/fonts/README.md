# engine/fonts

Font loading and caching helpers live here.

## Current contents

```text
engine/fonts/
|-- FontUtils.hpp/.cpp
|-- FontServiceModule.hpp
`-- README.md
```

## How to use it

Use the font manager when a screen or renderer needs a cached font by name:

```cpp
auto& fonts = biofuel::engine::runtime::Runtime::service<typed::FontService>();
fonts.load("pixel", "assets/fonts/pixel.ttf", 18);
Font font = fonts.get("pixel");
```

If a lookup misses, the manager falls back to `GetFontDefault()` so rendering
can continue.

## Coding standards

- Prefer `std::string_view` at the call boundary.
- Keep Raylib `Font` ownership inside the manager.
- Register the service through `FontServiceModule.hpp`.
- Do not use the font manager as a general dependency hub.
