#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <fstream>

namespace biofuel::utils::json {

using Json = nlohmann::json;

// ------------------------------------------------------------------------------
// JsonUtils - nlohmann/json helper functions
// ------------------------------------------------------------------------------
class JsonUtils {
public:
    [[nodiscard]] static Json loadFromFile(const std::string& path);
    static void saveToFile(const std::string& path, const Json& data);
    [[nodiscard]] static Json parseString(const std::string& str);
};

} // namespace biofuel::utils::json
