# corn_plant

First grow-able crop model for the biofuel farm (`src/World/src/crop.rs`'s
Liebig's-law growth system). Corn/maize was chosen as the first crop because
it's a real first-generation biofuel (ethanol) feedstock -- a later pass may
add a next-generation cellulosic crop (e.g. switchgrass) as a narrative/tech
progression from first- to advanced-generation biofuels.

## Source

Generated with [Meshy AI](https://www.meshy.ai) text-to-3D (`meshy-6`
default model), preview + refine (PBR, 2K textures) pipeline, via the
`openapi/v2/text-to-3d` REST API.

- Preview task: `01a051ac-eca8-7510-ae3d-3cb28ac7464e`
- Refine task: `01a051af-b433-73be-b997-c0e353a953ee`
- Prompt: "A single corn stalk plant with broad leaves and one ripe ear of
  corn, game environment prop, clean stylized low-poly style, vibrant green
  stalk with golden-yellow corn kernels, isolated single plant, no
  background, no pot, no ground"
- Settings: `target_polycount: 2000`, `topology: quad`, `should_remesh: true`,
  `enable_pbr: true`, `texture_resolution: 2k`

## License

Generated via a paid Meshy API subscription; Meshy's terms grant the
subscriber commercial usage rights to generated assets. No third-party
reference asset was used as generation input (pure text-to-3D).

## Preprocessing notes

None yet -- used as exported (no Blender cleanup pass). Not rigged/animated
(a static prop). If Blender cleanup becomes necessary (topology, UVs, or
pivot/origin adjustment), record it here.
