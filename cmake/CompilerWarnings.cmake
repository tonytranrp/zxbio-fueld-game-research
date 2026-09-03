# Applied to our own targets only — never to fetched third-party targets (see Dependencies.cmake).
add_library(project_warnings INTERFACE)

if(MSVC)
  target_compile_options(project_warnings INTERFACE /W4 /WX)
else()
  target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()
