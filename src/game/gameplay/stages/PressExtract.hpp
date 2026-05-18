#pragma once

#include "game/gameplay/stages/PassThrough.hpp"
#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Presses/extracts oil from oilseed crops (soybean, algae).
/// Used in biodiesel processing pipeline (instead of Ferment).
/// Currently a pass-through; yield calculation happens in Transesterify (final stage).
using PressExtract = PassThrough<ProcessingInput>;

} // namespace biofuel::game::gameplay::stages
