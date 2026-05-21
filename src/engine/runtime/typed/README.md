# engine/runtime/typed

Typed runtime service, event, asset, and shader declarations live here.

## Current contents

```text
engine/runtime/typed/
|-- ServiceBase.hpp / ServiceDeclare.hpp / Services.hpp / ServiceTags.hpp
|-- EventBase.hpp / EventDeclare.hpp / Events.hpp / EventTags.hpp
|-- AssetBase.hpp / AssetDeclare.hpp / AssetCatalog.hpp / Assets.hpp
`-- ShaderDeclare.hpp
```

Generated registry headers are included from this layer during the build.

## How to add a service

Create a service module near the backend it exposes:

```cpp
BIOFUEL_SERVICE_TAG(FarmService);
BIOFUEL_STATIC_SERVICE(FarmService, "service.farm", FarmServiceBackend);
BIOFUEL_SERVICE_MODULE(FarmServiceModule, FarmService)
```

Then include the module header from the build so
`GenerateTypedRegistries.cmake` can discover the macro.

## How to add an event

Create a payload in `engine/events/<domain>/`, define a tag/spec in the local
module, and publish through `Events`.

```cpp
BIOFUEL_EVENT_TAG(FarmTick, FarmTickEvent);
BIOFUEL_EVENT_SPEC(farm::FarmTick, "farm.tick");
BIOFUEL_EVENT_MODULE(FarmEventModule, AppEventRegistry, farm::FarmTick)
```

## How to group assets

Use asset catalogs when a screen or feature wants one readable list of related
typed assets without replacing generated registries.

```cpp
struct FarmPrototypeCatalog;

template<>
struct AssetCatalog<FarmPrototypeCatalog> {
    using Assets = biofuel::typed::Registry<
        VideoEntry<video::IdleAmbient>,
        SoundEntry<sound::MenuMove>>;
};

static_assert(validateAssetCatalog<FarmPrototypeCatalog>());
```

## Coding standards

- Tags are empty types; payloads and backends carry the data.
- Registry membership is compile-time and explicit.
- Runtime users should not include generated registry headers directly.
- Catalog entries must point at already registered typed assets or shaders.
- Labels use stable lowercase dotted names.

## Audit notes

- `AssetCatalog.hpp` has the most visible repetition: wrapper entries such as
  `ShaderEntry<T>` and `SoundEntry<T>`, traits, and validation concepts all
  encode the asset kind at compile time. This is deliberate until a replacement
  deletes that local code while preserving clear registration errors.
- Boost.PFR is not currently justified here: the repetition is tag/category
  metadata, not aggregate-field reflection, serialization, or comparison.
- Boost.MP11 is also deferred for this layer. The local registries provide
  concise domain errors such as "asset catalog entry must already be
  registered"; a generic typelist dependency should only be added if it removes
  meaningful local code without making those errors opaque.
- `glaze` should not replace `nlohmann/json` from this layer alone. Existing
  JSON usage lives in runtime/world and procedural-hand config code, not in the
  typed registry declarations.
