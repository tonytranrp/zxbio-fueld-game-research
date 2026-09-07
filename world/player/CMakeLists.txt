add_library(world_player STATIC
    src/player_state.cpp
    src/view_polish.cpp
)

target_sources(world_player PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/player/tuning.hpp
        include/world/player/player_state.hpp
        include/world/player/fixed_step.hpp
        include/world/player/controller.hpp
        include/world/player/view_polish.hpp
)

target_link_libraries(world_player PUBLIC project_warnings engine_core world_materials world_collision)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
