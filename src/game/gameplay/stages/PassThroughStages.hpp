#pragma once

#include "game/gameplay/stages/PassThrough.hpp"
#include "game/gameplay/stages/ProcessTypes.hpp"
#include "game/gameplay/stages/TurnTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Placeholder fuel-process stages that currently forward ProcessingInput
/// unchanged. Replace an alias with a concrete stage struct when that gameplay
/// step gains behavior.
using WashCrop = PassThrough<ProcessingInput>;
using GrindCrop = PassThrough<ProcessingInput>;
using Ferment = PassThrough<ProcessingInput>;
using PressExtract = PassThrough<ProcessingInput>;
using Pretreat = PassThrough<ProcessingInput>;

/// Placeholder turn-stage economy hook. Replace with a concrete stage when
/// seasonal market/cost behavior is implemented.
using EconomyUpdate = PassThrough<TurnOutput>;

} // namespace biofuel::game::gameplay::stages
