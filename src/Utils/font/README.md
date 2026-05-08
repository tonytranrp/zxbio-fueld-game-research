# Utils/font — Font Management

Loads, caches, and provides Raylib `Font` objects by name.

## Architecture

```
Utils/font/
├── FontUtils.hpp   ← FontManager singleton: load, unload, get
└── FontUtils.cpp   ← FontManager implementation
```

## Coding Standards

### 1. Load Fonts by Name

```cpp
FontManager::instance().load("pixel", "assets/fonts/pixel.ttf", 16);
```

The name is your key for later lookups — use short, descriptive names.

### 2. Get With Fallback

```cpp
Font font = FontManager::instance().get("pixel");  // Returns default font if "pixel" not loaded
```

`get()` never returns an invalid font — it falls back to `GetFontDefault()`.

### 3. Unload When Replacing

```cpp
FontManager::instance().unload("pixel");     // Frees the old font
FontManager::instance().load("pixel", ...);  // Loads new version
```

### 4. Types

- Font handles: Raylib `Font` (by value — it's a lightweight handle)
- Font names: `const std::string&`
- Sizes: `i32` (base size in pixels)

## Templates

None. The font manager stores `Font` objects in an `std::unordered_map<std::string, Font>` — no templates needed at this layer.
