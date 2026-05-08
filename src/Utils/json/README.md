# Utils/json — JSON Utilities

Wraps `nlohmann::json` for file I/O and string parsing.

## Architecture

```
Utils/json/
├── JsonUtils.hpp   ← JsonUtils class: loadFromFile, saveToFile, parseString
└── JsonUtils.cpp   ← Implementation
```

## Coding Standards

### 1. Json Type Alias

Use the project alias:
```cpp
using Json = nlohmann::json;
```

### 2. File Loading — Check Before Use

```cpp
Json data = JsonUtils::loadFromFile("config.json");
if (data.empty()) {
    // File didn't exist or was empty — use defaults
}
```

`loadFromFile()` never throws — it returns an empty object on failure.

### 3. Save With Indentation

```cpp
JsonUtils::saveToFile("save.json", data);  // Always indented (4 spaces)
```

### 4. String Parsing — Handle Failures Gracefully

```cpp
Json result = JsonUtils::parseString(rawJson);
if (result.is_discarded()) {
    // Invalid JSON
}
```

### 5. Don't Use Json as Game State

JSON is for **serialization boundaries** only — config files, save files, asset descriptors. Game state at runtime should use proper C++ structs. Load JSON once at startup, convert to structs, discard the JSON.

### 6. Types

- File paths: `const std::string&`
- Return values: `Json` (by value, nlohmann::json is move-optimized)

## Templates

None. This is a concrete utility wrapping a third-party library. The `Json` type alias hides `nlohmann::json`, and all functions operate on it directly.
