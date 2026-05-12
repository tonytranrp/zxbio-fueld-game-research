# game

Fuel Farm specific code lives here: concrete screens, presentation helpers,
model asset registration, future gameplay contracts, and domain data.

## Folder map

```text
game/
|-- screens/       loading, main menu, pause popup, idle, video, dev tools
|-- presentation/  screen effects, widgets, idle trigger
|-- models/        game model assets and runtime instances
|-- gameplay/      future economy/ecology/season/tech/save contracts
`-- data/          future crop, fuel, tech, policy, and balance tables
```

## Game boundary

Game code can use `biofuel::engine::runtime::Runtime` and compose engine UI,
render, audio, video, model, animation, vision, and procedural modules. Reusable
detectors, math, caches, service backends, or resource owners should live in
`engine/`.

## How to use it

Add player-facing features as screens, presentation helpers, gameplay systems,
or data tables based on ownership:

```cpp
Runtime::screen().queueReplace<biofuel::game::screens::MainMenuScreen>();
```

If a helper does not mention Fuel Farm and could be reused by another project,
it probably belongs in `engine/`.

## Coding standards

- Screens own workflow and presentation, not reusable engines.
- Shared screen visuals belong in `game/presentation/`.
- Fuel Farm rules and research-derived values belong in `game/data/` or
  `game/gameplay/`.
- Each concrete screen folder needs a `README.md` and a typed screen module.
