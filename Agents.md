# Agents.md — Project Plan & Status

## Current status (restored 2026-08-16)

This file previously contained disposable runtime configuration for a one-off
automated multi-agent ("OMX team") run, accidentally committed over the real
plan below on 2026-05-21. The original content has been recovered from git
history (`git show b022dc1:Agents.md`) and is preserved below for reference.

**The plan below is historical, not current.** The game's direction changed
significantly during development: it moved from the originally-planned 2D
pixel-art farm sim with a 2D-to-3D "model swap" mechanic, through an
intermediate 2D top-down prototype, to its current form — a first-person 3D
voxel world (Minecraft-style chunked terrain, walkable, Rapier physics via a
Rust FFI bridge) built on a substantial custom engine (typed service/event/
asset registries, a screen stack with crossfades, procedural terrain, audio,
video, animation systems). Mentions below of vcpkg, a 2D tile grid, the
`SwapEntity`/`RenderMode` system, or the planned file structure no longer
describe this codebase.

For accurate, current documentation, see:
- [README.md](README.md) — build instructions and coding direction
- [Notes/Source Map.md](Notes/Source%20Map.md) — source tree index
- [Notes/Architecture Decisions.md](Notes/Architecture%20Decisions.md)
- the per-module `README.md` files throughout `src/`

---

# Agents.md — Biofuel Game Development Plan (original, 2026-05-08)

> **Game:** 2D Pixel-Art Biofuel Management Sim **with 2D↔3D Model Swap**  
> **Language:** C++  
> **Engine:** Raylib (confirmed)  
> **Target:** Desktop (Windows/Linux/Mac)  
> **Status:** Research phase complete → Development planning

---

## 1. Game Concept: "Fuel Farm" (Concept A)

A 2D pixel-art farm management sim where the player manages land, crops, and processing facilities to produce biofuels while balancing profit, environmental impact, and food production.

### Visual Style: 2D Pixel → 3D Model Swap

The game is primarily **2D pixel art**, but during special moments (attacks, building upgrades, tech unlocks, cutscene triggers), the 2D sprite **swaps to a low-poly 3D model** for a brief animation, then swaps back to 2D. This gives the game a distinctive "pop" effect — like the character momentarily bursts out of the flat world.

**How it works:**
1. Normal gameplay: 2D pixel sprites on a tile grid (standard Raylib 2D rendering)
2. Trigger moment (attack, transform, special action): hide 2D sprite, spawn matching 3D model at same screen position
3. 3D model plays a short animation (attack swing, transformation, building pop-up)
4. Animation ends: fade/hide 3D model, show 2D sprite again

This requires **both** a 2D spritesheet and a low-poly 3D model for any entity that can "pop out."

**Core Loop:**
1. Choose land and crops each season
2. Build and upgrade processing facilities (ethanol plant, biodiesel reactor, digester)
3. Sell fuel to the market — prices fluctuate
4. See environmental consequences on your map
5. Research tech tree unlocks next-gen biofuels

**Why This Concept:** Best fit for a CS class project — manageable scope, clear CS concepts (2D grids, state management, turn-based logic), can be built incrementally.

---

## 2. Tech Stack (Confirmed: Raylib)

### Why Raylib Is Required (Not Optional)

The 2D↔3D model swap mechanic **requires** an engine that can render both 2D and 3D in the same scene. This eliminates SDL2 and SFML (2D-only; 3D requires raw OpenGL).

| Requirement | Raylib | SDL2 | SFML |
|-------------|--------|------|------|
| **2D+3D in same scene** | ✅ Built-in | ❌ Need raw OpenGL | ❌ Need raw OpenGL |
| **Pixel-art friendly** | ✅ Built-in nearest-neighbor | ⚠️ Manual | ⚠️ Manual |
| **3D model loading** | ✅ `.glb`, `.obj`, `.iqm` | ❌ Roll your own | ❌ Roll your own |
| **Shader support** | ✅ GLSL vertex/fragment | ⚠️ Manual GLSL | ⚠️ Manual GLSL |
| **Camera 2D + 3D** | ✅ Both built-in | ❌ | ❌ |
| **Audio** | ✅ Built-in | ✅ SDL_mixer | ✅ SFML Audio |
| **UI helpers** | ✅ raygui | ❌ None | ❌ None |

**Raylib is the only viable option** for this project's 2D↔3D hybrid visual style.

