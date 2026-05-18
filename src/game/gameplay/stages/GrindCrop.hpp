#pragma once

#include "game/gameplay/stages/PassThrough.hpp"
#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Grinds the washed crop into a fine mash.
/// Used in ethanol and cellulosic processing pipelines.
/// Currently a pass-through; efficiency factors deferred to future milestone.
using GrindCrop = PassThrough<ProcessingInput>;

} // namespace biofuel::game::gameplay::stages
