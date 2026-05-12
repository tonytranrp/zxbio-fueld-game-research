#pragma once

#include "engine/custom/procedural/hand/HandTypes.hpp"
#include "engine/custom/procedural/hand/RobotHandMaterials.hpp"
#include "engine/custom/procedural/ik/IkTypes.hpp"
#include <filesystem>
#include <string>

namespace biofuel::engine::custom::procedural::hand {

struct RobotHandPreset {
    static constexpr i32 SCHEMA_VERSION = 1;

    std::string name = "biofuel_robot_default";
    HandRigDimensions dimensions = defaultRobotHandDimensions();
    RobotHandMaterialState materials{};
    RobotHandTexturePaths textures{};
    ::biofuel::engine::custom::procedural::ik::IkSolveSettings ik{};
    f32 animationSpeed = 1.0f;
};

template<typename TPresetTag>
struct RobotHandPresetSpec;

template<>
struct RobotHandPresetSpec<DefaultRobotHandPreset> {
    static constexpr std::string_view name = "biofuel_robot_default";
    static constexpr std::string_view relativePath = "assets/custom/procedural/hand/presets/biofuel_robot_default.json";
};

class RobotHandPresetStore final {
public:
    template<typename TPresetTag>
    [[nodiscard]] RobotHandPreset load() const {
        return load(std::filesystem::path{RobotHandPresetSpec<TPresetTag>::relativePath});
    }

    [[nodiscard]] RobotHandPreset load(const std::filesystem::path& path) const;
    [[nodiscard]] bool exportJson(const RobotHandPreset& preset, const std::filesystem::path& path) const;
};

} // namespace biofuel::engine::custom::procedural::hand
