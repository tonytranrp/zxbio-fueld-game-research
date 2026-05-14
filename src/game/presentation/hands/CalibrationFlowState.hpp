#pragma once

namespace biofuel::game::presentation::hands {

enum class CalibrationRoute {
    None,
    Join,
};

enum class CalibrationOutcome {
    None,
    Pending,
    Completed,
    Cancelled,
    Failed,
};

class CalibrationFlowState final {
public:
    static CalibrationFlowState& instance() noexcept;

    void begin(CalibrationRoute route) noexcept;
    void complete() noexcept;
    void cancel() noexcept;
    void fail() noexcept;
    void clear() noexcept;

    [[nodiscard]] CalibrationRoute route() const noexcept { return m_route; }
    [[nodiscard]] CalibrationOutcome outcome() const noexcept { return m_outcome; }
    [[nodiscard]] bool pending() const noexcept { return m_outcome == CalibrationOutcome::Pending; }
    [[nodiscard]] bool completed() const noexcept { return m_outcome == CalibrationOutcome::Completed; }

private:
    CalibrationRoute m_route = CalibrationRoute::None;
    CalibrationOutcome m_outcome = CalibrationOutcome::None;
};

} // namespace biofuel::game::presentation::hands
