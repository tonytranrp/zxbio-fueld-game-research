#pragma once

#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::gameplay {
struct EconomyTickEvent {};
struct EcologyTickEvent {};
struct SeasonAdvancedEvent {};
struct TechUnlockedEvent {};
struct SaveRequestedEvent {};
struct LoadRequestedEvent {};
struct GameStateChangedEvent {};

BIOFUEL_EVENT_TAG(EconomyTick, EconomyTickEvent);
BIOFUEL_EVENT_TAG(EcologyTick, EcologyTickEvent);
BIOFUEL_EVENT_TAG(SeasonAdvanced, SeasonAdvancedEvent);
BIOFUEL_EVENT_TAG(TechUnlocked, TechUnlockedEvent);
BIOFUEL_EVENT_TAG(SaveRequested, SaveRequestedEvent);
BIOFUEL_EVENT_TAG(LoadRequested, LoadRequestedEvent);
BIOFUEL_EVENT_TAG(GameStateChanged, GameStateChangedEvent);
} // namespace biofuel::engine::runtime::typed::gameplay

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(gameplay::EconomyTick, "gameplay.economy_tick");
BIOFUEL_EVENT_SPEC(gameplay::EcologyTick, "gameplay.ecology_tick");
BIOFUEL_EVENT_SPEC(gameplay::SeasonAdvanced, "gameplay.season_advanced");
BIOFUEL_EVENT_SPEC(gameplay::TechUnlocked, "gameplay.tech_unlocked");
BIOFUEL_EVENT_SPEC(gameplay::SaveRequested, "gameplay.save_requested");
BIOFUEL_EVENT_SPEC(gameplay::LoadRequested, "gameplay.load_requested");
BIOFUEL_EVENT_SPEC(gameplay::GameStateChanged, "gameplay.game_state_changed");
BIOFUEL_EVENT_MODULE(FutureGameplayEventModule, FutureGameplayEvents,
    gameplay::EconomyTick,
    gameplay::EcologyTick,
    gameplay::SeasonAdvanced,
    gameplay::TechUnlocked,
    gameplay::SaveRequested,
    gameplay::LoadRequested,
    gameplay::GameStateChanged)
} // namespace biofuel::engine::runtime::typed

