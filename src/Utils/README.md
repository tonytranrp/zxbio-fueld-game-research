# Utils — Reusable Utilities

The `Utils/` directory contains self-contained utility modules that are reusable across the entire engine. Each subfolder has its own README with domain-specific standards.

## Subdirectories

| Directory | Purpose | Key Types |
|-----------|---------|-----------|
| `render/` | Raylib draw wrappers | `Renderer` (static) |
| `event/` | EventBus wrapper | `EventBus` (entt wrapper) |
| `ui/` | Reusable UI helpers | `MenuHelper` (free functions), `MenuItem`, `MenuLayout` |
| `json/` | JSON file I/O | `JsonUtils` (static), `Json` (nlohmann alias) |
| `task/` | Parallel task execution | `TaskSystem` (static) |
| `font/` | Font loading/caching | `FontManager` (singleton) |

## Cross-Cutting Rules

### 1. No Dependencies Between Utils

Each `Utils/` subfolder must be **self-contained**. A utility in `ui/` should not depend on `json/`. If two utilities need to share code, extract the shared part into its own utility or elevate it to a system.

### 2. Utils Are Leaf-Level

Utils may depend on:
- `Core/Types.hpp` (project types)
- Third-party libraries (Raylib, nlohmann, entt, etc.)
- Other Utils in the same subfolder

Utils must **never** depend on:
- `Data/` (event system, screens)
- `UI/` (screen classes)
- `Systems/` (game systems)
- `Application` (app lifecycle)

### 3. Static Classes or Free Functions

No singleton pattern in Utils (except `FontManager` which caches GPU resources). Prefer:
1. **Free functions in a namespace** (best — no state)
2. **Static class methods** (good — groups related functions)
3. **Singleton** (only when caching non-trivial resources like GPU fonts)

```cpp
// Tier 1: Free functions (best)
namespace utils::ui {
    void renderMenu(...);
}

// Tier 2: Static class (ok for grouping)
class Renderer {
public:
    static void beginFrame(...);
};

// Tier 3: Singleton (only for resource caching)
class FontManager {
public:
    static FontManager& instance();
};
```

### 4. Header-Only When Possible

If a utility has no `.cpp`-side state (static variables, heavy includes), make it header-only. This reduces build complexity.

### 5. Third-Party Includes Stay in .cpp

Headers should expose minimal third-party types. Wrap them:

```cpp
// ❌ .hpp leaks third-party types
#include <nlohmann/json.hpp>
nlohmann::json loadConfig();

// ✅ .hpp hides third-party types
using Json = nlohmann::json;
Json loadConfig();
```