### Raylib Capabilities We'll Use

- **2D rendering**: `DrawTextureRec`, `DrawTexturePro` for pixel sprites
- **3D rendering**: `DrawModel`, `DrawModelEx` for 3D model swaps
- **Camera blending**: `Camera2D` for gameplay, `Camera3D` for 3D pop-out moments
- **Shaders**: Custom GLSL shaders for transition effects (2D→3D, 3D→2D fades)
- **Model loading**: `LoadModel` supports `.glb`/`.gltf` (animated), `.obj` (static), `.iqm` (animated)
- **Pixel art scaling**: `SetTextureFilter(TEXTURE_FILTER_POINT)` for crisp pixel sprites

### Supporting Libraries

| Library | Purpose |
|---------|---------|
| `raylib-cpp` (optional) | C++ bindings if prefer OOP style |
| `nlohmann/json` | JSON parsing for game data/research configs |
| `fmtlib` | String formatting for UI text |
| `stb_image` / `stb_truetype` | Texture loading / font rendering (raylib has built-in, but for custom needs) |

---

## 3. Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                        GAME LAYER                        │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐ │
│  │  Title / │ │  Farm    │ │  Map     │ │  Tech Tree │ │
│  │  Menus   │ │  Screen  │ │  Editor  │ │  Screen    │ │
│  └──────────┘ └──────────┘ └──────────┘ └────────────┘ │
├─────────────────────────────────────────────────────────┤
│                      SYSTEMS LAYER                       │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐ │
│  │  Render  │ │  Input   │ │  Audio   │ │  Camera    │ │
│  │  System  │ │  System  │ │  System  │ │  System    │ │
│  └──────────┘ └──────────┘ └──────────┘ └────────────┘ │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐ │
│  │ Economy  │ │ Ecology  │ │  Season  │ │  Event     │ │
│  │ System   │ │ System   │ │  System  │ │  System    │ │
│  └──────────┘ └──────────┘ └──────────┘ └────────────┘ │
│  ┌────────────────────────────────────────────────────┐  │
│  │       2D↔3D Swap System (Model Swap Renderer)      │  │
│  └────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────┤
│                      CORE LAYER                          │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐ │
│  │  ECS /   │ │  Asset   │ │  Save /  │ │  Config    │ │
│  │  Entity  │ │  Manager │ │  Load    │ │  Manager   │ │
│  └──────────┘ └──────────┘ └──────────┘ └────────────┘ │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────────────┐ │
│  │  Game    │ │  State   │ │   Research Data Tables   │ │
│  │  Loop    │ │  Machine │ │   (hardcoded structs)    │ │
│  └──────────┘ └──────────┘ └──────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Entity Model: Struct-of-Arrays (Hybrid ECS)

For a 2D pixel tile game, full ECS is overkill. Use a hybrid approach:

```cpp
// Tile-based world data
struct Tile {
    enum Type { CORN, SOYBEANS, SWITCHGRASS, FOREST, FALLOW, WATER, BUILT };
    Type type;
    int soil_health;      // 0-100
    int moisture;         // 0-100
    int fertilizer;       // 0-100
    int age;              // days since planting
    int building_id;      // -1 if none, index into buildings array
};

// Entity array — the farm grid
struct Farm {
    std::vector<Tile> tiles;        // width * height
    std::vector<Building> buildings;
    int width, height;
    int money;
    int carbon_debt;                // tons CO₂ owed
    int fuel_produced;              // total gallons
    int food_produced;              // food units
    int season;                     // 0=spring, 1=summer, 2=fall, 3=winter
    int year;
};
```

### 2D↔3D Model Swap System

Each entity that can "pop out" needs both a 2D sprite and a matching 3D model:

```cpp
// Pop-out entity definition
struct SwapEntity {
    const char* name;               // e.g. "Player", "Harvester", "Bioreactor"
    Texture2D sprite;               // 2D pixel spritesheet
    Model model_3d;                 // Low-poly 3D model (.glb or .iqm)
    Shader swap_shader;             // Transition shader (2D→3D fade)
    Animation* anims_3d;            // 3D animation set
    int anim_count;                 // Number of 3D animations
};

// Swap state machine per entity
enum class RenderMode { SPRITE_2D, TRANSITIONING_TO_3D, MODEL_3D, TRANSITIONING_TO_2D };

struct SwapState {
    RenderMode mode = RenderMode::SPRITE_2D;
    float transition_progress = 0.0f;   // 0.0 = 2D, 1.0 = 3D
    float transition_speed = 3.0f;      // speed of swap animation
    int current_anim = -1;              // which 3D animation is playing
};
```

