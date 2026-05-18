#pragma once

#include <pb/runtime/observer.hpp>
#include <pb/runtime/error.hpp>
#include <pb/runtime/result.hpp>
#include <string_view>
#include <vector>

namespace biofuel::engine::runtime::typed {
class EventManager;
}

namespace biofuel::game::gameplay {

/// Bridges Pipeline-c- lifecycle events to the EnTT event bus.
/// When pipeline stages complete, this observer publishes the corresponding
/// FutureEventModule.hpp event types via Events::publish<T>().
///
/// Additionally accumulates pb::runtime::error records from stage failures
/// and exceptions so callers can inspect pipeline health after execution.
/// This extends the observer with pb::runtime::result-compatible error
/// tracking — errors collected here carry the same stage/category/message
/// triple that pb::runtime::result<…> would propagate.
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

    /// Accumulated errors from stage failures and exceptions since last clear.
    /// Each error carries stage identity, category, and message — matching the
    /// shape of a pb::runtime::result error payload.
    [[nodiscard]] const std::vector<pb::runtime::error>& errors() const noexcept { return m_errors; }

    /// True if at least one error has been recorded since the last clear.
    [[nodiscard]] bool has_errors() const noexcept { return !m_errors.empty(); }

    /// Reset the accumulated error list (typically called between pipeline runs).
    void clear_errors() noexcept { m_errors.clear(); }

    /// Convenience: last recorded error, or a default-constructed error if empty.
    [[nodiscard]] pb::runtime::error last_error() const noexcept {
        return m_errors.empty() ? pb::runtime::error{} : m_errors.back();
    }

private:
    /// Maps a stage key to the appropriate EnTT event publication.
    void publishEventForKey(std::string_view key) const;

    std::vector<pb::runtime::error> m_errors;
};

} // namespace biofuel::game::gameplay