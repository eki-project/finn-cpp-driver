/**
 * @file DeviceBufferBenchmark.cpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de)
 * @brief Benchmark for the Device Buffer Benchmark
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <FINNCppDriver/utils/Logger.h>
#include <benchmark/benchmark.h>

#include <FINNCppDriver/core/DeviceBuffer.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <chrono>
#include <fstream>
#include <thread>

#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

// Provides config and shapes for testing
#include "../unittests/core/UnittestConfig.h"
using namespace FinnUnittest;


const unsigned int benchmarkBufferSize = 10000;
const unsigned int iterations = 100;

static void BM_VecCopyBase(benchmark::State& state) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    auto dat1 = std::vector<uint8_t>();
    auto dat2 = std::vector<uint8_t>();
    dat1.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    dat2.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    filler.fillRandom(dat1);
    filler.fillRandom(dat2);
    for (auto _ : state) {
        for (unsigned int part = 0; part < benchmarkBufferSize; part++) {
            for (unsigned int index = 0; index < dat1.size(); index++) {
                dat2[index] = dat1[index];
            }
        }
    }
}
BENCHMARK(BM_VecCopyBase)->Iterations(iterations);

//! Notation: CV = Changing vector, SV = Static vector, RP = Reference passing, IP = Iterator passing

static void BM_StoreCVRP(benchmark::State& state) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    std::vector<uint8_t> data;
    data.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    for (auto _ : state) {
        for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
            filler.fillRandom(data);
            idb.store(data);
        }
    }
}
BENCHMARK(BM_StoreCVRP)->Iterations(iterations);


static void BM_StoreSVRP(benchmark::State& state) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    std::vector<uint8_t> data;
    data.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    for (auto _ : state) {
        for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
            filler.fillRandom(data);
            idb.store(data.begin(), data.end());
        }
    }
}
BENCHMARK(BM_StoreSVRP)->Iterations(iterations);


static void BM_StoreCVIP(benchmark::State& state) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    std::vector<uint8_t> data;
    data.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    for (auto _ : state) {
        for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
            idb.store(data);
        }
    }
}
BENCHMARK(BM_StoreCVIP)->Iterations(iterations);


static void BM_StoreSVIP(benchmark::State& state) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    std::vector<uint8_t> data;
    data.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    for (auto _ : state) {
        for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
            idb.store(data.begin(), data.end());
        }
    }
}
BENCHMARK(BM_StoreSVIP)->Iterations(iterations);

/*
static void BM_StoreFCVRP(benchmark::State& state) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    const auto bytesPerSample = idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART);
    std::vector<uint8_t> data;
    data.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    for (auto _ : state) {
        for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
            idb.storeFast(data);
        }
    }
}
BENCHMARK(BM_StoreFCVRP)->Iterations(iterations);


static void BM_StoreFCVIP(benchmark::State& state) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    const auto bytesPerSample = idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART);
    std::vector<uint8_t> data;
    data.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    for (auto _ : state) {
        for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
            idb.storeFast(data.begin(), data.end());
        }
    }
}
BENCHMARK(BM_StoreFCVIP)->Iterations(iterations);
*/

static void BM_StoreMultithreaded_CVRP(benchmark::State& state) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize * 10);

    // Prepare multithreading (allocate beforehand to improve vector performance)
    using Sample = std::vector<uint8_t>;
    using MultipleSamples = std::vector<Sample>;
    unsigned int countThreads = 10;
    unsigned int countSamplesPerThread = benchmarkBufferSize * 10 / countThreads;

    auto threads = std::vector<std::thread>();
    threads.resize(countThreads);

    // Each of datas entries is a list of samples for the given thread
    auto datas = std::vector<MultipleSamples>();
    datas.resize(countThreads);
    for (unsigned int i = 0; i < countThreads; i++) {
        datas[i] = MultipleSamples();
        datas[i].resize(countSamplesPerThread);
        for (unsigned int j = 0; j < countSamplesPerThread; j++) {
            datas[i][j] = Sample();
            datas[i][j].resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
            filler.fillRandom(datas[i][j]);
        }
    }

    // The actual benchmark
    for (auto _ : state) {
        for (unsigned int batch = 0; batch < countThreads; batch++) {
            threads[batch] = std::thread([&idb, &datas, batch, countSamplesPerThread]() {
                for (unsigned int sampleNumber = 0; sampleNumber < countSamplesPerThread; sampleNumber++) {
                    idb.store(datas[batch][sampleNumber]);
                }
            });
        }
        for (unsigned int i = 0; i < countThreads; i++) {
            threads[i].join();
        }
    }
}
BENCHMARK(BM_StoreMultithreaded_CVRP)->Iterations(iterations);


BENCHMARK_MAIN();