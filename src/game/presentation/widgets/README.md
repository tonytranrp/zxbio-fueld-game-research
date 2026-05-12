# game/presentation/widgets

Shared UI helpers for screen code.

## Current contents

```text
game/presentation/widgets/
|-- MenuHelper.hpp
|-- MenuHelper.cpp
`-- README.md
```

## What MenuHelper provides

- Render a vertical or horizontal carousel menu from `std::span<const MenuItem>`
- Handle keyboard navigation and activation
- Perform mouse hover and click hit-testing
- Return multi-value mouse results through `MenuHitResult`
- **`InputCooldown`** — reusable debounce timer struct for input cooldowns between screens

### InputCooldown

```cpp
game::presentation::widgets::InputCooldown m_cooldown;
// In onEnter():
m_cooldown.reset(0.12f);
// In onUpdate():
m_cooldown.update(dt);
if (m_cooldown.ready()) { /* handle input */ }
```

A lightweight, `noexcept` struct with configurable delay. Both `MainMenuScreen` and `PausePopupScreen` use identical cooldown patterns — this struct unifies them.

## Style rules

- Keep helpers stateless
- Prefer `std::span` for caller-owned item lists
- Keep layout knobs grouped inside `MenuLayout`
- Use structs for multi-value returns instead of sentinel integers
- Keep this folder concrete; do not turn menus into a widget framework unless the real codebase grows into needing one
