# engine/custom/procedural/mesh

Generated mesh ownership and caching lives here.

## Current contents

```text
engine/custom/procedural/mesh/
|-- ProceduralMeshCache.hpp
`-- ProceduralMeshCache.cpp
```

## How to use it

Renderers should request generated primitive meshes from the cache instead of
rebuilding Raylib meshes every frame.

```cpp
auto& cache = ProceduralMeshCache::instance();
Model phalanx = cache.modelFor(spec);
```

The cache should be the only place that knows when generated `Mesh` or `Model`
objects are created, reused, and unloaded.

## Coding standards

- Keep mesh specs hashable or comparable through stable typed fields.
- Do not store screen pointers or camera state in cached resources.
- Unload Raylib resources in cache shutdown/destructors.
- Add telemetry when a new cache starts owning significant resources.
