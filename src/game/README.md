# game

Game-specific code lives here: current screens, presentation effects, model
assets, and future gameplay contracts/data.

Game code can use `biofuel::engine::runtime::Runtime` for typed services and can
compose engine UI/render primitives, but reusable engine systems should not move
back into this tree.
