# Hand Capture Studio

Windows GUI wrapper around the cloned MobileHand runtime in `../mobilehand`.

## What It Does

- Opens a webcam without relaunching the old demo script.
- Runs MobileHand inference on the live camera crop.
- Draws the predicted 2D skeleton over the live camera feed.
- Captures the current estimated MANO hand mesh as `.obj`.
- Writes matching `.json` pose/keypoint/parameter metadata.
- Reports basic mesh artifact checks: non-finite values, degenerate faces, bounding box size, and max edge length.
- Adds live smoothing to reduce prediction jitter.
- Adds adaptive hand crop tracking before MobileHand inference. Auto mode tries MediaPipe when installed, then motion and skin-color fallback, before using a center crop.
- Adds optional lighting normalization for dim or uneven webcam images.

This is a MANO estimator, not a true scanner. It predicts a clean hand mesh from RGB, but it will not capture skin texture or exact photogrammetry-level details.

## Run

From this folder:

```powershell
& "..\mobilehand\.venv\Scripts\python.exe" app.py
```

Or from `../mobilehand/code` use the same venv path adjusted to this file.

## GUI Usage

1. Choose dataset weights: `freihand` is usually better for webcam-like RGB.
2. Choose camera index, usually `0`.
3. Press `Start`.
4. Use `Hand crop mode = auto` for best detection. If it jumps, try `center` and keep your hand inside the square.
5. Adjust `Fallback crop size` live if the hand is too small or clipped.
6. Keep `Normalize lighting` on unless the image looks washed out.
7. Adjust `Smoothing` if the hand jitters.
8. Press `Capture OBJ + JSON`.

`auto` mode works even without MediaPipe installed: it falls back to motion and skin-color tracking. Installing MediaPipe later improves the first ROI stage, but the app does not require it.

Captures are written to `captures/` by default.

## Smoke Test

```powershell
& "..\mobilehand\.venv\Scripts\python.exe" app.py --sample-smoke
```

This loads the model, runs one bundled sample image through inference, and writes an OBJ/JSON capture without opening the GUI.

## Artifact Audit

```powershell
& "..\mobilehand\.venv\Scripts\python.exe" app.py --audit-captures --capture-dir captures
```

This scans exported OBJ files and reports obvious mesh problems.
