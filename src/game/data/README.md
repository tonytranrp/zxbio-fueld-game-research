# game/data

Fuel Farm balance data will live here.

## Current status

This folder now starts with `FuelFarmData.hpp`, a compact constexpr crop/fuel
table extracted from the project plan. It remains the home for game-domain data
from the research documents, such as crops, fuels, buildings, technologies,
events, prices, and ecology numbers.

## Expected use

Good candidates:

- `constexpr` crop and fuel tables
- tech tree definitions
- policy and weather event tables
- building conversion recipes

Not good candidates:

- reusable engine constants
- screen layout values
- raw research markdown copies

## Example

```cpp
struct CropData {
    std::string_view name;
    i32 yieldGallonsPerAcre = 0;
    i32 waterNeed = 0;
    i32 fertilizerNeed = 0;
    i32 carbonScore = 0;
};

inline constexpr CropData CROPS[] = {
    {"Corn Ethanol", 400, 4, 4, 3},
};
```

## Coding standards

- Prefer `constexpr` typed tables while the dataset is small.
- Include units in field names.
- Keep raw research citations in comments only when a number needs provenance.
- Move to JSON only when runtime modding or large data editing becomes real.
