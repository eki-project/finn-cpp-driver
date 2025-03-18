add_custom_target(Benchmarks)
set_target_properties(Benchmarks PROPERTIES FOLDER "benchmarks")

set(FINN_BENCHMARK_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_benchmark(DataPackingBenchmark.cpp)
add_benchmark(CustomDynamicBitsetBenchmark.cpp)
add_benchmark(DynamicMdSpanBenchmark.cpp)