**Swap Flow:**
1. **Normal state**: Entity rendered as 2D sprite via `DrawTextureRec()`
2. **Trigger** (attack, upgrade, cutscene): Set `RenderMode::TRANSITIONING_TO_3D`
3. **Transition**: Fade out 2D sprite while fading in 3D model at same screen position. Use `transition_progress` (0→1) to interpolate alpha
4. **3D mode**: `RenderMode::MODEL_3D` — play 3D animation via `UpdateModelAnimation()`
5. **Animation complete**: Set `RenderMode::TRANSITIONING_TO_2D`
6. **Return transition**: Fade out 3D, fade in 2D sprite. `transition_progress` (1→0)
7. **Back to normal**: `RenderMode::SPRITE_2D`

**3D Model Aesthetic**: Low-poly models with **pixel-art textures** (tiny texture maps with `TEXTURE_FILTER_POINT`). This makes the 3D models feel like they belong in the 2D pixel world — same color palette, same chunky style, just with depth.

---

## 4. Core Systems

### 4.1 Rendering System (Raylib)

- **Tile-based renderer**: Draw 16×16 or 32×32 pixel tiles from spritesheet
- **Camera**: follows player, clamped to map bounds
- **UI layer**: raylib's `Gui*` functions or custom immediate-mode UI
- **Pixel-art scaling**: nearest-neighbor filtering (set `SETTEXTUREFILTER` to `TEXTURE_FILTER_POINT`)
- **Parallax background**: simple sky + horizon layers
- **2D↔3D swap rendering**: When `SwapState.mode` transitions to 3D, switch from `BeginMode2D()` to `BeginMode3D()` for the pop-out entity, then back to `BeginMode2D()` for the rest of the scene

### 4.2 Season / Turn System

- Each "year" = 4 turns (spring, summer, fall, winter)
- Player actions per turn: plant, harvest, build, research, buy/sell
- End-turn → AI processes: growth, weather, market price updates, events

### 4.3 Economy System

- Market prices fluctuate each turn using a seeded random walk
- Prices tracked: corn, soybeans, ethanol, biodiesel, carbon credits, fertilizer
- Player earns: selling fuel + crops
- Player spends: seeds, fertilizer, building construction, research

### 4.4 Ecology System

- **Carbon ledger**: positive when converting land, negative when producing fuel. Net zero = "green" status
- **Soil health**: depletes with monocropping, restores with legumes/fallow
- **Biodiversity score**: affected by land use choices
- **Water quality**: fertilizer runoff affects nearby water tiles

### 4.5 Tech Tree System

- 4-tiers of research: Corn/Soy → Cellulosic → Algae → Synthetic
- Each tech unlocks new crops, buildings, or efficiency upgrades
- Research costs money and takes N turns to complete

### 4.6 Event System

- Random policy events (mandate changes, tax credits, oil price shocks)
- Weather events (drought, flood, optimal)
- Global market events (food price spikes, trade wars)

---

## 5. Development Phases

### Phase 0: Project Setup (Week 1)
- [ ] Set up C++ build system (CMake + vcpkg or Conan)
- [ ] Integrate Raylib + dependencies
- [ ] Create window, game loop, frame timing
- [ ] Asset pipeline: spritesheet generation or placeholder rectangles
- [ ] Define game state data structures

### Phase 1: MVP Tile Map (Week 2)
- [ ] Generate grid (10×10 minimum)
- [ ] Render colored rectangles as placeholder tiles
- [ ] Mouse click: select tile, change tile type
- [ ] Keyboard navigation, camera pan
- [ ] Toolbar: choose what to paint (corn, forest, soy, etc.)

### Phase 2: Turn Logic (Week 3)
- [ ] Implement season cycle (spring → summer → fall → winter)
- [ ] Crop growth over turns based on soil/water
- [ ] Harvest mechanic: collect yield, store in inventory
- [ ] Simple economy: buy seeds, sell crops
- [ ] UI panels: inventory, money display

### Phase 3: Processing & Fuel (Week 4)
- [ ] Building placement: ethanol plant, biodiesel reactor
- [ ] Convert crops → fuel each turn
- [ ] Fuel market: price fluctuates per turn
- [ ] Carbon debt tracking: land conversion → carbon released → pay down with fuel production

