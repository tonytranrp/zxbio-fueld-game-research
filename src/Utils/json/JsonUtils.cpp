#include "JsonUtils.hpp"

namespace biofuel::utils::json {

Json JsonUtils::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Json{};
    }
    Json data;
    file >> data;
    return data;
}

void JsonUtils::saveToFile(const std::string& path, const Json& data) {
    std::ofstream file(path);
    if (file.is_open()) {
        file << data.dump(4);
    }
}

Json JsonUtils::parseString(const std::string& str) {
    return Json::parse(str, nullptr, false);
}

} // namespace biofuel::utils::json
