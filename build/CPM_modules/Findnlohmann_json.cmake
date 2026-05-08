include("C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/cmake/CPM_0.40.2.cmake")
CPMAddPackage("NAME;nlohmann_json;URL;https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz;OPTIONS;JSON_BuildTests OFF;JSON_Install OFF")
set(nlohmann_json_FOUND TRUE)