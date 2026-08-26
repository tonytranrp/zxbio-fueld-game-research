add_executable(pb_basic_order_pipeline basic_order_pipeline.cpp)
target_link_libraries(pb_basic_order_pipeline PRIVATE pb::pipeline)
pb_apply_project_options(pb_basic_order_pipeline PRIVATE)
pb_enable_pch_if_requested(pb_basic_order_pipeline)

add_executable(pb_error_diagnostic_example error_diagnostic.cpp)
target_link_libraries(pb_error_diagnostic_example PRIVATE pb::pipeline)
pb_apply_project_options(pb_error_diagnostic_example PRIVATE)
pb_enable_pch_if_requested(pb_error_diagnostic_example)

add_test(NAME pb_example_basic_order_pipeline COMMAND pb_basic_order_pipeline)
set_tests_properties(pb_example_basic_order_pipeline PROPERTIES LABELS "examples;smoke")
add_test(NAME pb_example_error_diagnostic COMMAND pb_error_diagnostic_example)
set_tests_properties(pb_example_error_diagnostic PROPERTIES LABELS "examples;smoke")
add_test(NAME pb_example_error_diagnostic_compile_fail
  COMMAND ${CMAKE_COMMAND}
    -DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/error_diagnostic.cpp
    -DTEST_COMPILER=${CMAKE_CXX_COMPILER}
    -DTEST_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/include
    -DTEST_DEFINITIONS=PB_EXAMPLE_ENABLE_COMPILE_FAILURE
    -DEXPECT_DIAGNOSTIC=Pipeline\ edge\ mismatch
    -P ${PROJECT_SOURCE_DIR}/tests/run_compile_fail.cmake
)
set_tests_properties(pb_example_error_diagnostic_compile_fail PROPERTIES LABELS "examples;compile-fail")

# ---------------------------------------------------------------------------
# Branch / join examples
# ---------------------------------------------------------------------------

add_executable(pb_branch_routing_demo branch_routing_demo.cpp)
target_link_libraries(pb_branch_routing_demo PRIVATE pb::pipeline)
pb_apply_project_options(pb_branch_routing_demo PRIVATE)
pb_enable_pch_if_requested(pb_branch_routing_demo)
add_test(NAME pb_example_branch_routing_demo COMMAND pb_branch_routing_demo)
set_tests_properties(pb_example_branch_routing_demo PROPERTIES LABELS "examples;branch")

add_executable(pb_branch_error_handling branch_error_handling.cpp)
target_link_libraries(pb_branch_error_handling PRIVATE pb::pipeline)
pb_apply_project_options(pb_branch_error_handling PRIVATE)
pb_enable_pch_if_requested(pb_branch_error_handling)
add_test(NAME pb_example_branch_error_handling COMMAND pb_branch_error_handling)
set_tests_properties(pb_example_branch_error_handling PROPERTIES LABELS "examples;branch")

# ---------------------------------------------------------------------------
# Descriptor schema demo
# ---------------------------------------------------------------------------

add_executable(pb_descriptor_demo descriptor_demo.cpp)
target_link_libraries(pb_descriptor_demo PRIVATE pb::pipeline)
pb_apply_project_options(pb_descriptor_demo PRIVATE)
pb_enable_pch_if_requested(pb_descriptor_demo)
add_test(NAME pb_example_descriptor_demo COMMAND pb_descriptor_demo)
set_tests_properties(pb_example_descriptor_demo PROPERTIES
  LABELS "examples;descriptor;branch"
  PASS_REGULAR_EXPRESSION "All descriptor demos completed\."
)

# ---------------------------------------------------------------------------
# Heterogeneous branch example
# ---------------------------------------------------------------------------

add_executable(pb_branch_heterogeneous_demo branch_heterogeneous_demo.cpp)
target_link_libraries(pb_branch_heterogeneous_demo PRIVATE pb::pipeline)
pb_apply_project_options(pb_branch_heterogeneous_demo PRIVATE)
pb_enable_pch_if_requested(pb_branch_heterogeneous_demo)
add_test(NAME pb_example_branch_heterogeneous_demo COMMAND pb_branch_heterogeneous_demo)
set_tests_properties(pb_example_branch_heterogeneous_demo PROPERTIES LABELS "examples;branch;heterogeneous")

# ---------------------------------------------------------------------------
# Thread-pool fan-in example
# ---------------------------------------------------------------------------

add_executable(pb_thread_pool_fan_in_demo thread_pool_fan_in_demo.cpp)
target_link_libraries(pb_thread_pool_fan_in_demo PRIVATE pb::pipeline)
pb_apply_project_options(pb_thread_pool_fan_in_demo PRIVATE)
pb_enable_pch_if_requested(pb_thread_pool_fan_in_demo)
add_test(NAME pb_example_thread_pool_fan_in_demo COMMAND pb_thread_pool_fan_in_demo)
set_tests_properties(pb_example_thread_pool_fan_in_demo PROPERTIES
  LABELS "examples;branch;fan-in;thread-pool"
  PASS_REGULAR_EXPRESSION "All fan-in cases joined in deterministic case order\."
)

# ---------------------------------------------------------------------------
# Policy DSL example (::with<pb::policy::...>)
# ---------------------------------------------------------------------------

add_executable(pb_policy_dsl_demo policy_dsl_demo.cpp)
target_link_libraries(pb_policy_dsl_demo PRIVATE pb::pipeline)
pb_apply_project_options(pb_policy_dsl_demo PRIVATE)
pb_enable_pch_if_requested(pb_policy_dsl_demo)
add_test(NAME pb_example_policy_dsl_demo COMMAND pb_policy_dsl_demo)
set_tests_properties(pb_example_policy_dsl_demo PROPERTIES
  LABELS "examples;policy;dsl"
  PASS_REGULAR_EXPRESSION "throwing: caught exception from stage 'reject'"
)

# ---------------------------------------------------------------------------
# Coroutine stage adapter example
# ---------------------------------------------------------------------------

add_executable(pb_coroutine_stage_demo coroutine_stage_demo.cpp)
target_link_libraries(pb_coroutine_stage_demo PRIVATE pb::pipeline)
pb_apply_project_options(pb_coroutine_stage_demo PRIVATE)
pb_enable_pch_if_requested(pb_coroutine_stage_demo)
add_test(NAME pb_example_coroutine_stage_demo COMMAND pb_coroutine_stage_demo)
set_tests_properties(pb_example_coroutine_stage_demo PROPERTIES LABELS "examples;adapter;coroutine")

# ---------------------------------------------------------------------------
# Synchronous sender/receiver adapter scaffold example
# ---------------------------------------------------------------------------

add_executable(pb_sender_receiver_stage_demo sender_receiver_stage_demo.cpp)
target_link_libraries(pb_sender_receiver_stage_demo PRIVATE pb::pipeline)
pb_apply_project_options(pb_sender_receiver_stage_demo PRIVATE)
pb_enable_pch_if_requested(pb_sender_receiver_stage_demo)
add_test(NAME pb_example_sender_receiver_stage_demo COMMAND pb_sender_receiver_stage_demo)
set_tests_properties(pb_example_sender_receiver_stage_demo PROPERTIES
  LABELS "examples;adapter;sender-receiver"
  PASS_REGULAR_EXPRESSION "low-risk order: auto-approved"
)
