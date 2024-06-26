/**
 * @file DataPackingBenchmark.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Unittest for the data packing and unpacking functionalities of the FINN driver
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <benchmark/benchmark.h>

#include <FINNCppDriver/utils/DataPacking.hpp>
#include <algorithm>
#include <random>
#include <vector>

std::random_device rnd_device;
std::mt19937 mersenne_engine{rnd_device()};


static void BM_PackUINT8_1M(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeUInt<8>>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackUINT8_1M)->Iterations(1000);

static void BM_PackINT3_1M(benchmark::State& state) {
    std::uniform_int_distribution<int8_t> dist{-3, 3};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<int8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeInt<3>>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackINT3_1M)->Iterations(1000);

static void BM_PackINT4_1M(benchmark::State& state) {
    std::uniform_int_distribution<int8_t> dist{-8, 8};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<int8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeInt<4>>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackINT4_1M)->Iterations(1000);

static void BM_PackINT7_1M(benchmark::State& state) {
    std::uniform_int_distribution<int8_t> dist{-8, 8};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<int8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeInt<7>>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackINT7_1M)->Iterations(1000);

static void BM_PackINT7_600(benchmark::State& state) {
    std::uniform_int_distribution<int8_t> dist{-8, 8};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<int8_t> inp(600);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeInt<7>>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackINT7_600)->Iterations(10000);

static void BM_PackINT32_1M(benchmark::State& state) {
    std::uniform_int_distribution<int32_t> dist{-1000, 1000};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<int32_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeInt<32>>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackINT32_1M)->Iterations(1000);

static void BM_PackFLOAT32_F_1M(benchmark::State& state) {
    std::uniform_real_distribution<> dist{-1000, 1000};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<float> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeFloat>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackFLOAT32_F_1M)->Iterations(1000);

static void BM_PackFLOAT32_D_1M(benchmark::State& state) {
    std::uniform_real_distribution<> dist{-1000, 1000};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<double> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeFloat>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackFLOAT32_D_1M)->Iterations(1000);

static void BM_PackFLOAT32_D_500K(benchmark::State& state) {
    std::uniform_real_distribution<> dist{-1000, 1000};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<double> inp(500000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeFloat>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackFLOAT32_D_500K)->Iterations(1000);

static void BM_PackFLOAT32_D_250K(benchmark::State& state) {
    std::uniform_real_distribution<> dist{-1000, 1000};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<double> inp(250000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeFloat>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackFLOAT32_D_250K)->Iterations(1000);

static void BM_PackFLOAT32_D_600(benchmark::State& state) {
    std::uniform_real_distribution<> dist{-1000, 1000};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<double> inp(250000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::pack<Finn::DatatypeFloat>(inp.begin(), inp.end());
}
// Register the function as a benchmark
BENCHMARK(BM_PackFLOAT32_D_600)->Iterations(10000);

static void BM_UnpackUINT8_1M(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::unpack<Finn::DatatypeUInt<8>>(inp);
}
// Register the function as a benchmark
BENCHMARK(BM_UnpackUINT8_1M)->Iterations(1000);

static void BM_UnpackUINT16_1M(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::unpack<Finn::DatatypeUInt<16>>(inp);
}
// Register the function as a benchmark
BENCHMARK(BM_UnpackUINT16_1M)->Iterations(1000);

static void BM_UnpackINT10_1M(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::unpack<Finn::DatatypeInt<10>>(inp);
}
// Register the function as a benchmark
BENCHMARK(BM_UnpackINT10_1M)->Iterations(1000);

static void BM_UnpackINT16_1M(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::unpack<Finn::DatatypeInt<16>>(inp);
}
// Register the function as a benchmark
BENCHMARK(BM_UnpackINT16_1M)->Iterations(1000);

static void BM_UnpackUINT64_1M(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::unpack<Finn::DatatypeUInt<64>>(inp);
}
// Register the function as a benchmark
BENCHMARK(BM_UnpackUINT64_1M)->Iterations(1000);

static void BM_UnpackFLOAT_1M(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::unpack<Finn::DatatypeFloat>(inp);
}
// Register the function as a benchmark
BENCHMARK(BM_UnpackFLOAT_1M)->Iterations(1000);

static void BM_UnpackFLOAT_600(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(600);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::unpack<Finn::DatatypeFloat>(inp);
}
// Register the function as a benchmark
BENCHMARK(BM_UnpackFLOAT_600)->Iterations(10000);

static void BM_UnpackINT10_600(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(600);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state)
        auto ret = Finn::unpack<Finn::DatatypeInt<10>>(inp);
}
// Register the function as a benchmark
BENCHMARK(BM_UnpackINT10_600)->Iterations(10000);


BENCHMARK_MAIN();
