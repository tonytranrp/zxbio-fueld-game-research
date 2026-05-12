#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"
#include "engine/audio/AudioManager.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(AudioService);
BIOFUEL_RUNTIME_SERVICE(AudioService, "service.audio", ::biofuel::engine::audio::AudioManager,
    ::biofuel::engine::audio::AudioManager::instance());
BIOFUEL_SERVICE_MODULE(AudioServiceModule, AudioService)
} // namespace biofuel::engine::runtime::typed

