# Build and Verification

## Standard Build

```powershell
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

## Visual Studio Build Tree

```powershell
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"" -arch=x64 -host_arch=x64 && cmake --build out\build\x64-Debug --config Debug --parallel"
ctest --test-dir out\build\x64-Debug -C Debug --output-on-failure
```

## Rust Physics Bridge

```powershell
cargo test --manifest-path src\engine\physics\rapier_bridge\Cargo.toml --locked
cargo clippy --manifest-path src\engine\physics\rapier_bridge\Cargo.toml --locked -- -D warnings
```

## Python Hand Tracking

```powershell
python -m compileall tools\python\biofuel_hand_tracking
```

## Related Notes

- [[src/engine/physics/README]]
- [[src/engine/vision/hand_tracking/README]]
- [[tools/python/biofuel_hand_tracking/README]]
