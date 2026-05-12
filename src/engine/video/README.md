# engine/video

Video playback for screen overlays, currently used by `IdleScreen`.

## Current Backend

`VideoManager` uses `ffmpeg.exe` as an external decoder process on Windows:

- video is decoded to raw RGBA frames and uploaded to a Raylib `Texture2D`
- audio is decoded to raw 44.1 kHz stereo PCM and streamed through Raylib `AudioStream`
- playback prebuffers decoded video/audio before reporting success, then uploads the first video frame before starting audio
- the game does not ship FFmpeg DLLs or decode MP4 internals by hand
- CMake records `ffmpeg.exe` when found; runtime falls back to searching `PATH`

This keeps the game executable small and avoids the previous libmpv vendor/DLL path. A local FFmpeg install is required for MP4 playback.

## Lifecycle

```cpp
auto& vm = engine::video::VideoManager::instance();
vm.init();
vm.loadVideo("idle", "assets/video/idle.mp4");
vm.setLooping("idle", true);
vm.play("idle");

// Each app update:
vm.update();

// Each render:
Texture2D frame = vm.getFrameTexture("idle");
```

`play()` can fail if FFmpeg starts but does not deliver enough decoded video/audio within the startup prebuffer window. Screens should only switch into video rendering after `isPlaying(name)` returns true; otherwise use their fallback presentation.

## Smooth Playback Notes

- `UpdateTexture()` is called only after a decoded frame exists. If the decode queue temporarily underflows, the previous texture remains visible instead of replacing it with black.
- The first decoded frame is uploaded synchronously during `play()` so the screen never enters video mode with the initial placeholder texture.
- Audio chunks match the Raylib stream sub-buffer size. Keep `AUDIO_FRAMES_PER_CHUNK` aligned with `SetAudioStreamBufferSizeDefault()` if those constants change.
- Video/audio decoding currently uses separate FFmpeg processes. This is adequate for ambient looping overlays, but not intended as a cutscene-grade sync system.

## Asset Policy

`assets/video/*.mp4` is local-only by default. Commit only project-owned or redistributable clips. The idle screen currently uses a fixed project video path (`assets/video/ssstik.io_1778485755339.mp4`) so additional local videos do not change runtime behavior by directory iteration order.

## Failure Behavior

If FFmpeg is missing, the MP4 is missing, startup prebuffering times out, or decoding fails, `VideoManager` marks the video as errored, fires `VideoErrorEvent`, and screens should fall back to their non-video presentation.
