add_executable(pb_bench_include_pipeline compile_time/include_pipeline.cpp)
target_link_libraries(pb_bench_include_pipeline PRIVATE pb::pipeline)
pb_apply_project_options(pb_bench_include_pipeline PRIVATE)
add_test(NAME pb_bench_include_pipeline COMMAND pb_bench_include_pipeline)
set_tests_properties(pb_bench_include_pipeline PROPERTIES LABELS "benchmark;compile_time;header;smoke")

add_executable(pb_bench_compile_chain_5 compile_time/chain_5.cpp)
target_link_libraries(pb_bench_compile_chain_5 PRIVATE pb::pipeline)
pb_apply_project_options(pb_bench_compile_chain_5 PRIVATE)
add_test(NAME pb_bench_compile_chain_5 COMMAND pb_bench_compile_chain_5)
set_tests_properties(pb_bench_compile_chain_5 PROPERTIES LABELS "benchmark;compile_time;compile-pass;smoke")

add_executable(pb_bench_compile_chain_50 compile_time/chain_50.cpp)
target_link_libraries(pb_bench_compile_chain_50 PRIVATE pb::pipeline)
pb_apply_project_options(pb_bench_compile_chain_50 PRIVATE)
add_test(NAME pb_bench_compile_chain_50 COMMAND pb_bench_compile_chain_50)
set_tests_properties(pb_bench_compile_chain_50 PROPERTIES LABELS "benchmark;compile_time;compile-pass;smoke")

add_executable(pb_bench_compile_branch_fan_in_10 compile_time/branch_fan_in_10.cpp)
target_link_libraries(pb_bench_compile_branch_fan_in_10 PRIVATE pb::pipeline)
pb_apply_project_options(pb_bench_compile_branch_fan_in_10 PRIVATE)
add_test(NAME pb_bench_compile_branch_fan_in_10 COMMAND pb_bench_compile_branch_fan_in_10)
set_tests_properties(pb_bench_compile_branch_fan_in_10 PROPERTIES LABELS "benchmark;compile_time;compile-pass;branch;fan-in;smoke")

add_custom_target(pb_compile_time_benchmarks
  DEPENDS
    pb_bench_include_pipeline
    pb_bench_compile_chain_5
    pb_bench_compile_chain_50
    pb_bench_compile_branch_fan_in_10
)

if(PB_BUILD_BENCHMARKS)
  add_executable(pb_bench_sequential_5_stage runtime/sequential_5_stage.cpp)
  target_link_libraries(pb_bench_sequential_5_stage PRIVATE pb::pipeline)
  pb_apply_project_options(pb_bench_sequential_5_stage PRIVATE)
  add_test(NAME pb_bench_sequential_5_stage COMMAND pb_bench_sequential_5_stage)
  set_tests_properties(pb_bench_sequential_5_stage PROPERTIES LABELS "benchmark;runtime;smoke")

  add_executable(pb_bench_thread_pool_fan_in runtime/thread_pool_fan_in.cpp)
  target_link_libraries(pb_bench_thread_pool_fan_in PRIVATE pb::pipeline)
  pb_apply_project_options(pb_bench_thread_pool_fan_in PRIVATE)
  add_test(NAME pb_bench_thread_pool_fan_in COMMAND pb_bench_thread_pool_fan_in)
  set_tests_properties(pb_bench_thread_pool_fan_in PROPERTIES LABELS "benchmark;runtime;thread-pool;smoke")
endif()
