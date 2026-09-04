add_library(render_diligent STATIC
    src/device_init.cpp
    src/pso_terrain.cpp
    src/terrain_renderer.cpp
    src/frustum_cull.cpp
    src/frame_verify.cpp
    src/vulkan_tools.cpp
    src/debug_overlay.cpp
    # The vendored ImGui GLFW platform backend (renderer half comes from Diligent-Imgui). Compiled
    # here because DiligentTools only builds its own Win32/etc. platform impls, and this app's
    # window is GLFW. Third-party TU: exempted from /W4 /WX below.
    "${DiligentEngine_SOURCE_DIR}/DiligentTools/ThirdParty/imgui/backends/imgui_impl_glfw.cpp"
)
set_source_files_properties(
    "${DiligentEngine_SOURCE_DIR}/DiligentTools/ThirdParty/imgui/backends/imgui_impl_glfw.cpp"
    PROPERTIES COMPILE_OPTIONS "/W3;/WX-"
)

target_sources(render_diligent PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/render/diligent/render_context.hpp
        include/render/diligent/terrain_renderer.hpp
        include/render/diligent/frustum.hpp
        include/render/diligent/memory_tracking.hpp
        include/render/diligent/frame_verify.hpp
        include/render/diligent/gpu_tools.hpp
        include/render/diligent/debug_overlay.hpp
)

# detail/ headers (the PIMPL innards holding real Diligent types) are includable by this module's
# own src/ only -- deliberately not part of the public FILE_SET, so the DiligentCore compile
# firewall (PROJECT_BRIEF.md §3) is enforced by the build, not by convention.
target_include_directories(render_diligent PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# glfw is PRIVATE and ONLY for the ImGui platform backend TU above -- the rendering core still
# consumes nothing but the opaque native window handle. Diligent-Imgui provides the ImGui renderer
# + the vendored imgui headers (PUBLIC include on that target); Tracy::TracyClient carries
# TRACY_ENABLE transitively into these TUs' zone macros.
target_link_libraries(render_diligent
    PUBLIC project_warnings engine_core world_chunk world_meshing render_interface
    PRIVATE Diligent-GraphicsEngineVk-static Diligent-Imgui glfw Tracy::TracyClient
)

# DiligentCore headers use the Graphics/...-from-repo-root include style. SYSTEM keeps third-party
# headers out of this project's /W4 /WX net (CMake maps SYSTEM to MSVC's /external:I).
target_include_directories(render_diligent SYSTEM PRIVATE
    "${DiligentEngine_SOURCE_DIR}/DiligentCore"
    # Vendored Vulkan headers for the Tracy GPU-zone / memory-budget TUs -- no Vulkan SDK install
    # needed (vulkan-1.dll is runtime-loaded, never import-linked).
    "${DiligentEngine_SOURCE_DIR}/DiligentCore/ThirdParty/Vulkan-Headers/include"
    # ImGuiImplDiligent.hpp lives in DiligentTools' interface dir; backends/ for imgui_impl_glfw.h.
    "${DiligentEngine_SOURCE_DIR}/DiligentTools/Imgui/interface"
    "${DiligentEngine_SOURCE_DIR}/DiligentTools/ThirdParty/imgui"
)

# Tracy's Vulkan context is created through runtime-loaded function pointers (see vulkan_tools.cpp).
target_compile_definitions(render_diligent PRIVATE TRACY_VK_USE_SYMBOL_TABLE)

target_compile_definitions(render_diligent PRIVATE
    VOXEL_TERRAIN_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
)
if(WIN32)
  # DiligentCore's NativeWindow.h/platform headers key off this; Diligent's own targets get it
  # from Diligent-PublicBuildSettings, which is not linked here to keep the dependency surface
  # explicit.
  target_compile_definitions(render_diligent PRIVATE PLATFORM_WIN32=1)
endif()

# D3D12 exists on this machine's Phase 0 build (all four backends) but stays feature-gated so a
# future configure without it (e.g. GPU-less CI) still builds the Vulkan-primary project.
if(TARGET Diligent-GraphicsEngineD3D12-static)
  target_link_libraries(render_diligent PRIVATE Diligent-GraphicsEngineD3D12-static)
  target_compile_definitions(render_diligent PRIVATE VOXEL_HAS_D3D12=1)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
