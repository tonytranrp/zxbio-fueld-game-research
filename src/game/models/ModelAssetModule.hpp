#pragma once

#include "engine/runtime/typed/AssetDeclare.hpp"
#include "game/models/ModelSystem.hpp"

namespace biofuel::engine::runtime::typed::model {
struct MenuTransitionHands {};
} // namespace biofuel::engine::runtime::typed::model

namespace biofuel::engine::runtime::typed {
BIOFUEL_MODEL_ASSET(model::MenuTransitionHands,
    game::models::ModelAssetId::MenuTransitionHands,
    "menu_transition_hands",
    true);
BIOFUEL_ASSET_MODULE(ModelAssetModule, ModelAssetRegistry, model::MenuTransitionHands)
} // namespace biofuel::engine::runtime::typed

