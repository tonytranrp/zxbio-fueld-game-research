# engine/fonts

Font loading and caching helpers.

## Current contents

```text
engine/fonts/
|-- FontUtils.hpp
`-- FontUtils.cpp
```

## FontManager

`FontManager` is a small singleton that:

- loads fonts by caller-chosen name
- unloads and replaces fonts safely
- returns loaded fonts by name
- falls back to `GetFontDefault()` when a lookup misses

## Guidance

- prefer `std::string_view` at the call boundary
- keep font ownership inside the manager
- use this manager only for cached font resources, not as a general dependency hub
