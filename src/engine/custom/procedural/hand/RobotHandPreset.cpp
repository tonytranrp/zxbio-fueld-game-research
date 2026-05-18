#include "engine/custom/procedural/hand/RobotHandPreset.hpp"

#include <array>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace biofuel::engine::custom::procedural::hand {

namespace {

using json = nlohmann::json;

[[nodiscard]] json toJson(const Vector3 value) {
    return json::array({value.x, value.y, value.z});
}

[[nodiscard]] Vector3 vectorFromJson(const json& object, const Vector3 fallback, const char* field) {
    if (!object.is_array() || object.size() != 3U) {
        spdlog::warn("RobotHandPreset: '{}' must be a 3-number array, using default", field);
        return fallback;
    }

    const auto read = [&](const usize index, const f32 defaultValue) -> f32 {
        if (!object[index].is_number()) {
            spdlog::warn("RobotHandPreset: '{}[{}]' must be a number, using default", field, index);
            return defaultValue;
        }
        return object[index].get<f32>();
    };

    return Vector3{read(0U, fallback.x), read(1U, fallback.y), read(2U, fallback.z)};
}

[[nodiscard]] f32 numberFromJson(const json& object, const char* field, const f32 fallback) {
    if (!object.contains(field)) {
        return fallback;
    }
    if (!object[field].is_number()) {
        spdlog::warn("RobotHandPreset: '{}' must be a number, using default", field);
        return fallback;
    }
    return object[field].get<f32>();
}

void readFingerDimensions(const json& source, HandRigDimensions& dimensions) {
    static constexpr std::array<std::string_view, static_cast<usize>(FingerId::Count)> names{
        "thumb", "index", "middle", "ring", "pinky",
    };

    if (!source.is_object()) {
        return;
    }

    for (usize index = 0U; index < names.size(); ++index) {
        const std::string name{names[index]};
        if (!source.contains(name) || !source[name].is_object()) {
            continue;
        }

        const json& finger = source[name];
        HandFingerDimensions& target = dimensions.fingers[index];
        if (finger.contains("baseOffset")) {
            target.baseOffset = vectorFromJson(finger["baseOffset"], target.baseOffset, "finger.baseOffset");
        }
        if (finger.contains("direction")) {
            target.direction = vectorFromJson(finger["direction"], target.direction, "finger.direction");
        }
        target.segmentLength = numberFromJson(finger, "segmentLength", target.segmentLength);
    }
}

void writeFingerDimensions(json& target, const HandRigDimensions& dimensions) {
    static constexpr std::array<std::string_view, static_cast<usize>(FingerId::Count)> names{
        "thumb", "index", "middle", "ring", "pinky",
    };

    for (usize index = 0U; index < names.size(); ++index) {
        const HandFingerDimensions& finger = dimensions.fingers[index];
        const std::string name{names[index]};
        target[name] = json{
            {"baseOffset", toJson(finger.baseOffset)},
            {"direction", toJson(finger.direction)},
            {"segmentLength", finger.segmentLength},
        };
    }
}

[[nodiscard]] RobotHandMaterialSlot slotFromName(const std::string& name) {
    if (name == "shell") { return RobotHandMaterialSlot::Shell; }
    if (name == "shellShadow") { return RobotHandMaterialSlot::ShellShadow; }
    if (name == "accent") { return RobotHandMaterialSlot::Accent; }
    if (name == "joint") { return RobotHandMaterialSlot::Joint; }
    if (name == "jointEdge") { return RobotHandMaterialSlot::JointEdge; }
    if (name == "target") { return RobotHandMaterialSlot::Target; }
    if (name == "palmPanel") { return RobotHandMaterialSlot::PalmPanel; }
    return RobotHandMaterialSlot::Count;
}

[[nodiscard]] std::string_view slotName(const RobotHandMaterialSlot slot) {
    switch (slot) {
    case RobotHandMaterialSlot::Shell: return "shell";
    case RobotHandMaterialSlot::ShellShadow: return "shellShadow";
    case RobotHandMaterialSlot::Accent: return "accent";
    case RobotHandMaterialSlot::Joint: return "joint";
    case RobotHandMaterialSlot::JointEdge: return "jointEdge";
    case RobotHandMaterialSlot::Target: return "target";
    case RobotHandMaterialSlot::PalmPanel: return "palmPanel";
    case RobotHandMaterialSlot::Count: break;
    default: break;
    }
    return "unknown";
}

} // namespace

