# Utils

`Utils/` contains concrete shared helpers used across the runtime.

## Current subfolders

| Folder | Purpose |
|---|---|
| `render/` | draw helpers, shader access, shader modules |
| `ui/` | reusable menu rendering and hit-testing |
| `font/` | font loading and caching |

## Rules

- keep utilities small, concrete, and focused
- prefer helper functions or concrete managers over broad abstraction layers
- utilities may depend on other utility subfolders when the dependency is clear and one-directional
- utilities must not depend on concrete screens or gameplay-specific logic

## Modern C++ direction

This folder is a good place for conservative modern C++ improvements such as:

- `std::string_view`
- `std::span`
- `constexpr`
- `[[nodiscard]]`
- `noexcept`
- small RAII helpers for paired low-level APIs

Do not add templates here unless they solve real duplication for multiple callers.
