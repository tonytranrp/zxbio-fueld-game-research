# engine/audio

Music, sound loading, playback, and audio asset/service registration live here.

## Current contents

```text
engine/audio/
|-- AudioManager.hpp/.cpp
|-- AudioAssetModule.hpp
|-- AudioServiceModule.hpp
`-- README.md
```

## How to use it

Most game code should reach audio through the runtime service:

```cpp
auto& audio = biofuel::engine::runtime::Runtime::audio();
audio.loadMusic("menu", "assets/audio/menu.ogg");
audio.playMusic("menu");
audio.setMusicVolume(0.75f);
```

Typed audio assets belong in `AudioAssetModule.hpp` so startup/preload code can
discover them through generated registries.

## AudioManager

`AudioManager` owns Raylib `Music` and `Sound` handles, updates streamed music,
and unloads resources on shutdown. It uses `TransparentHash` for
`std::string_view`-compatible map lookups without constructing temporary
strings.

## Coding standards

- Keep raw Raylib audio handles inside the manager.
- Use stable asset names; callers should not depend on file paths after load.
- Use `constexpr` for default volume and timing values.
- Register service access through `AudioServiceModule.hpp`.
- Audio failures should be recoverable; screens need fallback behavior.
