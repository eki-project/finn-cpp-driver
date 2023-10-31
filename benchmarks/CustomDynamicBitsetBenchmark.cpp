/**
 * @file CustomDynamicBitsetBenchmark.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Benchmark for the Custom Dynamic Bitset
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <FINNCppDriver/utils/CustomDynamicBitset.h>
#include <benchmark/benchmark.h>

#include <FINNCppDriver/utils/DataPacking.hpp>
#include <bitset>
#include <boost/dynamic_bitset.hpp>
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