### Phase 4: 2D↔3D Model Swap (Week 5)
- [ ] Create placeholder low-poly 3D models for 1-2 entities (player, harvester)
- [ ] Implement `SwapEntity` data structure and `SwapState` state machine
- [ ] 2D→3D transition: hide sprite, spawn 3D model at same position, fade in
- [ ] 3D→2D transition: fade out 3D model, show sprite again
- [ ] Test swap triggers: attack animation, building upgrade, special action
- [ ] Pixel-art textures on 3D models (`TEXTURE_FILTER_POINT` on model materials)

### Phase 5: Tech Tree & Events (Week 6)
- [ ] Tech tree UI: 4 tiers, 4-6 techs each
- [ ] Research queue: select tech, wait N turns
- [ ] Event system: random events trigger each year
- [ ] Tooltips on everything

### Phase 6: Polish & Data (Week 7)
- [ ] Real pixel-art sprites (replace colored rectangles)
- [ ] More 3D models for all swap entities
- [ ] Sound effects + background music
- [ ] Balance numbers against real data from research docs
- [ ] Win/lose conditions: meet fuel target in N years
- [ ] Title screen, game over screen

### Phase 7: Extra Credit (If Time)
- [ ] Weather system visualization
- [ ] Multiple maps/biomes
- [ ] Graphs: carbon balance over time, profit/loss chart
- [ ] Policy decision popups
- [ ] Save/load game

---

## 6. Research → Game Data Mapping

Each research document feeds specific game systems:

| Document | Maps To | Key Data Used |
|----------|---------|---------------|
| **01-biofuel-fundamentals.md** | Tech tree descriptions, fuel type definitions | 4 generations, fuel categories |
| **02-production-processes.md** | Building mechanics, conversion formulas | Fermentation → ethanol, transesterification → biodiesel, anaerobic digestion → biogas |
| **03-feedstock-and-crops.md** | Crop types, yield tables, fertilizer/water costs | gal/acre yields, input requirements per crop |
| **04-energy-and-emissions.md** | Carbon scoring, energy balance | BTU/gal, FER, carbon debt payback times, GHG reduction % |
| **05-economics-and-policy.md** | Market prices, policy events, win conditions | RFS mandates, tax credits, market sizes |
| **06-game-design-ideas.md** | Overall game design, MVP scope, mechanics | Core loop definition, tech tree structure, carbon debt mechanic |

**Convenience data table** (directly from the research, for your game balance):

```cpp
// From 03-feedstock-and-crops.md + 04-energy-and-emissions.md
struct CropData {
    const char* name;
    int yield_gallons_per_acre;  // fuel yield
    int water_need;              // 1-5 scale
    int fertilizer_need;         // 1-5 scale
    int land_impact;             // 1-5 scale
    int carbon_score;            // 1-10
    int energy_per_gallon_btu;   // from 04
};

constexpr CropData CROPS[] = {
    {"Corn (Ethanol)",    400, 4, 4, 4, 3,  76330},
    {"Sugarcane (Ethanol)", 590, 5, 4, 3, 5,  76330},
    {"Soybean (Biodiesel)", 48, 3, 2, 4, 4, 118300},
    {"Switchgrass (Cel.)", 300, 2, 1, 1, 9,  76330},
    {"Algae (Biodiesel)", 5000, 2, 1, 1, 10, 118300},
};
```

---

## 7. File Structure

```
zxbio-fueld-game-research/
├── Agents.md                  ← THIS FILE: development master plan
├── README.md                  ← repo overview
│
├── Research/                  ← research documents (read-only reference)
│   ├── 01-biofuel-fundamentals.md
│   ├── 02-production-processes.md
│   ├── 03-feedstock-and-crops.md
│   ├── 04-energy-and-emissions.md
│   ├── 05-economics-and-policy.md
│   └── plans/
│       └── 06-game-design-ideas.md   ← game design concepts
│
└── src/                       ← game source code (future)
    ├── CMakeLists.txt
    ├── main.cpp
    ├── core/
    │   ├── game.h             ← Game struct, enums, constants
    │   ├── game.cpp
    │   ├── state_machine.h
    │   └── state_machine.cpp
    ├── systems/
    │   ├── render_system.h
    │   ├── swap_system.h      ← 2D↔3D model swap logic
    │   ├── swap_system.cpp
    │   ├── economy_system.h
    │   ├── ecology_system.h
    │   ├── season_system.h
    │   ├── tech_tree_system.h
    │   └── event_system.h
    ├── ui/
    │   ├── toolbar.h
    │   ├── info_panel.h
    │   ├── tech_tree_ui.h
    │   └── market_panel.h
    ├── data/
    │   ├── crop_data.h        ← hardcoded research data tables
    │   ├── tech_data.h
    │   └── event_data.h
    └── assets/
        ├── sprites/            ← 2D pixel spritesheets (.png)
        ├── models/             ← 3D low-poly models (.glb/.iqm)
        ├── shaders/            ← GLSL transition shaders
        ├── audio/              ← sound effects + music
        └── fonts/              ← pixel fonts
```

