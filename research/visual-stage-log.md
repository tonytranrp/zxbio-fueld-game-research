# Visual-richness stage log (docs/goals.md groups B–F, L, M)

Running before/after record. Every claim below was checked by actually viewing the referenced
capture, per the standing methodology note in `docs/goals.md` — never by the verify fraction alone.
Reference captures live in `research/captures/`; the full working set (all angles, numbered
sequences) stays in the session scratchpad.

## Baseline (goal 8, 2026-09-04, post-winding-fix, pre-Stage-1)

Three angles viewed (`captures/baseline_default.png`, ground-level walk view, straight-down):
- Terrain rendered as near-uniform pale gray (every solid voxel is Stone; Dirt exists in the
  palette but generation never assigns it). Flat white Lambert + 0.25 ambient floor.
- Voxel terracing reads as an artifact on every slope; no AO, so creases/valleys have no depth.
- Straight-down view shows the loaded region's water EDGE as a hard rectangle against sky — the
  "geometry just stops" pop that distance fog (goal 33) exists to hide.

## Stage 1 — AO, hemisphere ambient, sun color, albedo variation, richer palette (goals 10–20)

Changes: baked per-vertex concavity AO (`research/baked-ao-design.md`; the former pad byte of the
12B vertex, stride unchanged), two-color hemisphere ambient replacing the flat floor, warm golden
sun color, two-octave world-XZ value-noise albedo mottling (±10%), richer base palette.

After-captures viewed (`captures/stage1_default.png`, `captures/stage1_ground.png`), same angles
as baseline:
- The scene went from clinical gray to warm, sun-lit tan/sand tones; up-facing surfaces read
  warmer/brighter than side/down-facing ones (hemisphere term visibly working).
- AO darkens exactly the concave features: terrace inside-corners, valley creases, pit corners —
  the terracing now reads as sculpted sediment layers with depth rather than a flat artifact.
- Mottling is visible as gentle dappling at two scales, no visible noise-grid.
- Honest gap: with every solid voxel still Stone, the warm light makes mountains read as "sand
  dunes"; material identity (grass tops, shore sand — Group M / goal 81) is the missing half of
  "colorful", not more lighting.

Numbers:
- Tests 70/70 → **71/71** (new `[ao]` case asserts the design table's exact levels on
  hand-constructed flat/crease/pit/ridge geometry).
- Verify fraction unchanged: 39.4% Vulkan at the standard pose (goal 17: the 25% threshold stays
  meaningful — the metric counts differing pixels, and lighting changes don't shift it).
- Goal 18 shader-cost A/B, measured (not assumed), same 700-frame run, settled scene, RTX 4070
  laptop, debug build: old PS math 164.9 fps ↔ Stage-1 PS math 165.1 fps — the added ALU
  (AO multiply + hemisphere lerp + 8 sin-hash noise samples) is below run-to-run noise. The PS
  A/B was done by swapping shader SOURCE at runtime (same varyings both sides), no rebuild.

## Stage 2 — Bloom & tone mapping (goals 21–27)

Wiring reality vs the research: at the pinned DiligentFX commit, `Bloom::Execute` gates on
`PostFXContext::IsPSOsReady()`, and that flag is set ONLY inside `PostFXContext::Execute` — which
demands depth/motion-vector/camera inputs Bloom never consumes and runs four full-res helper
passes per call. Found empirically (bloom stayed PENDING forever; the first A/B diff measured
bloom's contribution at literally zero once overlay text was masked). Resolution: ONE warm-up
`PostFXContext::Execute` at startup with 2×2 dummy inputs flips the sticky readiness flag; per
frame only `PrepareResources` runs. Scene now renders into an RGBA16F offscreen target
(terrain PSO format follows it; construction order PostProcessor→TerrainRenderer is load-bearing);
composite = soft-knee tonemap (identity below 0.75, tanh shoulder above — ACES was tried first
and VISIBLY washed out mids ~30% in the viewed capture; the knee curve preserves the authored
look and only rolls off bloom overshoot).

Tuning by viewed captures + masked pixel-diffs (`stage2_default.png`):
- First tune (Threshold 0.60): whole-frame haze, mean +8% everywhere — the SKY (max channel ~0.9)
  is brighter than any terrain (~0.8), so a low threshold blooms 77% of the frame. Re-tuned to
  0.80/0.12: soft silhouette glow only (max delta 11/255), no clipped whites. Stage 3's water
  sun-glint (first real >1.0 emitter) is the intended true bloom source (goal 32 re-tunes).
- Goal 26's predicted metric break happened EXACTLY: bloom's sky gradient made 97.7% of pixels
  differ bytewise from the verify reference. Fixed by tolerance comparison (>16/channel — bloom
  deltas ≤11, terrain-vs-sky 50+): verify returns 39.4% Vulkan / 39.2% D3D12, same as
  pre-post-processing, restoring the metric's "terrain visible" meaning.
- Goal 25 cost, measured settled at 700 frames: 165.0 fps with the full chain ↔ 165.0 fps
  --no-post (worst-frame 7.2 vs 7.3 ms) — unmeasurable at this scene scale.
- Goal 27 combined view: warm Stage-1 terrain + subtle Stage-2 silhouette glow reads as
  atmosphere, not effect-soup; the remaining flatness is material identity (Group M), not light.
