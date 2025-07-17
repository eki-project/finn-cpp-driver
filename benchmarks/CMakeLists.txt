add_custom_target(Benchmarks)
set_target_properties(Benchmarks PROPERTIES FOLDER "benchmarks")

set(FINN_BENCHMARK_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_benchmark(DataPackingBenchmark.cpp)
add_benchmark(DynamicMdSpanBenchmark.cpp)
add_benchmark(SynchronousInferenceBenchmark.cpp)
add_benchmark(SPSCQueueBenchmark.cpp)
add_benchmark(AsynchronousInferenceBenchmark.cpp)
add_benchmark(RegressionTest.cpp)