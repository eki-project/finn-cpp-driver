add_custom_target(Benchmarks)
set_target_properties(Benchmarks PROPERTIES FOLDER "benchmarks")

set(FINNC_BENCHMARK_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_benchmark(DataPackingBenchmark.cpp)
add_benchmark(CostumDynamicBitsetBenchmark.cpp)