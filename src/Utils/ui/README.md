# Utils/ui — Reusable UI Widgets & Helpers

Shared UI utilities that screens compose together. No inheritance — just free functions and concrete utility structs.

## Architecture

```
Utils/ui/
├── MenuHelper.hpp   ← Vertical menu: render, keyboard nav, mouse hit-test
└── MenuHelper.cpp   ← MenuHelper implementation
```

## Coding Standards

### 1. Free Functions Over Classes

UI utilities should be **stateless free functions** in a namespace, not classes with state:

```cpp
// ✅ Free functions with explicit parameters
namespace utils::ui {
    void renderVerticalMenu(std::span<const MenuItem> items, i32 selected, ...);
    bool navigateVerticalMenu(i32& selected, i32 count, f32& cooldown, f32 dt, ...);
}

// ❌ Stateful widget class (only if truly needed)
class MenuWidget {
    std::vector<MenuItem> m_items;  // Avoid unless widget needs lifetime
    i32 m_selected;
};
```

### 2. Output via Parameters, Not Return Values

Functions that modify state take it by reference:

```cpp
// ✅ Clear: selectedIndex and cooldownTimer are modified in place
bool navigateVerticalMenu(i32& selectedIndex, i32 itemCount, f32& cooldownTimer, f32 dt, ...);

// ❌ Ambiguous: what does the bool mean? What happened to cooldown?
f32 navigateVerticalMenu(i32& selectedIndex, i32 itemCount, ...);
```

### 3. Result Structs for Multi-Value Returns

When a function returns multiple pieces of information, use a named struct:

```cpp
struct MenuHitResult {
    i32 hoveredIndex = -1;
    bool clicked = false;
};

[[nodiscard]] MenuHitResult hitTestVerticalMenu(...);
```

Never use sentinel values (like returning `-2` to mean "hovered but not clicked") — that's what the struct is for.

### 4. std::span for Array Parameters

All functions that accept arrays of items use `std::span`:

```cpp
void renderVerticalMenu(std::span<const MenuItem> items, ...);
```

This works with `std::array`, `std::vector`, and C arrays without copying.

### 5. Layout Structs for Visual Parameters

Group related layout constants into a `constexpr`-friendly struct:

```cpp
struct MenuLayout {
    i32 itemSpacing = 48;
    i32 fontSize = 26;
    i32 hitboxPaddingX = 16;
    i32 hitboxPaddingY = 4;
    Color colorNormal = LIGHTGRAY;
    Color colorSelected = YELLOW;
    Color colorLocked = {60, 60, 60, 255};
    f32 keyRepeatDelay = 0.12f;
};
```

Screens can override defaults using designated initializers:
```cpp
static constexpr MenuLayout MY_LAYOUT = {
    .itemSpacing = 44,
    .fontSize = 22,
};
```

## Templates

Template usage in this directory: **avoid unless there's a clear generic need.** If you find yourself writing a template, ask:

1. Will this be instantiated with more than 2 types? If not, use a concrete type or `std::string_view`.
2. Is the template just to avoid a `std::string` allocation? Use `std::string_view` instead.
3. Is it a container-agnostic algorithm? Use `std::span<const T>` — that's the only template you should need.
