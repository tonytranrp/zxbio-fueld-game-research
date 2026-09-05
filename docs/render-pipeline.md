# Render pipeline (goals.md goal 48 — the actual multi-pass shape, written down)

The frame, in execution order, as of the Stage 1–3 + Group L/M work (2026-09-04). Every pass
lists its owner and its kill switch (goal 52's isolation discipline).

```
1. Scene pass                owner: TerrainRenderer::render
   target: PostProcessor's RGBA16F offscreen scene target when the post chain is live
           (RenderContext::Impl::sceneColor; the terrain/sky PSOs are created against its
           format -- PostProcessor is constructed BEFORE TerrainRenderer on purpose),
           else the swap chain directly (--no-post)
   depth:  the swap chain's depth buffer, cleared here
   1a. terrain + baked-in trees   one PSO, per-chunk draws, frustum-culled
                                  (materials/AO/fog/water/sway all live in this pair of shaders)
   1b. analytic sky               fullscreen triangle at far depth, LESS_EQUAL, no depth write
                                  (draws only where no terrain covered; --no-sky)
2. Bloom                     owner: PostProcessor (DiligentFX Bloom; --no-bloom)
   input: scene SRV; output: DiligentFX's own texture, already scene+bloom composited
   (PostFXContext is PrepareResources-only after one warm-up Execute -- see post_process.cpp)
3. Tonemap composite         owner: PostProcessor (--no-tonemap for raw clamp)
   fullscreen soft-knee (identity below 0.75, tanh shoulder) into the swap chain;
   swap-chain depth stays bound (disabled) so the overlay's PSO contract holds
4. Debug overlay             owner: DebugOverlay (ImGui), draws over the composite
5. Readback hooks            --verify-frame / VOXEL_DUMP_FRAME / --dump-every / F2 -- all read
                             the swap chain AFTER the overlay, BEFORE Present
```

Shared state worth knowing when adding a pass:
- `sky_common.fxh` is the single palette for sky, fog, and water reflection — a new pass that
  needs "the sky's color" includes it rather than re-tuning constants.
- The scene target registers on `RenderContext::Impl` (no public-API plumbing); a second
  offscreen consumer should follow the same pattern.
- Animation phase comes from the renderer's own steady clock (FrameConstants.time / fog w lane).

## Goal 41 — SSAO / G-buffer go/no-go: **NO-GO for this pass** (the gate's written decision)

Weighed as the goal demands:
- What SSAO adds here: contact darkening beyond the baked voxel AO — mainly in dense foliage
  and at object-ground contacts.
- What it costs at this pin (verified in source, not assumed): a world-space normal render
  target (the first genuinely new G-buffer attachment), depth-as-SRV (our depth is the swap
  chain's, not SRV-capable — so a real depth-target refactor), PostFXContext::Execute per frame
  (four full-res helper passes + camera constants — the machinery Stage 2 deliberately avoided),
  and SSAO's own passes on top ("the most expensive addition in this arc" per the research).
- What the cheap alternatives already delivered, checked by viewed captures: baked per-vertex AO
  darkens exactly the creases/pits SSAO would (Stage 1), and the scene's visual ceiling right
  now is material/texture identity, not contact shadow fidelity — flat-shaded primitives don't
  show SSAO's subtlety.

Decision: defer the entire Stage 4 group (42–47). Reopen when EITHER a texture/detail pass makes
contact shadows legible, OR the deferred-context/GPU-driven work (progress.md's deferred list)
introduces a real G-buffer for its own reasons — at that point wire PostFXContext::Execute with
real depth + zeroed motion vectors per goal 43 and re-run this gate with numbers.
