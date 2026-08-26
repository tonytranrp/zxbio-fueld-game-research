include_guard(GLOBAL)

function(pb_apply_project_options target visibility)
  if(PB_ENABLE_WARNINGS)
    target_compile_options(
      ${target}
      ${visibility}
        $<BUILD_INTERFACE:$<$<CXX_COMPILER_ID:MSVC>:/W4>>
        $<BUILD_INTERFACE:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>>
    )
  endif()

  if(PB_WARNINGS_AS_ERRORS)
    target_compile_options(
      ${target}
      ${visibility}
        $<BUILD_INTERFACE:$<$<CXX_COMPILER_ID:MSVC>:/WX>>
        $<BUILD_INTERFACE:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror>>
    )
  endif()

  if(PB_ENABLE_CLANG_TIME_TRACE)
    target_compile_options(
      ${target}
      ${visibility}
        $<BUILD_INTERFACE:$<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-ftime-trace>>
    )
  endif()
endfunction()

function(pb_enable_clang_tidy_if_requested target)
  if(NOT PB_ENABLE_CLANG_TIDY)
    return()
  endif()
  get_target_property(target_type ${target} TYPE)
  if(target_type STREQUAL "INTERFACE_LIBRARY")
    return()
  endif()
  find_program(PB_CLANG_TIDY_EXE NAMES clang-tidy)
  if(PB_CLANG_TIDY_EXE)
    set_property(TARGET ${target} PROPERTY CXX_CLANG_TIDY "${PB_CLANG_TIDY_EXE}")
  else()
    message(STATUS "PB_ENABLE_CLANG_TIDY requested but clang-tidy was not found")
  endif()
endfunction()

function(pb_enable_ipo_if_requested target)
  if(NOT PB_ENABLE_IPO)
    return()
  endif()
  get_target_property(target_type ${target} TYPE)
  if(target_type STREQUAL "INTERFACE_LIBRARY")
    return()
  endif()
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES CXX)
  if(ipo_supported)
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(STATUS "IPO requested but not supported: ${ipo_error}")
  endif()
endfunction()

function(pb_enable_pch_if_requested target)
  if(NOT PB_ENABLE_PCH)
    return()
  endif()
  get_target_property(target_type ${target} TYPE)
  if(target_type STREQUAL "INTERFACE_LIBRARY")
    return()
  endif()
  target_precompile_headers(${target} PRIVATE <pb/pipeline.hpp>)
endfunction()
