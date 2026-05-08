# Biofuel Game Design Ideas

## What Makes a Good Biofuel Game?

A biofuel game should teach players about the **complex trade-offs** involved
in renewable energy — it's not just "biofuel = good." The most interesting games
emerge from tough decisions:

- Grow food or fuel?
- Cut down forest or use marginal land?
- Invest in cheap corn ethanol now or expensive algae R&D?
- Maximize short-term profit or long-term sustainability?
- What happens when global food prices spike because of your biofuel choices?

---

## Existing Biofuel Games (For Reference)

### Fields of Fuel (GLBRC / University of Wisconsin)

A web-based computer game where players act as farmers growing crops for
bioenergy while managing ecosystem services. Players learn about:

- Ecological and economic aspects of sustainability
- Short-term vs. long-term dynamics
- Local and global impacts of individual management decisions

Play at: http://fieldsoffuel.org/

**Mechanics:** Turn-based farming simulation, crop selection, ecosystem
score tracking, graphs showing environmental and economic outcomes.

### Bioenergy Farm Game (Wisconsin Energy Institute)

Board game / classroom activity. Teams manage farms and compete to
produce bioenergy profitably while minimizing environmental impact.

### IRENA Bioenergy Simulator

Policy-level simulator showing how bioenergy can help meet climate goals.
Online tool at IRENA website.

---

## Game Concept Ideas

### Concept A: "Fuel Farm" — Farm Management Sim

**Genre:** Strategy / management sim (think Stardew Valley meets Factorio)

**Core Loop:**
1. Choose land and crops each season
2. Build and upgrade processing facilities (ethanol plant, biodiesel reactor, anaerobic digester)
3. Sell fuel to the market — prices fluctuate based on global events
4. See environmental consequences unfold on your map
5. Research tech tree unlocks new generations of biofuel

**Key Mechanics:**
- **Land use grid** — each tile has soil quality, water access, existing ecosystem value
- **Crop rotation** — continuous corn depletes soil; legumes restore nitrogen
- **Carbon ledger** — track your lifetime carbon balance; land conversion creates "carbon debt"
- **Market prices** — corn price, oil price, carbon credit price all fluctuate
- **Government policy events** — random RFS mandate changes, tax credit expirations
- **Weather** — droughts reduce yields, floods delay planting

**Tech Tree Tiers:**
```
Tier 1: Corn ethanol, soybean biodiesel (cheap, easy, low efficiency)
  → Tier 2: Cellulosic ethanol, renewable diesel (more expensive, uses waste)
    → Tier 3: Algae bioreactors (high research cost, massive yield)
      → Tier 4: Synthetic biology, carbon-negative fuels
```

**Win Condition:** Meet renewable fuel targets over 20-30 years while
maintaining soil health, water quality, and biodiversity.

---

### Concept B: "Carbon Rush" — Tycoon/Resource Management

**Genre:** Economic strategy / tycoon game

**Core Loop:**
1. Start with limited capital, buy or lease land
2. Build refinery infrastructure
3. Secure feedstock contracts with farmers
4. Navigate volatile commodity markets
5. Compete with AI players and fossil fuel companies
6. Lobby for favorable government policies

**Key Mechanics:**
- **Supply chain management** — trucking feedstock, pipeline contracts, storage
- **Commodity futures market** — hedge against price volatility
- **R&D investment** — patent new technologies, license from universities
- **Political influence** — campaign contributions affect RFS mandates and tax credits
- **Carbon credit trading** — LCFS credits as a revenue stream
- **Competitive multiplayer** — players compete for feedstock, market share

**Events System (Random + Triggered):**
- "Oil price crashes to $30/barrel — biofuel demand plummets"
- "EPA doubles cellulosic biofuel mandate — can you meet it?"
- "Drought in Midwest — corn prices spike 200%"
- "EU bans palm oil biodiesel imports — opportunity for US soybean biodiesel?"
- "Breakthrough! Algae harvesting cost drops 60% — do you pivot?"

