/**
 * @file DeviceBufferBenchmark.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de)
 * @brief Benchmark for the Device Buffer Benchmark
 * @version 0.1
 * @date 2023-11-14
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <benchmark/benchmark.h>

#include <FINNCppDriver/utils/RingBuffer.hpp>
#include <FINNCppDriver/utils/RingBufferNew.hpp>

const unsigned int iterations = 1000;
const unsigned int elementSize = 100000;

static void BM_RingBufferStore(benchmark::State& state) {
    RingBuffer<uint8_t> rb(iterations, elementSize);
    Finn::vector<uint8_t> vec(elementSize);
    std::iota(vec.begin(), vec.end(), 0);

    for (auto _ : state) {
        rb.store(vec.begin(), vec.end());
    }
}
BENCHMARK(BM_RingBufferStore)->Iterations(iterations);

static void BM_RingBufferStoreFast(benchmark::State& state) {
    RingBuffer<uint8_t> rb(iterations, elementSize);
    Finn::vector<uint8_t> vec(elementSize);
    std::iota(vec.begin(), vec.end(), 0);

    for (auto _ : state) {
        rb.storeFast(vec.begin(), vec.end());
    }
}
BENCHMARK(BM_RingBufferStoreFast)->Iterations(iterations);

static void BM_RingBufferStoreNewST(benchmark::State& state) {
    Finn::RingBuffer<uint8_t, false> rb(iterations, elementSize);
    Finn::vector<uint8_t> vec(elementSize);
    std::iota(vec.begin(), vec.end(), 0);

    for (auto _ : state) {
        rb.store(vec.begin(), vec.end());
    }
}
BENCHMARK(BM_RingBufferStoreNewST)->Iterations(iterations);

static void BM_RingBufferStoreNewMT(benchmark::State& state) {
    Finn::RingBuffer<uint8_t, true> rb(iterations, elementSize);
    Finn::vector<uint8_t> vec(elementSize);
    std::iota(vec.begin(), vec.end(), 0);

    for (auto _ : state) {
        rb.store(vec.begin(), vec.end());
    }
}
BENCHMARK(BM_RingBufferStoreNewMT)->Iterations(iterations);

static void BM_RingBufferReadNewMT(benchmark::State& state) {
    Finn::RingBuffer<uint8_t, true> rb(iterations, elementSize);
    Finn::vector<uint8_t> vec(iterations * elementSize);
    std::iota(vec.begin(), vec.end(), 0);

    rb.store(vec.begin(), vec.end());

    Finn::vector<uint8_t> out;
    out.reserve(elementSize);

    for (auto _ : state) {
        rb.read(std::back_inserter(out));
        out.clear();
    }
}
BENCHMARK(BM_RingBufferReadNewMT)->Iterations(iterations);

static void BM_RingBufferReadNewST(benchmark::State& state) {
    Finn::RingBuffer<uint8_t, true> rb(iterations, elementSize);
    Finn::vector<uint8_t> vec(iterations * elementSize);
    std::iota(vec.begin(), vec.end(), 0);

    rb.store(vec.begin(), vec.end());

    Finn::vector<uint8_t> out;
    out.reserve(elementSize);

    for (auto _ : state) {
        rb.read(std::back_inserter(out));
        out.clear();
    }
}
BENCHMARK(BM_RingBufferReadNewST)->Iterations(iterations);

static void BM_RingBufferRead(benchmark::State& state) {
    RingBuffer<uint8_t> rb(iterations, elementSize);
    Finn::vector<uint8_t> vec(elementSize);
    std::iota(vec.begin(), vec.end(), 0);

    for (int i = 0; i < iterations; ++i) {
        rb.storeFast(vec.begin(), vec.end());
    }

    Finn::vector<uint8_t> out;
    out.reserve(elementSize);

    for (auto _ : state) {
        rb.read(std::back_inserter(out));
        out.clear();
    }
}
BENCHMARK(BM_RingBufferRead)->Iterations(iterations);

static void BM_RingBufferReadWriteNewST(benchmark::State& state) {
    Finn::RingBuffer<uint8_t, true> rb(iterations, elementSize);
    Finn::vector<uint8_t> vec(elementSize);
    std::iota(vec.begin(), vec.end(), 0);

    Finn::vector<uint8_t> out;
    out.reserve(elementSize);

    for (auto _ : state) {
        rb.store(vec.begin(), vec.end());
        rb.read(std::back_inserter(out));
        out.clear();
    }
}
BENCHMARK(BM_RingBufferReadWriteNewST)->Iterations(iterations);

static void BM_RingBufferReadWriteFast(benchmark::State& state) {
    RingBuffer<uint8_t> rb(iterations, elementSize);
    Finn::vector<uint8_t> vec(elementSize);
    std::iota(vec.begin(), vec.end(), 0);

    Finn::vector<uint8_t> out;
    out.reserve(elementSize);

    for (auto _ : state) {
        rb.storeFast(vec.begin(), vec.end());
        rb.read(std::back_inserter(out));
        out.clear();
    }
}
BENCHMARK(BM_RingBufferReadWriteFast)->Iterations(iterations);

BENCHMARK_MAIN();