RobotHandPreset RobotHandPresetStore::load(const std::filesystem::path& path) const {
    RobotHandPreset preset{};
    if (!std::filesystem::exists(path)) {
        spdlog::warn("RobotHandPreset: '{}' not found, using typed defaults", path.generic_string());
        return preset;
    }

    std::ifstream input(path);
    if (!input) {
        spdlog::warn("RobotHandPreset: failed to open '{}', using typed defaults", path.generic_string());
        return preset;
    }

    json root;
    try {
        input >> root;
    } catch (const std::exception& error) {
        spdlog::warn("RobotHandPreset: failed to parse '{}': {}", path.generic_string(), error.what());
        return preset;
    }

    if (root.contains("name") && root["name"].is_string()) {
        preset.name = root["name"].get<std::string>();
    }

    if (root.contains("rig") && root["rig"].is_object()) {
        const json& rig = root["rig"];
        if (rig.contains("palmJointOffset")) {
            preset.dimensions.palmJointOffset = vectorFromJson(rig["palmJointOffset"], preset.dimensions.palmJointOffset, "rig.palmJointOffset");
        }
        if (rig.contains("fingers")) {
            readFingerDimensions(rig["fingers"], preset.dimensions);
        }
    }

    if (root.contains("materials") && root["materials"].is_object()) {
        const json& materials = root["materials"];
        preset.materials.shellIntensity = numberFromJson(materials, "shellIntensity", preset.materials.shellIntensity);
        preset.materials.accentIntensity = numberFromJson(materials, "accentIntensity", preset.materials.accentIntensity);
        preset.materials.jointIntensity = numberFromJson(materials, "jointIntensity", preset.materials.jointIntensity);
    }

    if (root.contains("textures") && root["textures"].is_object()) {
        for (const auto& [name, value] : root["textures"].items()) {
            if (!value.is_string()) {
                spdlog::warn("RobotHandPreset: texture '{}' must be a path string, ignoring", name);
                continue;
            }
            const RobotHandMaterialSlot slot = slotFromName(name);
            if (slot == RobotHandMaterialSlot::Count) {
                spdlog::warn("RobotHandPreset: unknown texture slot '{}', ignoring", name);
                continue;
            }
            preset.textures.slots[static_cast<usize>(slot)] = std::filesystem::path{value.get<std::string>()};
        }
    }

    if (root.contains("ik") && root["ik"].is_object()) {
        const json& ik = root["ik"];
        preset.ik.maxIterations = static_cast<i32>(numberFromJson(ik, "maxIterations", static_cast<f32>(preset.ik.maxIterations)));
        preset.ik.tolerance = numberFromJson(ik, "tolerance", preset.ik.tolerance);
    }

    if (root.contains("animation") && root["animation"].is_object()) {
        preset.animationSpeed = numberFromJson(root["animation"], "speed", preset.animationSpeed);
    }

    return preset;
}

bool RobotHandPresetStore::exportJson(const RobotHandPreset& preset, const std::filesystem::path& path) const {
    json fingers;
    writeFingerDimensions(fingers, preset.dimensions);

    json textures = json::object();
    for (usize index = 0U; index < preset.textures.slots.size(); ++index) {
        const auto slot = static_cast<RobotHandMaterialSlot>(index);
        const auto& texturePath = preset.textures.slots[index];
        if (!texturePath.empty()) {
            textures[std::string{slotName(slot)}] = texturePath.generic_string();
        }
    }

    const json root{
        {"schemaVersion", RobotHandPreset::SCHEMA_VERSION},
        {"name", preset.name},
        {"rig", {
            {"palmJointOffset", toJson(preset.dimensions.palmJointOffset)},
            {"fingers", fingers},
        }},
        {"materials", {
            {"shellIntensity", preset.materials.shellIntensity},
            {"accentIntensity", preset.materials.accentIntensity},
            {"jointIntensity", preset.materials.jointIntensity},
        }},
        {"textures", textures},
        {"ik", {
            {"maxIterations", preset.ik.maxIterations},
            {"tolerance", preset.ik.tolerance},
        }},
        {"animation", {
            {"speed", preset.animationSpeed},
        }},
    };

    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            spdlog::warn("RobotHandPreset: failed to create '{}': {}", parent.generic_string(), error.message());
            return false;
        }
    }

    std::ofstream output(path);
    if (!output) {
        spdlog::warn("RobotHandPreset: failed to write '{}'", path.generic_string());
        return false;
    }

    output << root.dump(2) << '\n';
    return true;
}

} // namespace biofuel::engine::custom::procedural::hand