---

### Concept C: "Biorefinery" — Factory Builder / Process Optimization

**Genre:** Factory automation (think Factorio, Satisfactory)

**Core Loop:**
1. Design and build biofuel production lines
2. Optimize feedstock → fuel conversion efficiency
3. Manage byproduct and waste streams
4. Scale from pilot plant to industrial operation
5. Meet quality standards (ASTM D6751)

**Key Mechanics:**
- **Process design** — lay out reactors, distillation columns, centrifuges
- **Flow optimization** — pipes, pumps, heat exchangers, mass balance
- **Chemical management** — methanol, NaOH catalyst, enzymes, yeast strains
- **Quality control** — test fuel batches, reject off-spec product
- **Waste treatment** — glycerin purification, stillage management, wastewater
- **Energy integration** — capture waste heat, use biogas for plant power

**Real Chemistry You Could Model:**
- Transesterification: oil + methanol + NaOH → biodiesel + glycerin
- Fermentation: C₆H₁₂O₆ → 2 C₂H₅OH + 2 CO₂
- Saponification (failure mode): too much NaOH + water in oil → soap instead of fuel!
- Distillation: boil ethanol at 78.37°C, separate from water
- Anaerobic digestion: organic waste → CH₄ + CO₂

**Why This Works:** The actual chemistry is straightforward enough to model.
The fun comes from scaling it up without things going wrong.

---

### Concept D: "Green Empire" — 4X / Civilization-Style

**Genre:** Turn-based strategy / 4X (explore, expand, exploit, exterminate)

**Core Loop:**
1. Start with a small country/region
2. Research biofuel technologies across a deep tech tree
3. Expand biofuel production across your territory
4. Trade biofuels on the global market
5. Compete or cooperate with neighboring regions
6. Deal with climate disasters, food crises, oil price shocks

**Tech Tree Structure (4 Eras):**

```
ERA 1: FERMENTATION AGE
- Corn ethanol
- Sugarcane ethanol
- Basic biodiesel
- Crop rotation

ERA 2: ADVANCED CONVERSION
- Cellulosic ethanol
- Renewable diesel
- Anaerobic digestion (biogas)
- Waste-to-fuel
- Precision agriculture

ERA 3: ALGAL REVOLUTION
- Open pond algae
- Photobioreactors
- Algae-to-jet-fuel (SAF)
- Carbon capture integration

ERA 4: SYNTHETIC BIOLOGY
- Engineered microbes
- Direct solar-to-fuel
- Carbon-negative systems
- Closed-loop biorefineries
```

**Each Technology Has Trade-Offs:**

| Tech | Cost | Efficiency | Carbon | Land | Food Impact |
|------|------|-----------|--------|------|-------------|
| Corn ethanol | $ | ★★ | ★★ | Bad | Bad |
| Sugarcane ethanol | $$ | ★★★ | ★★★ | Bad | Bad |
| Cellulosic ethanol | $$$ | ★★★ | ★★★★★ | Good | Good |
| Biodiesel (soy) | $ | ★★★ | ★★★ | Bad | Medium |
| Biodiesel (algae) | $$$$$ | ★★★★★ | ★★★★★ | Great | Great |
| Renewable diesel | $$$$ | ★★★★★ | ★★★★ | Medium | Good |
| Biogas from waste | $$ | ★★ | ★★★★★ | Great | Great |

---

### Concept E: "Energy Policy Simulator" — Political Strategy

**Genre:** Political sim / decision-making game (think Papers Please, Reigns)

**Core Loop:**
1. You're the Energy Secretary / EPA Administrator
2. Each turn: a policy decision appears (swipe left/right style)
3. Decisions affect: fuel prices, food prices, emissions, employment, political support
4. Juggling: farmers, oil companies, environmentalists, consumers, foreign governments
5. Random crises: oil embargo, food shortage, climate disaster, election year

