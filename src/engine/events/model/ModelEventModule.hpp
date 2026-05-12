#pragma once

#include "engine/events/model/ModelEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::model {
BIOFUEL_EVENT_TAG(SetState, ::biofuel::engine::events::model::ModelSetStateEvent);
BIOFUEL_EVENT_TAG(PlayAction, ::biofuel::engine::events::model::ModelPlayActionEvent);
} // namespace biofuel::engine::runtime::typed::model

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(model::SetState, "model.set_state");
BIOFUEL_EVENT_SPEC(model::PlayAction, "model.play_action");
BIOFUEL_EVENT_MODULE(ModelEventModule, ModelEvents, model::SetState, model::PlayAction)
} // namespace biofuel::engine::runtime::typed

