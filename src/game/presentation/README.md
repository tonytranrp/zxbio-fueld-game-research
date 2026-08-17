# game/presentation

Game-specific presentation helpers live here. These are not full screens, but
they help screens render or coordinate UI effects.

## Current folders

```text
game/presentation/
|-- effects/  shader backdrops, blur, screen visual helpers
|-- idle/     idle detection utility
|-- widgets/  shared menu widgets
`-- world/   2D farm tile-grid renderer
```

## How to use it

Put code here when multiple game screens need the same visual behavior or UI
helper and the code is still Fuel Farm specific.

If the helper becomes reusable across games, move it into `engine/ui/` or
`engine/graphics/`.

## Coding standards

- Keep presentation helpers screen-agnostic when possible.
- Do not own global services here; receive dependencies from screens.
- Layout constants can live near the helper when the helper owns the layout.
- Keep game domain language out of engine folders unless the concept is truly
  reusable.