---

## 8. Key Technical Decisions

### 8.1 Build System

**CMake** is the standard for C++ projects. Use it from day one:
```cmake
cmake_minimum_required(VERSION 3.20)
project(BiofuelGame CXX)
find_package(raylib REQUIRED)
find_package(nlohmann_json REQUIRED)
add_executable(biofuel src/main.cpp ...)
target_link_libraries(biofuel raylib nlohmann_json::nlohmann_json)
```

Use **vcpkg** for dependency management:
```
vcpkg install raylib nlohmann-json fmt
```

### 8.2 Game Loop

Fixed-timestep loop with accumulator pattern:
```cpp
constexpr double TICK_RATE = 1.0 / 60.0;
double accumulator = 0.0;

while (!WindowShouldClose()) {
    double dt = GetFrameTime();
    accumulator += dt;
    
    while (accumulator >= TICK_RATE) {
        Update();           // game logic at fixed rate
        accumulator -= TICK_RATE;
    }
    
    Draw();                 // rendering at display rate
}
```

### 8.3 Tile Coordinates

- World space: tile `(tx, ty)` where `tx = pixel_x / TILE_SIZE`
- Screen space: `screen_x = (tx * TILE_SIZE) - camera.x`
- Use `Vector2` from Raylib for camera and mouse positions
- Mouse-to-tile: `tx = (GetMouseX() + camera.x) / TILE_SIZE`

### 8.4 Data-Driven Design

Hardcode initial game data as C++ constexpr tables (from research docs). Later, optionally move to JSON configs.

---

## 9. Engine Decision Matrix

| Requirement | Raylib | SDL2 | SFML |
|-------------|--------|------|------|
| C++ compatible | ✅ C99/C++ | ✅ C/C++ | ✅ Native C++ |
| 2D pixel-art friendly | ✅ Built-in nearest-neighbor | ✅ Manual | ✅ Manual |
| **2D+3D in same scene** | ✅ **Built-in** | ❌ Need raw OpenGL | ❌ Need raw OpenGL |
| **3D model loading** | ✅ **`.glb`, `.obj`, `.iqm`** | ❌ Roll your own | ❌ Roll your own |
| **Camera 2D + 3D** | ✅ **Both built-in** | ❌ | ❌ |
| **Shader support** | ✅ GLSL vertex/fragment | ⚠️ Manual GLSL | ⚠️ Manual GLSL |
| Learning curve | ⭐ Easiest | Medium | Medium |
| Build setup | ⭐ Single file | Multi-file SDL_image, SDL_ttf, etc. | Multi-module |
| Documentation | ⭐ Excellent | Good | Good |
| Community size | Growing | ⭐ Massive | Large |
| Audio | ✅ Built-in | ✅ SDL_mixer | ✅ SFML Audio |
| UI helpers | ✅ raygui | ❌ None | ❌ None |

**Verdict: Raylib is the only viable option** for this project's 2D↔3D hybrid visual style. SDL2 and SFML are 2D-only and would require raw OpenGL for any 3D rendering.

---

## 10. Immediate Next Steps

1. **Set up Raylib** — CMake + vcpkg, compile "Hello Triangle"
2. **Draw a tile grid** — colored rectangles on screen, click to change colors
3. **Test 2D↔3D swap** — render a 2D sprite, swap to a simple 3D cube at the same position
4. **Copy game data** — port crop/tech tables from `Research/` into C++ structs
5. **Build MVP** — 10×10 grid, plant/harvest, buy/sell

---

*Created: May 2026 — Biofuel Game Research → Implementation transition*
