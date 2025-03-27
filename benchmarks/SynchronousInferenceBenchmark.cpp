/**
 * @file SynchronousInferenceBenchmark.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de)
 * @brief Benchmarks the SynchronousInference Performance of the Driver
 * @version 0.1
 * @date 2025-03-21
 *
 * @copyright Copyright (c) 2025
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <benchmark/benchmark.h>

#include <FINNCppDriver/core/BaseDriver.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

template<typename O>
using destribution_t = typename std::conditional_t<std::is_same_v<O, float>, std::uniform_real_distribution<O>, std::uniform_int_distribution<O>>;

using InputFinnType = Finn::DatatypeInt<8>;
using OutputFinnType = Finn::DatatypeInt<16>;

namespace Finn {
    template<bool SynchronousInference>
    using Driver = Finn::BaseDriver<SynchronousInference, InputFinnType, OutputFinnType>;
}  // namespace Finn

template<bool SynchronousInference>
Finn::Driver<SynchronousInference> createDriverFromConfig(const std::filesystem::path& configFilePath, unsigned int batchSize) {
    Finn::Driver<SynchronousInference> driver(configFilePath, batchSize);
    driver.setForceAchieval(true);
    return driver;
}

static void BM_SynchronousInference(benchmark::State& state) {
    const std::string exampleNetworkConfig = "jetConfig.json";
    const uint batchSize = static_cast<uint>(state.range(0));
    auto driver = createDriverFromConfig<true>(exampleNetworkConfig, batchSize);
    using dtype = int8_t;
    Finn::vector<dtype> testInputs(24 * batchSize);

    std::random_device rndDevice;
    std::mt19937 mersenneEngine{rndDevice()};  // Generates random integers

    destribution_t<dtype> dist{static_cast<dtype>(InputFinnType().min()), static_cast<dtype>(InputFinnType().max())};

    auto gen = [&dist, &mersenneEngine]() { return dist(mersenneEngine); };

    // Warmup
    std::fill(testInputs.begin(), testInputs.end(), 1);
    auto warmup = driver.inferSynchronous(testInputs.begin(), testInputs.end());
    benchmark::DoNotOptimize(warmup);

    std::generate(testInputs.begin(), testInputs.end(), gen);
    for (auto _ : state) {
        auto ret = driver.inferSynchronous(testInputs.begin(), testInputs.end());
        benchmark::DoNotOptimize(ret);
        benchmark::ClobberMemory();
    }
}
// Register the function as a benchmark
BENCHMARK(BM_SynchronousInference)->Iterations(1000000)->RangeMultiplier(2)->Range(1, 4 << 10)->Repetitions(10);


BENCHMARK_MAIN();
