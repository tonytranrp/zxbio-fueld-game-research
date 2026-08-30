# solar_array

A farm-scale ground-mounted solar panel prop for `src/World/src/solar.rs`'s clean-power system --
the counterpart `hydrogen.rs`'s own doc comment named as unbuilt future work. See `solar.rs`'s own
doc comment for the cited real-research (IPCC AR5 WG3 Annex III 2014, Hsu et al. 2012/NREL, NREL 2024
Annual Technology Baseline, LBNL "Utility-Scale Solar 2024") behind its gameplay constants -- in
particular the deliberately-not-smoothed-over finding that a single unstored farm-scale array only
partially offsets the electrolyzer's own grid-carbon cost, not zeroes it out.

## Source

Generated with [Meshy AI](https://www.meshy.ai) text-to-3D (`meshy-6` default model), preview + refine
(PBR) pipeline, via the `openapi/v2/text-to-3d` REST API.

- Preview task: `01a052c2-5db5-7009-9b05-14f06d0804b4`
- Refine task: `01a052c5-354e-7788-b95b-f5c44af0c767`
- Prompt: "a small farm-scale ground-mounted solar panel array, three tilted photovoltaic panels on a
  metal support frame with angled legs, stylized low-poly game asset, clean industrial farm equipment
  aesthetic"
- Texture prompt (refine pass): "dark blue-black photovoltaic cells with a fine grid pattern, silver
  aluminum frame edges, subtle anti-reflective sheen, weathered galvanized steel support legs"
- Settings: `art_style: realistic`, `should_remesh: true`, `enable_pbr: true`

## License

Generated via a paid Meshy API subscription; Meshy's terms grant the subscriber commercial usage
rights to generated assets. No third-party reference asset was used as generation input (pure
text-to-3D).

## Preprocessing notes

None yet -- used as exported (no Blender cleanup pass). Not rigged/animated (a static prop). If
Blender cleanup becomes necessary (topology, UVs, or pivot/origin adjustment), record it here.
