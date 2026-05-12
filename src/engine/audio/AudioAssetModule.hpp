#pragma once

#include "engine/runtime/typed/AssetDeclare.hpp"

namespace biofuel::engine::runtime::typed::sound {
struct MenuMove {};
struct MenuAccept {};
} // namespace biofuel::engine::runtime::typed::sound

namespace biofuel::engine::runtime::typed::music {
struct IdleAmbient {};
} // namespace biofuel::engine::runtime::typed::music

namespace biofuel::engine::runtime::typed {
BIOFUEL_SOUND_ASSET(sound::MenuMove, "menu.move", "", false);
BIOFUEL_SOUND_ASSET(sound::MenuAccept, "menu.accept", "", false);
BIOFUEL_MUSIC_ASSET(music::IdleAmbient, "idle.ambient", "", false);
BIOFUEL_ASSET_MODULE(SoundAssetModule, SoundAssetRegistry, sound::MenuMove, sound::MenuAccept)
BIOFUEL_ASSET_MODULE(MusicAssetModule, MusicAssetRegistry, music::IdleAmbient)
} // namespace biofuel::engine::runtime::typed

