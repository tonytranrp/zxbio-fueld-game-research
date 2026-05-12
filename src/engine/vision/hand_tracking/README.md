# Hand Tracking Runtime

The hand-tracking runtime is optional and disabled by default. Enable it with:

```powershell
cmake -S . -B build-handtracking -DBIOFUEL_ENABLE_HAND_TRACKING=ON
cmake --build build-handtracking --config Debug --parallel
```

For the Visual Studio/CMake debug tree used by this workspace:

```powershell
cmake -S . -B out/build/x64-Debug -DBIOFUEL_ENABLE_HAND_TRACKING=ON
cmake --build out/build/x64-Debug --config Debug --parallel
```

When enabled, CMake fetches standalone Asio through CPM and provisions a
build-local Python 3.12 environment with `uv`. The app never relies on system
Python. The Python worker and pinned requirements live in
`tools/python/biofuel_hand_tracking/`.

Runtime shape:

- C++ launches a managed Python worker lazily, after in-app camera consent.
- Python uses MediaPipe Gesture Recognizer and OpenCV to detect hands.
- Landmark and gesture snapshots are binary UDP packets on port `40241`.
- C++ sends JSON-line control commands over TCP port `40242`.
- Dev preview frames use a TCP length-prefixed MJPEG stream on port `40243`.
- Rendering and smoothing remain C++ side; Python only detects hands.

Privacy and failure behavior:

- The worker does not open a camera until the game asks for consent and the user
  approves it for the current session.
- Missing Python packages, missing model assets, camera failure, packet timeout,
  or worker exit should leave the game running with tracking offline.
- Preview is intended for Debug/Dev UI only.

Packet format:

- Magic: `BHTK`
- Version: `1`
- Payload: up to 2 hands, handedness, confidence, gesture, 21 image landmarks,
  and 21 world landmarks per hand.
- C++ validates magic, version, packet size, and hand count before accepting a
  snapshot.
