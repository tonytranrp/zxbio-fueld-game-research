# EmbedShaders.cmake — Build-time shader source embedding
#
# This script reads all .glsl files from assets/shaders/ and generates
# ShaderSources.hpp with constexpr std::string_view for each.
#
# Called via add_custom_command at build time, so editing a .glsl file
# and hitting Build (F7) in Visual Studio will regenerate the header
# and recompile any files that include it.
#
# Usage:
#   cmake -P EmbedShaders.cmake -D SHADER_DIR=... -D OUTPUT_FILE=... -D SHADER_NAMES="a b c"

if(NOT SHADER_DIR OR NOT OUTPUT_FILE OR NOT SHADER_NAMES)
    message(FATAL_ERROR "EmbedShaders.cmake: SHADER_DIR, OUTPUT_FILE, and SHADER_NAMES must be set")
endif()

set(CONTENT "#pragma once\n#include <string_view>\n\nnamespace biofuel::shader_source {\n")

foreach(NAME ${SHADER_NAMES})
    set(SHADER_PATH "${SHADER_DIR}/${NAME}.glsl")
    if(NOT EXISTS "${SHADER_PATH}")
        message(FATAL_ERROR "Shader file not found: ${SHADER_PATH}")
    endif()

    file(READ "${SHADER_PATH}" SHADER_SOURCE)
    string(APPEND CONTENT "inline constexpr std::string_view ${NAME}_source = R\"shader(${SHADER_SOURCE})shader\";\n\n")
endforeach()

string(APPEND CONTENT "} // namespace biofuel::shader_source\n")

# Only write if content changed — avoids unnecessary rebuilds
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" EXISTING_CONTENT)
    if(NOT EXISTING_CONTENT STREQUAL CONTENT)
        file(WRITE "${OUTPUT_FILE}" "${CONTENT}")
        message(STATUS "Updated ${OUTPUT_FILE}")
    else()
        message(STATUS "ShaderSources.hpp is up to date")
    endif()
else()
    file(WRITE "${OUTPUT_FILE}" "${CONTENT}")
    message(STATUS "Generated ${OUTPUT_FILE}")
endif()
