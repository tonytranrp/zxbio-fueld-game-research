# src - Source Map

The source tree is split between reusable engine code and game-specific code.
Generated output must stay outside `src/`; use top-level `build/` or `out/build/`.

```text
src/
|-- main.cpp
|-- CMakeLists.txt
|-- engine/
|   |-- app/          application loop and startup
|   |-- core/         aliases and typed metaprogramming helpers
|   |-- runtime/      Runtime facade, typed services, generated registries
|   |-- events/       event payloads and dispatcher bridge
|   |-- graphics/     renderer, render surfaces, shaders, camera components
|   |-- animation/    generic animation runtime and keyframes
|   |-- audio/        audio assets and manager
|   |-- video/        video assets and manager
|   |-- fonts/        font helpers
|   |-- input/        input polling service
|   |-- window/       window/platform helpers
|   `-- ui/           reusable screen stack and typed render pipeline
`-- game/
    |-- screens/      loading, main menu, pause popup, idle, video
    |-- presentation/ screen effects, widgets, idle trigger
    |-- models/       game model assets and instances
    |-- gameplay/     typed future gameplay contracts
    `-- data/         future crop/tech/research balance data
```

Prefer root-relative includes from `src`, such as `engine/runtime/Runtime.hpp`
or `game/screens/main_menu/MainMenuScreen.hpp`. Runtime access goes through
`biofuel::engine::runtime::Runtime`; do not add new global facades or dumping
ground utility folders.
