# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/_deps/taskflow-src")
  file(MAKE_DIRECTORY "C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/_deps/taskflow-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/_deps/taskflow-build"
  "C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/_deps/taskflow-subbuild/taskflow-populate-prefix"
  "C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/_deps/taskflow-subbuild/taskflow-populate-prefix/tmp"
  "C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/_deps/taskflow-subbuild/taskflow-populate-prefix/src/taskflow-populate-stamp"
  "C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/_deps/taskflow-subbuild/taskflow-populate-prefix/src"
  "C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/_deps/taskflow-subbuild/taskflow-populate-prefix/src/taskflow-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/_deps/taskflow-subbuild/taskflow-populate-prefix/src/taskflow-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/build/_deps/taskflow-subbuild/taskflow-populate-prefix/src/taskflow-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
