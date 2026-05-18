#pragma once

#include "game/gameplay/stages/PassThrough.hpp"
#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Ferments the ground mash into ethanol.
/// Used in ethanol and cellulosic processing pipelines.
/// Currently a pass-through; yield calculation happens in Distill (final stage).
using Ferment = PassThrough<ProcessingInput>;

} // namespace biofuel::game::gameplay::stages
