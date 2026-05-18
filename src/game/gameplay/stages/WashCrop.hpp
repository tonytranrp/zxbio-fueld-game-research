#pragma once

#include "game/gameplay/stages/PassThrough.hpp"
#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Washes the raw crop material (removes dirt, debris).
/// First stage in all fuel processing pipelines.
/// Currently a pass-through; efficiency factors deferred to future milestone.
using WashCrop = PassThrough<ProcessingInput>;

} // namespace biofuel::game::gameplay::stages
