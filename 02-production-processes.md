# Biofuel Production Processes

## Overview: Three Ways to Get Energy from Biomass

1. **Direct combustion** — burn solid biomass (wood, pellets) for heat/electricity
2. **Bacterial decomposition** — anaerobic digestion produces methane (biogas)
3. **Conversion to liquid/gas fuel** — the main focus for transportation biofuels

The two main production pathways:

```
                    BIOMASS
                       │
          ┌────────────┴────────────┐
          │                         │
   BIOCHEMICAL                 THERMOCHEMICAL
   (enzymes, microbes)         (heat, pressure)
          │                         │
   ┌──────┼──────┐           ┌──────┼──────┐
   │      │      │           │      │      │
Ferment- Trans- Anaerobic   Pyro-  Gasifi- Hydro-
 ation   ester. Digestion   lysis  cation  thermal
   │      │      │           │      │      │
Ethanol Biodiesel Biogas   Bio-oil Syngas Bio-crude
```

---

## Process 1: Fermentation → Ethanol

### How It Works (Biochemical)

This is the same process used to make alcoholic beverages, but fuel ethanol is
**denatured** (made undrinkable) to avoid liquor taxes.

```
SUGAR CROPS (sugarcane)          STARCH CROPS (corn)
       │                                │
       │ Crush/juice                    │ Grind, add water, cook
       ▼                                ▼
   Sugar juice                      Starch slurry
       │                                │
       │                                │ Hydrolysis (enzymes break
       │                                │ starch → sugar)
       │                                ▼
       └────────────┬───────────────────┘
                    │
                    ▼
              Sugar solution
                    │
                    │ Add yeast
                    ▼
              FERMENTATION
          C₆H₁₂O₆ → 2 C₂H₅OH + 2 CO₂
          (sugar)      (ethanol)  (carbon dioxide)
                    │
                    │ Produces ~10-15% ethanol "beer"
                    ▼
              DISTILLATION
              (boil off ethanol,
               water stays behind)
                    │
                    ▼
          ~95% ethanol + ~5% water
                    │
                    │ Molecular sieve dehydration
                    ▼
              ~99.5% pure ethanol
              (fuel-grade, "anhydrous ethanol")
```

### Key Numbers

- **Corn ethanol:** ~2.8 gallons of ethanol per bushel of corn
- **Sugarcane ethanol:** ~590 gallons per acre (vs. 370-430 for corn)
- **Energy balance (FER):** Corn ethanol ~1.5 (output ÷ fossil input),
  sugarcane ethanol ~2.1, cellulosic ethanol ~4.4–6.6
- A typical dry-mill ethanol plant produces ~100 million gallons/year
- **Byproduct:** Distillers grains (high-protein animal feed)

### Cellulosic Ethanol (Advanced)

For 2nd-gen ethanol using non-food biomass like corn stover, wood chips, or switchgrass:

1. **Pretreatment** — chemical or mechanical breakdown of lignin/hemicellulose
   to expose cellulose
2. **Hydrolysis** — enzymes (cellulases) break cellulose into fermentable sugars
3. **Fermentation** — same as above
4. **Distillation** — same as above

Challenge: Cellulose is much harder to break down than starch. No commercial-scale
cellulosic ethanol plants operate in the US as of 2022 despite years of R&D.

---

## Process 2: Transesterification → Biodiesel

### The Chemical Reaction

```
     Triglyceride     +    3 Methanol    →    3 Methyl Esters   +   Glycerol
       (oil/fat)           (alcohol)            (BIODIESEL!)       (byproduct)
```

**In numbers:**

> 100 lbs oil + 10 lbs methanol → 100 lbs biodiesel + 10 lbs glycerin
> (with NaOH or KOH catalyst)

### Step-by-Step Production

```
                 VEGETABLE OIL / ANIMAL FAT
                 (triglycerides)
                        │
                        │ Filter & remove water/contaminants
                        ▼
                 CLEAN FEEDSTOCK
                        │
           ┌────────────┴────────────┐
           │                         │
     METHANOL + NaOH           HEATED OIL
     (mix to make              (~130°F / 55°C)
      sodium methoxide)             │
           │                         │
           └────────────┬────────────┘
                        │
                        ▼
                 REACTOR (1-2 hours)
                 TRANSESTERIFICATION
                        │
                        ▼
                 SETTLING TANK
                 (glycerin sinks,
                  biodiesel floats)
                        │
           ┌────────────┴────────────┐
           │                         │
           ▼                         ▼
      CRUDE GLYCERIN           CRUDE BIODIESEL
      (sold for pharma,              │
       cosmetics, soaps)        WASHING (remove
                                     │  soap, catalyst,
                                     │  residual methanol)
                                     ▼
                                DRYING
                                     │
                                     ▼
                                B100 BIODIESEL
                                (ready for blending)
```

