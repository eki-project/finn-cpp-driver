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
    return Finn::Driver<SynchronousInference>(configFilePath, batchSize);
}

static void BM_SynchronousInferenceSingleThread(benchmark::State& state) {
    const std::string exampleNetworkConfig = "jetConfig.json";
    const uint batchSize = static_cast<uint>(state.range(0));
    std::cout << "Running single-threaded benchmark with batch size: " << batchSize << std::endl;
    auto driver = createDriverFromConfig<true>(exampleNetworkConfig, batchSize);
    using dtype = int8_t;

    // Create buffers for pipelining
    std::vector<dtype> inputBuffer(24 * batchSize);

    std::random_device rndDevice;
    std::mt19937 mersenneEngine{rndDevice()};
    destribution_t<dtype> dist{static_cast<dtype>(InputFinnType().min()), static_cast<dtype>(InputFinnType().max())};

    // Fill all buffers with random data
    std::generate(inputBuffer.begin(), inputBuffer.end(), [&dist, &mersenneEngine]() { return dist(mersenneEngine); });

    // Warmup
    auto warmup = driver.inferSynchronous(inputBuffer.begin(), inputBuffer.end());
    benchmark::DoNotOptimize(warmup);

    std::chrono::duration<float> runtime = std::chrono::seconds(90);  // Fixed runtime for the benchmark

    for (auto _ : state) {
        std::size_t processedCount = 0;

        // Set a fixed time for the benchmark
        const auto start = std::chrono::high_resolution_clock::now();

        while (std::chrono::high_resolution_clock::now() - start < std::chrono::duration<float>(runtime)) {
            auto results = driver.inferSynchronous(inputBuffer.begin(), inputBuffer.end());
            benchmark::DoNotOptimize(results);
            ++processedCount;
        }
        const auto end = std::chrono::high_resolution_clock::now();

        std::size_t infered = processedCount * batchSize;

        auto elapsed_seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start);

        state.SetIterationTime(elapsed_seconds.count());
        state.SetItemsProcessed(static_cast<int64_t>(infered));
    }

    std::cout << state.iterations() << "\n";
}

// Register the function as a benchmark
BENCHMARK(BM_SynchronousInferenceSingleThread)->RangeMultiplier(2)->Range(1, 4096)->Repetitions(5)->UseManualTime();

BENCHMARK_MAIN();
