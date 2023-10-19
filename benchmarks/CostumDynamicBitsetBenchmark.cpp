#ifndef COSTUMDYNAMICBITSETBENCHMARK_CPP
#define COSTUMDYNAMICBITSETBENCHMARK_CPP
#include <benchmark/benchmark.h>
#include <utils/CostumDynamicBitset.h>

#include <bitset>
#include <boost/dynamic_bitset.hpp>
#include <utils/DataPacking.hpp>
#include <vector>

static void BM_Boost_DynBitset(benchmark::State& state) {
    finnBoost::dynamic_bitset<uint8_t> set(1000000);
    for (auto _ : state)
#pragma omp for schedule(guided)
        for (std::size_t i = 0; i < set.size(); ++i) {
            set.set(i);
        }
}
// Register the function as a benchmark
BENCHMARK(BM_Boost_DynBitset)->Iterations(1000);

static void BM_Finn_DynBitset(benchmark::State& state) {
    DynamicBitset set(1000000);
    for (auto _ : state)
#pragma omp for schedule(guided)
        for (std::size_t i = 0; i < set.size(); ++i) {
            set.setSingleBit(i);
        }
}
// Register the function as a benchmark
BENCHMARK(BM_Finn_DynBitset)->Iterations(1000);

static void BM_Finn_DynBitset2(benchmark::State& state) {
    std::bitset<32> bit(2147483649);
    DynamicBitset set(1000000);
    for (auto _ : state)
        for (std::size_t i = 0; i <= 10000; ++i) {
            set.setByte(bit.to_ulong(), 0);
        }
}
// Register the function as a benchmark
BENCHMARK(BM_Finn_DynBitset2)->Iterations(1000);

static void BM_Boost_DynBitset2(benchmark::State& state) {
    std::bitset<32> bit(2147483649);
    finnBoost::dynamic_bitset<uint8_t> set(1000000);
    for (auto _ : state)
        for (std::size_t i = 0; i <= 10000; ++i) {
            for (std::size_t j = 0; j < 32; ++j) {
                set[j] = bit[j];
            }
        }
}
// Register the function as a benchmark
BENCHMARK(BM_Boost_DynBitset2)->Iterations(1000);


BENCHMARK_MAIN();

#endif  // COSTUMDYNAMICBITSETBENCHMARK_CPP
