# Utils/audio

Music and sound loading, playback, and singleton lifecycle.

## Current contents

```text
Utils/audio/
|-- AudioManager.hpp
|-- AudioManager.cpp
`-- README.md
```

## AudioManager

Singleton that loads and plays music and sound effects. Uses `TransparentHash` for `string_view`-compatible map lookups without heap allocation.

### Key API

- `loadMusic(name, path)` — load a music file, identified by name
- `playMusic(name, loop)` — start playback immediately
- `stopMusic()` — stop current music
- `hasMusic(name)` — check if a music file is loaded
- `setMusicVolume(vol)` — global music volume (0.0–1.0)
- `isMusicPlaying()` — query playback state

### Design notes

- `get()` accessor is `noexcept` — all 7 manager singletons in the project follow this convention
- Uses `TransparentHash` from `Core/Types.hpp` for `std::unordered_map` lookups with `std::string_view` keys without constructing `std::string` temporaries
- Owns Raylib `Music` and `Sound` handles; closed on destruction

## Dependencies

- `Core/Types.hpp` for `TransparentHash`, `f32`
- Raylib `Music`, `Sound`

## Coding standards

- Singleton pattern with `noexcept` accessors
- `constexpr` for default volume values
- Keep raw Raylib audio handle management inside the manager
