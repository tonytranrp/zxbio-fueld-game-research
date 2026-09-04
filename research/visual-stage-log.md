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
