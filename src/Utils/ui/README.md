# Utils/ui

Shared menu helpers for screen code.

## Current contents

```text
Utils/ui/
|-- MenuHelper.hpp
`-- MenuHelper.cpp
```

## What MenuHelper does today

- render a vertical menu from `std::span<const MenuItem>`
- handle keyboard navigation and activation
- perform mouse hover and click hit-testing
- return multi-value mouse results through `MenuHitResult`

## Style rules

- keep helpers stateless
- prefer `std::span` for caller-owned item lists
- keep layout knobs grouped inside `MenuLayout`
- use structs for multi-value returns instead of sentinel integers
- keep this folder concrete; do not turn menus into a widget framework unless the real codebase grows into needing one
