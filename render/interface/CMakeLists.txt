add_library(render_interface INTERFACE)

target_sources(render_interface INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/render/interface/camera.hpp
)

target_link_libraries(render_interface INTERFACE project_warnings engine_core)
