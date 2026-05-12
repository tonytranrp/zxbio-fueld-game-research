# engine/events/video

Video playback events fired by `VideoManager`.

| Event | Meaning |
| --- | --- |
| `VideoStartedEvent` | `play()` accepted and playback began |
| `VideoCompletedEvent` | Video reaches end-of-file for non-looping playback |
| `VideoErrorEvent` | Decoder, file, or stream setup failed |

All events carry `videoName` (`std::string_view`); `VideoErrorEvent` also carries `errorMessage`.

## How to use it

Publish these through the typed event facade when video playback state changes:

```cpp
Events::publish<typed::video::VideoStarted>({.videoName = name});
```

Screens should listen for these events only when they need cross-system
coordination. A screen that directly owns playback can also query `VideoManager`
state.

## Coding standards

- Keep payloads plain and small.
- Use `std::string_view` for registered video names.
- Decoder details belong in `engine/video/`, not event payloads.
- Register new video event tags in `VideoEventModule.hpp`.
