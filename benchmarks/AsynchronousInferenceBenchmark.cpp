/**
 * @file AsynchronousInferenceBenchmark.cpp
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
#include <FINNCppDriver/utils/Logger.hpp>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
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

static void BM_AsynchronousInferenceSingleThread(benchmark::State& state) {
    const std::string exampleNetworkConfig = "jetConfig.json";
    const uint batchSize = static_cast<uint>(state.range(0));
    std::cout << "Running single-threaded benchmark with batch size: " << batchSize << std::endl;
    auto driver = createDriverFromConfig<false>(exampleNetworkConfig, batchSize);
    using dtype = int8_t;

    // Create buffers for pipelining
    std::vector<dtype> inputBuffer(24 * batchSize);

    std::random_device rndDevice;
    std::mt19937 mersenneEngine{rndDevice()};
    destribution_t<dtype> dist{static_cast<dtype>(InputFinnType().min()), static_cast<dtype>(InputFinnType().max())};

    // Fill all buffers with random data
    std::generate(inputBuffer.begin(), inputBuffer.end(), [&dist, &mersenneEngine]() { return dist(mersenneEngine); });

    // Warmup
    driver.input(inputBuffer.begin(), inputBuffer.end());
    auto warmup = driver.getResults();
    benchmark::DoNotOptimize(warmup);
    std::chrono::duration<float> runtime = std::chrono::seconds(90);  // Fixed runtime for the benchmark

    for (auto _ : state) {
        size_t processedCount = 0;

        // Set a fixed time for the benchmark
        const auto start = std::chrono::high_resolution_clock::now();

        while (std::chrono::high_resolution_clock::now() - start < std::chrono::duration<float>(runtime)) {
            // Submit as many inputs as we have available buffers
            driver.input(inputBuffer.begin(), inputBuffer.end());

            // Retrieve results (this makes it single-threaded - we wait for results)
            auto results = driver.getResults();
            benchmark::DoNotOptimize(results);
            ++processedCount;
        }
        std::size_t infered = processedCount * batchSize;

        // Report items processed in this iteration
        state.SetItemsProcessed(static_cast<int64_t>(infered));
    }
}

// Register the function as a benchmark
BENCHMARK(BM_AsynchronousInferenceSingleThread)->RangeMultiplier(2)->Range(1, 4096)->Repetitions(5);

static void BM_AsynchronousInferenceMultiThread(benchmark::State& state) {
    const std::string exampleNetworkConfig = "jetConfig.json";
    const uint batchSize = static_cast<uint>(state.range(0));
    std::cout << "Running multi-threaded benchmark with batch size: " << batchSize << std::endl;
    auto driver = createDriverFromConfig<false>(exampleNetworkConfig, batchSize);
    using dtype = int8_t;

    // Create buffers for pipelining
    std::vector<dtype> inputBuffer(24 * batchSize);

    std::random_device rndDevice;
    std::mt19937 mersenneEngine{rndDevice()};
    destribution_t<dtype> dist{static_cast<dtype>(InputFinnType().min()), static_cast<dtype>(InputFinnType().max())};

    // Fill all buffers with random data
    std::generate(inputBuffer.begin(), inputBuffer.end(), [&dist, &mersenneEngine]() { return dist(mersenneEngine); });

    // Warmup
    driver.input(inputBuffer.begin(), inputBuffer.end());
    auto warmup = driver.getResults();
    benchmark::DoNotOptimize(warmup);
    std::chrono::duration<float> runtime = std::chrono::seconds(90);  // Fixed runtime for the benchmark

    for (auto _ : state) {
        std::atomic<std::size_t> processedCount = 0;

        // Start input thread that continuously submits new inputs
        std::jthread inputThread([&](std::stop_token stoken) {
            // Set a fixed time for the benchmark
            while (!stoken.stop_requested()) {
                driver.input(inputBuffer.begin(), inputBuffer.end());
            }
        });

        // Start output thread that retrieves results
        std::jthread outputThread([&](std::stop_token stoken) {
            // Set a fixed time for the benchmark
            while (!stoken.stop_requested()) {
                auto results = driver.getResults();
                benchmark::DoNotOptimize(results);
                ++processedCount;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Make sure input thread is already exited
            driver.drain();                                               // Drain any remaining results; might need to be accounted for in runtime for inf/s calculation
        });

        const auto start = std::chrono::high_resolution_clock::now();
        while (std::chrono::high_resolution_clock::now() - start < std::chrono::duration<float>(runtime)) {}  // Looks stupid, but is for some reason more reliable...
        inputThread.request_stop();                                                                           // Stop input thread
        outputThread.request_stop();                                                                          // Stop output thread

        inputThread.join();
        outputThread.join();
        std::size_t infered = processedCount * batchSize;

        // Report items processed in this iteration
        state.SetItemsProcessed(static_cast<int64_t>(infered));
    }
}

// Register the multi-threaded benchmark
BENCHMARK(BM_AsynchronousInferenceMultiThread)->RangeMultiplier(2)->Range(1, 4096)->Repetitions(5);

BENCHMARK_MAIN();