**Example Decisions:**
- "Corn farmers are lobbying to increase the ethanol mandate. Raise it?"
- "California wants to ban B100 that uses palm oil from deforested land. Support?"
- "An algae startup claims breakthrough. Give them a $500M loan guarantee?"
- "Gas prices hit $5/gallon. Suspend the ethanol mandate to lower food prices?"
- "The EU threatens tariffs on US biodiesel exports unless you match their sustainability standards. Concede?"

---

## Recommended Approach for a CS Class Project

Given it's a computer science class project, here's what I'd suggest:

### Best Fit: Concept A (Fuel Farm) — Why?

1. **Scope is manageable** — can start simple (grid-based farm) and add features
2. **Clear CS concepts** — 2D grid, state management, turn-based logic,
   data structures for inventory/resources, UI for charts/graphs
3. **Visually interesting** — maps, crop animations, color-coded tiles
4. **Educational** — clearly demonstrates the real trade-offs
5. **Can be built incrementally** — start with just corn vs. soybeans,
   add complexity later

### Tech Stack Suggestions

| Option | Pros | Cons |
|--------|------|------|
| **Godot + GDScript** | Free, lightweight, great for 2D, easy to learn | GDScript is niche |
| **Python + Pygame** | You know Python, fast to prototype | Performance for complex games |
| **JavaScript + Canvas / Phaser** | Runs in browser, easy to share | JS ecosystem complexity |
| **Unity + C#** | Professional, massive resources | Heavy, steep learning |

### MVP Scope (What to Build First)

1. A grid-based map showing farmland (10x10 tiles minimum)
2. Each tile can be: corn, soybeans, switchgrass, forest, or fallow
3. Turn-based: plant in spring, harvest in fall, process into fuel
4. Simple economy: sell ethanol/biodiesel, buy fertilizer/seeds
5. One environmental metric: carbon balance or soil health
6. Win/lose condition: meet fuel target in X years without degrading land

### Features to Add for Extra Credit

- Tech tree (unlock cellulosic ethanol, algae)
- Weather system (random drought/flood events)
- Multiple environmental metrics (carbon, water, biodiversity)
- Market price simulation
- Policy events
- Visual comparison graphs

---

## Game Mechanics Reference Table

Real-world numbers you can use directly in game balancing:

| Crop | Yield (gal fuel/acre) | Water Need | Fertilizer Need | Land Impact | Carbon Score |
|------|----------------------|------------|-----------------|-------------|--------------|
| Corn (ethanol) | 400 | High | High | High | 3/10 |
| Sugarcane (ethanol) | 590 | Very High | High | Medium | 5/10 |
| Soybean (biodiesel) | 48 | Medium | Low | High | 4/10 |
| Palm (biodiesel) | 635 | High | Medium | Very High | 2/10 |
| Switchgrass (cellulosic) | 300 | Low | Low | Low | 9/10 |
| Algae (biodiesel) | 5,000 | Low | None | None | 10/10 |
| Used cooking oil | N/A | None | None | None | 10/10 |

---

## Real Game Mechanic: Carbon Debt

One powerful mechanic borrowed from real science:

```
If you convert FOREST → CORN ETHANOL:
  Carbon released from forest: ~100 tons CO₂/acre
  Annual CO₂ savings from biofuel: ~1.5 tons CO₂/acre
  PAYBACK TIME: ~67 years

If you convert GRASSLAND → CORN ETHANOL:
  Carbon released from grassland: ~30 tons CO₂/acre
  Annual CO₂ savings: ~1.5 tons CO₂/acre
  PAYBACK TIME: ~20 years

If you grow CELLULOSIC ETHANOL on MARGINAL LAND:
  Carbon released: ~0
  PAYBACK TIME: Immediate
```

The player's "carbon debt" counter ticks down each year based on their
fuel production. They only become truly "green" after the debt is paid off.
This is surprisingly accurate to real science!

---

Sources: Fields of Fuel (GLBRC), IRENA Bioenergy Simulator, research compilation
from 01-05 documents; real-world data from EIA, USDA, UMich CSS Factsheet
