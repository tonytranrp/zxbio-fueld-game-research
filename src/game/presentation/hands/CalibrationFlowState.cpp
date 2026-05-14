#include "game/presentation/hands/CalibrationFlowState.hpp"

namespace biofuel::game::presentation::hands {

CalibrationFlowState& CalibrationFlowState::instance() noexcept {
    static CalibrationFlowState state{};
    return state;
}

void CalibrationFlowState::begin(const CalibrationRoute routeValue) noexcept {
    m_route = routeValue;
    m_outcome = routeValue == CalibrationRoute::None ? CalibrationOutcome::None : CalibrationOutcome::Pending;
}

void CalibrationFlowState::complete() noexcept {
    if (m_route != CalibrationRoute::None) {
        m_outcome = CalibrationOutcome::Completed;
    }
}

void CalibrationFlowState::cancel() noexcept {
    if (m_route != CalibrationRoute::None) {
        m_outcome = CalibrationOutcome::Cancelled;
    }
}

void CalibrationFlowState::fail() noexcept {
    if (m_route != CalibrationRoute::None) {
        m_outcome = CalibrationOutcome::Failed;
    }
}

void CalibrationFlowState::clear() noexcept {
    m_route = CalibrationRoute::None;
    m_outcome = CalibrationOutcome::None;
}

} // namespace biofuel::game::presentation::hands
