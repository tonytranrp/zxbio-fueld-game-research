# electrolyzer

A farm-scale hydrogen electrolyzer prop for `src/World/src/hydrogen.rs`'s
second energy pathway (2 H2O -> 2 H2 + O2), coupled to the same shared
`CarbonBudget` as the crop/fuel system. See `hydrogen.rs`'s own doc comment
for the cited real-research (DOE technical-target kWh/kg figures, IEA
Global Hydrogen Review 2024, Ember Global Electricity Review) behind its
gameplay constants -- in particular the deliberately-not-smoothed-over
finding that grid-powered electrolysis can emit MORE CO2/kg H2 than grey
(natural-gas SMR) hydrogen.

## Source

Generated with [Meshy AI](https://www.meshy.ai) text-to-3D (`meshy-6`
default model), preview + refine (PBR) pipeline, via the `openapi/v2/
text-to-3d` REST API.

- Preview task: `01a052a9-6783-7740-a880-a1642f59f13f`
- Refine task: `01a052ad-0721-7529-a864-70947ae50d11`
- Prompt: "a compact farm-scale hydrogen electrolyzer unit, industrial metal
  box with pipes, gauges, a small water tank, and an electrical hookup
  panel, stylized low-poly game asset, sci-fi-adjacent but grounded farm
  equipment aesthetic"
- Texture prompt (refine pass): "brushed metal industrial casing,
  warning-yellow accent stripes, small analog gauges, weathered but
  well-maintained farm equipment"
- Settings: `art_style: realistic`, `should_remesh: true`, `enable_pbr: true`

## License

Generated via a paid Meshy API subscription; Meshy's terms grant the
subscriber commercial usage rights to generated assets. No third-party
reference asset was used as generation input (pure text-to-3D).

## Preprocessing notes

None yet -- used as exported (no Blender cleanup pass). Not rigged/animated
(a static prop). If Blender cleanup becomes necessary (topology, UVs, or
pivot/origin adjustment), record it here.
