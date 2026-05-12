#pragma once

#include "engine/core/typed/Meta.hpp"
#include "engine/runtime/typed/EventTags.hpp"
#include "engine/runtime/typed/ServiceTags.hpp"
#include <string_view>

namespace biofuel::game::gameplay {

template<typename TSystem>
struct SystemState {};

template<typename TSystem>
struct SystemModule {
    using System = TSystem;
    using State = SystemState<TSystem>;
};

template<typename TSystem>
struct SystemSpec;

namespace future {
struct Economy {};
struct Ecology {};
struct Season {};
struct Tech {};
struct Save {};
struct GameState {};
} // namespace future

#define BIOFUEL_FUTURE_SYSTEM_SPEC(TAG, LABEL, SERVICE, EVENT) \
    template<> struct SystemSpec<TAG> { \
        using System = TAG; \
        using Service = SERVICE; \
        using PrimaryEvent = EVENT; \
        static constexpr std::string_view Name = LABEL; \
    }

BIOFUEL_FUTURE_SYSTEM_SPEC(future::Economy, "future.economy", ::biofuel::engine::runtime::typed::EconomyService, ::biofuel::engine::runtime::typed::gameplay::EconomyTick);
BIOFUEL_FUTURE_SYSTEM_SPEC(future::Ecology, "future.ecology", ::biofuel::engine::runtime::typed::EcologyService, ::biofuel::engine::runtime::typed::gameplay::EcologyTick);
BIOFUEL_FUTURE_SYSTEM_SPEC(future::Season, "future.season", ::biofuel::engine::runtime::typed::SeasonService, ::biofuel::engine::runtime::typed::gameplay::SeasonAdvanced);
BIOFUEL_FUTURE_SYSTEM_SPEC(future::Tech, "future.tech", ::biofuel::engine::runtime::typed::TechService, ::biofuel::engine::runtime::typed::gameplay::TechUnlocked);
BIOFUEL_FUTURE_SYSTEM_SPEC(future::Save, "future.save", ::biofuel::engine::runtime::typed::SaveService, ::biofuel::engine::runtime::typed::gameplay::SaveRequested);
BIOFUEL_FUTURE_SYSTEM_SPEC(future::GameState, "future.game_state", ::biofuel::engine::runtime::typed::GameStateService, ::biofuel::engine::runtime::typed::gameplay::GameStateChanged);

#undef BIOFUEL_FUTURE_SYSTEM_SPEC

using FutureSystemRegistry = biofuel::typed::Registry<
    future::Economy,
    future::Ecology,
    future::Season,
    future::Tech,
    future::Save,
    future::GameState>;

static_assert(FutureSystemRegistry::valid());

} // namespace biofuel::game::gameplay
