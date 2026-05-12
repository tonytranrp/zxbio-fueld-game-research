# game/screens

Concrete Fuel Farm screens live here.

## Current folders

```text
game/screens/
|-- loading/
|-- main_menu/
|-- pause_popup/
|-- idle/
|-- video/
`-- dev_hand_lab/
```

## How to add a screen

Create a folder with the screen class, module header, and local README:

```text
game/screens/farm/
|-- FarmScreen.hpp
|-- FarmScreen.cpp
|-- FarmScreenModule.hpp
`-- README.md
```

The screen derives from `engine::ui::Screen`, owns only screen workflow state,
and registers a typed screen module.

```cpp
class FarmScreen final : public engine::ui::Screen {
public:
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;
};
```

## Coding standards

- Keep reusable math, services, caches, and detector logic out of screens.
- Use `queuePush`, `queueReplace`, and `queuePop` from update paths.
- Put shared effects in `game/presentation/`.
- Put reusable engine behavior in `engine/`.
- Each screen folder must have its own `README.md`.
