include("C:/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research/out/build/x64-Debug/cmake/CPM_0.40.2.cmake")
CPMAddPackage("NAME;Taskflow;URL;https://github.com/taskflow/taskflow/archive/refs/tags/v3.7.0.tar.gz;OPTIONS;TF_BUILD_TESTS OFF;TF_BUILD_EXAMPLES OFF")
set(Taskflow_FOUND TRUE)