### Catalyst & Alcohol Options

| Component | Common Choice | Alternative |
|-----------|--------------|-------------|
| Alcohol | Methanol (cheap, toxic) | Ethanol (safer, more expensive) |
| Catalyst | NaOH (sodium hydroxide) | KOH (potassium hydroxide) |
| Commercial | Sodium methoxide solution | — |

### Important Safety Notes (Game-Relevant!)

- **Methanol is highly toxic** — blindness or death if ingested, dangerous via skin/inhalation
- **NaOH/KOH are caustic** — can cause severe chemical burns
- **Methanol + NaOH reaction is exothermic** — generates heat
- Glycerin byproduct must be purified before use (residual methanol)
- **Saponification risk:** Too much water or free fatty acids in the oil +
  NaOH catalyst = soap instead of biodiesel!

### Quality Standards

Biodiesel must meet **ASTM D6751** specification (US) or **EN 14214** (Europe).
Key tests: viscosity, flash point, free glycerin, acid number, oxidation stability.

---

## Process 3: Anaerobic Digestion → Biogas

```
        ORGANIC WASTE
   (manure, food waste, crop residues, sewage)
              │
              │ Feed into sealed digester tank
              ▼
        ANAEROBIC DIGESTER
        (no oxygen, ~35-55°C / 95-130°F)
              │
              │ Bacteria digest over 20-40 days
              ▼
    ┌─────────┴─────────┐
    │                   │
    ▼                   ▼
  BIOGAS             DIGESTATE
  (~60% CH₄,         (nutrient-rich
   ~40% CO₂)          fertilizer)
    │
    │ Clean/upgrade
    │ (remove CO₂, H₂S)
    ▼
  BIOMETHANE
  (pipeline-quality,
   can replace natural gas)
```

Typical biogas composition: 50-70% methane, 30-50% CO₂, trace H₂S and water vapor.

---

## Process 4: Thermochemical Routes

### Pyrolysis

Heating biomass without oxygen (~400-600°C) → bio-oil + biochar + syngas

- **Bio-oil:** Can be upgraded to diesel/gasoline replacements
- **Biochar:** Solid carbon-rich residue — soil amendment, carbon sequestration

### Gasification

Heating biomass with limited oxygen/steam at high temp (>700°C) → **syngas** (CO + H₂)

- Syngas can be burned directly, or converted to liquid fuels via
  **Fischer-Tropsch synthesis** (makes synthetic diesel, jet fuel, gasoline)

### Hydrothermal Liquefaction (HTL)

Uses wet biomass (algae, sewage, slurries) at high pressure and moderate
temperature (~300°C) → **bio-crude** (similar to petroleum crude oil)
→ can be refined into gasoline, diesel, jet fuel

---

## Summary: Which Process for Which Biofuel?

| Biofuel | Feedstock | Process | Type | Maturity |
|---------|-----------|---------|------|----------|
| Ethanol | Corn, sugarcane, cellulose | Fermentation + distillation | Biochemical | Commercial |
| Biodiesel | Vegetable oils, animal fats, WCO | Transesterification | Biochemical | Commercial |
| Renewable diesel | Oils, fats, WCO | Hydrotreating | Thermochemical | Growing |
| Biogas | Organic waste, manure | Anaerobic digestion | Biochemical | Commercial |
| Bio-oil | Any dry biomass | Pyrolysis | Thermochemical | Demo |
| Syngas → FT liquids | Any dry biomass | Gasification + FT | Thermochemical | Pilot |
| Bio-crude → drop-in fuels | Wet biomass, algae | HTL + refining | Thermochemical | R&D |
| Cellulosic ethanol | Crop residues, wood, grasses | Pretreatment + enzymatic hydrolysis + fermentation | Biochemical | Pilot |

---

Sources: USDA Climate Hubs, AFDC (DOE), Farm Energy Extension, Penn State EGEE 439,
MSU Biodiesel Production Guide
