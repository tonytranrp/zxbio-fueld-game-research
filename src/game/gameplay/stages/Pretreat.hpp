#pragma once

#include "game/gameplay/stages/PassThrough.hpp"
#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Pretreats cellulosic biomass before fermentation.
/// Breaks down lignin and hemicellulose to make cellulose accessible.
/// Used in cellulosic processing pipeline (between GrindCrop and Ferment).
/// Currently a pass-through; yield modification deferred to future milestone.
using Pretreat = PassThrough<ProcessingInput>;

} // namespace biofuel::game::gameplay::stages
