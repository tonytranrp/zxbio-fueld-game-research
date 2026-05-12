# engine/custom/procedural/materials

Procedural texture and material helpers live here.

## Current contents

```text
engine/custom/procedural/materials/
|-- ProceduralTextureCache.hpp
`-- ProceduralTextureCache.cpp
```

## How to use it

Use `ProceduralTextureCache` when generated meshes need shared Raylib textures
or PNG overrides. The cache owns texture lifetime so renderers can request a
stable handle instead of loading textures per draw.

```cpp
auto& cache = ProceduralTextureCache::instance();
Texture2D metal = cache.getOrCreate("robot_hand.metal", spec);
```

## Coding standards

- Raylib texture ownership belongs in caches, not screens.
- Cache keys should be stable dotted names.
- Generated defaults must work even when optional PNG overrides are missing.
- Keep material tuning data typed; avoid free-form stringly state in renderers.
