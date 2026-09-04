# Group N: isolated, repeatable micro-benchmarks (Google Benchmark) complementing Tracy's live
# in-context profiling. The methodology ("did an optimization help?") is: a number from here AND a
# Tracy capture of the same code in real context, before and after -- one alone is insufficient.
# Not analyzed by clang-tidy for the same reason tests aren't: harness macros, not product code.
set(CMAKE_CXX_CLANG_TIDY "")

add_executable(voxel_benchmarks
    bench_chunk_map.cpp
    bench_octahedral.cpp
    bench_mesh_extract.cpp
)

target_link_libraries(voxel_benchmarks PRIVATE
    benchmark::benchmark_main
    unordered_dense::unordered_dense
    Boost::unordered
    world_chunk
    world_generation
    world_meshing
)
