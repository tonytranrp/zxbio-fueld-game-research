#pragma once

#include <pb/runtime/observer.hpp>
#include <string_view>

namespace biofuel::engine::runtime::typed {
class EventManager;
}

namespace biofuel::game::gameplay {

/// Bridges Pipeline-c- lifecycle events to the EnTT event bus.
/// When pipeline stages complete, this observer publishes the corresponding
/// FutureEventModule.hpp event types via Events::publish<T>().
///
/// Stage-to-event mapping (matched by PascalCase stage class names):
///   "SeasonAdvance"                        → SeasonAdvancedEvent
///   "EcologyUpdate"                        → EcologyTickEvent
///   "EconomyUpdate"                        → EconomyTickEvent
///   "ValidateCrop" / "CalculateYield" /
///     "UpdateInventory"                    → GameStateChangedEvent
///   "WashCrop" / "GrindCrop" / "Ferment" /
///     "PressExtract" / "Pretreat" /
///     "Distill" / "Transesterify"          → EconomyTickEvent
///   "QueueResearch" / "AdvanceResearch" /
///     "UnlockTech"                         → TechUnlockedEvent
///
/// Legacy dot-notation keys (e.g. "turn.season_advance") are also matched.
/// Unknown stage names are silently ignored (no crash, no event published).
class PipelineEventObserver : public pb::runtime::observer {
public:
    PipelineEventObserver() = default;
    ~PipelineEventObserver() override = default;

    void on_stage_start(const pb::runtime::stage_id& id) override;
    void on_stage_success(const pb::runtime::stage_id& id) override;
    void on_stage_failure(const pb::runtime::stage_id& id, const pb::runtime::error& err) override;
    void on_stage_exception(const pb::runtime::stage_id& id, const pb::runtime::error& err) override;

private:
    /// Maps a stage key to the appropriate EnTT event publication.
    void publishEventForKey(std::string_view key) const;
};

} // namespace biofuel::game::gameplay