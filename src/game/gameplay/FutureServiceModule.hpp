#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(EconomyService);
BIOFUEL_SERVICE_TAG(EcologyService);
BIOFUEL_SERVICE_TAG(SeasonService);
BIOFUEL_SERVICE_TAG(TechService);
BIOFUEL_SERVICE_TAG(SaveService);
BIOFUEL_SERVICE_TAG(GameStateService);

struct EconomyServiceBackend {};
struct EcologyServiceBackend {};
struct SeasonServiceBackend {};
struct TechServiceBackend {};
struct SaveServiceBackend {};
struct GameStateServiceBackend {};

BIOFUEL_STATIC_SERVICE(EconomyService, "service.economy", EconomyServiceBackend);
BIOFUEL_STATIC_SERVICE(EcologyService, "service.ecology", EcologyServiceBackend);
BIOFUEL_STATIC_SERVICE(SeasonService, "service.season", SeasonServiceBackend);
BIOFUEL_STATIC_SERVICE(TechService, "service.tech", TechServiceBackend);
BIOFUEL_STATIC_SERVICE(SaveService, "service.save", SaveServiceBackend);
BIOFUEL_STATIC_SERVICE(GameStateService, "service.game_state", GameStateServiceBackend);
BIOFUEL_SERVICE_MODULE(FutureServiceModule,
    EconomyService,
    EcologyService,
    SeasonService,
    TechService,
    SaveService,
    GameStateService)
} // namespace biofuel::engine::runtime::typed

