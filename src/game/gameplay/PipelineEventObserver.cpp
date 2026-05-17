#include "game/gameplay/PipelineEventObserver.hpp"
#include "engine/runtime/typed/Events.hpp"
#include "game/gameplay/FutureEventModule.hpp"
#include <spdlog/spdlog.h>

namespace biofuel::game::gameplay {

void PipelineEventObserver::on_stage_start(const pb::runtime::stage_id& id) {
    spdlog::trace("Pipeline stage starting: {}", std::string(id.key));
}

void PipelineEventObserver::on_stage_success(const pb::runtime::stage_id& id) {
    spdlog::trace("Pipeline stage succeeded: {}", std::string(id.key));
    publishEventForKey(id.key);
}

void PipelineEventObserver::on_stage_failure(const pb::runtime::stage_id& id, const pb::runtime::error& err) {
    spdlog::warn("Pipeline stage failed: {} - {}", std::string(id.key), std::string(err.message));
}

void PipelineEventObserver::on_stage_exception(const pb::runtime::stage_id& id, const pb::runtime::error& err) {
    spdlog::error("Pipeline stage exception: {} - {}", std::string(id.key), std::string(err.message));
}

void PipelineEventObserver::publishEventForKey(std::string_view key) const {
    using biofuel::engine::runtime::typed::Events;
    namespace gameplay_events = biofuel::engine::runtime::typed::gameplay;

    // Map stage keys to EnTT event publications.
    // Only the 7 existing event types from FutureEventModule.hpp are used.
    if (key == "turn.season_advance" || key == "SeasonAdvance") {
        Events::publish<gameplay_events::SeasonAdvanced>(gameplay_events::SeasonAdvancedEvent{});
    } else if (key == "turn.ecology_update" || key == "EcologyUpdate") {
        Events::publish<gameplay_events::EcologyTick>(gameplay_events::EcologyTickEvent{});
    } else if (key == "turn.economy_update" || key == "EconomyUpdate") {
        Events::publish<gameplay_events::EconomyTick>(gameplay_events::EconomyTickEvent{});
    } else if (key == "harvest.validate" || key == "ValidateCrop"
            || key == "harvest.calculate_yield" || key == "CalculateYield"
            || key == "harvest.update_inventory" || key == "UpdateInventory") {
        Events::publish<gameplay_events::GameStateChanged>(gameplay_events::GameStateChangedEvent{});
    } else if (key == "WashCrop" || key == "GrindCrop" || key == "Ferment"
            || key == "PressExtract" || key == "Pretreat"
            || key == "Distill" || key == "Transesterify") {
        Events::publish<gameplay_events::EconomyTick>(gameplay_events::EconomyTickEvent{});
    } else if (key == "QueueResearch" || key == "AdvanceResearch"
            || key == "UnlockTech") {
        Events::publish<gameplay_events::TechUnlocked>(gameplay_events::TechUnlockedEvent{});
    }
    // Unknown stage keys are silently ignored (no crash, no event published).
}

} // namespace biofuel::game::gameplay