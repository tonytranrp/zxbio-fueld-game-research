# IdleScreen

Ambient overlay pushed when the player is inactive on the main menu.

## Behavior

`IdleScreen` prefers the project idle video at `assets/video/ssstik.io_1778485755339.mp4`:

1. `preloadAssets()` initializes `VideoManager` and loads the configured idle MP4
2. `onEnter()` plays that MP4 through the FFmpeg-backed video manager and enters video mode only if playback starts successfully
3. `onRender()` draws the decoded frame as a normal fullscreen texture
4. `onInput()` pops back to the main menu after the input delay on Space, Enter, Escape, or mouse-button release

If no local MP4 is present, FFmpeg is unavailable, startup prebuffering times out, or playback fails, it falls back to the original dimmed shader and idle music path. This avoids showing the video backend's placeholder texture as a black screen.

## Development

Use `-DBIOFUEL_DEV_STARTUP_IDLE_VIDEO=ON` to start directly on this screen after the loading screen. This is the fast path for testing video playback without waiting for idle detection.

The MP4 path is intentionally fixed so adding other local videos under `assets/video/` does not change the idle screen by directory iteration order.
