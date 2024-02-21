/**
 * @file DynamicMdSpanBenchmark.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de)
 * @brief Benchmark for the DynamicMdSpan
 * @version 0.1
 * @date 2024-02-02
 *
 * @copyright Copyright (c) 2024
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <FINNCppDriver/utils/Types.h>
#include <benchmark/benchmark.h>

#include <FINNCppDriver/utils/DataPacking.hpp>
#include <FINNCppDriver/utils/DynamicMdSpan.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <algorithm>
#include <random>

#include "omp.h"

std::random_device rnd_device;
std::mt19937 mersenne_engine{rnd_device()};

const unsigned int iterations = 1000;

static void BM_BaseWithoutDynMdSpan(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state) {
        auto ret = Finn::pack<Finn::DatatypeUInt<8>>(inp.begin(), inp.end());
    }
}
BENCHMARK(BM_BaseWithoutDynMdSpan)->Iterations(iterations);


static void BM_BaseWithDynMdSpan(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    Finn::DynamicMdSpan sp(inp.begin(), inp.end(), {1000000});
    for (auto _ : state) {
        for (auto&& spe : sp.getMostInnerDims()) {
            auto ret = Finn::pack<Finn::DatatypeUInt<8>>(spe.begin(), spe.end());
        }
    }
}
BENCHMARK(BM_BaseWithDynMdSpan)->Iterations(iterations);


static void BM_MDWithDynMdSpan2x500k(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    Finn::DynamicMdSpan sp(inp.begin(), inp.end(), {2, 500000});
    for (auto _ : state) {
        for (auto&& spe : sp.getMostInnerDims()) {
            auto ret = Finn::pack<Finn::DatatypeUInt<8>>(spe.begin(), spe.end());
        }
    }
}
BENCHMARK(BM_MDWithDynMdSpan2x500k)->Iterations(iterations);

static void BM_MDWithDynMdSpan500kx2(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    std::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    Finn::DynamicMdSpan sp(inp.begin(), inp.end(), {500000, 2});
    for (auto _ : state) {
        for (auto&& spe : sp.getMostInnerDims()) {
            auto ret = Finn::pack<Finn::DatatypeUInt<8>>(spe.begin(), spe.end());
        }
    }
}
BENCHMARK(BM_MDWithDynMdSpan500kx2)->Iterations(iterations);

static void BM_BaseWithoutDynMdSpanFinnVec(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state) {
        auto ret = Finn::pack<Finn::DatatypeUInt<8>>(inp.begin(), inp.end());
    }
}
BENCHMARK(BM_BaseWithoutDynMdSpanFinnVec)->Iterations(iterations);


static void BM_BaseWithDynMdSpanFinnVec(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    Finn::DynamicMdSpan sp(inp.begin(), inp.end(), {1000000});
    for (auto _ : state) {
        for (auto&& spe : sp.getMostInnerDims()) {
            auto ret = Finn::pack<Finn::DatatypeUInt<8>>(spe.begin(), spe.end());
        }
    }
}
BENCHMARK(BM_BaseWithDynMdSpanFinnVec)->Iterations(iterations);


static void BM_MDWithDynMdSpan2x500kFinnVec(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    Finn::DynamicMdSpan sp(inp.begin(), inp.end(), {2, 500000});
    for (auto _ : state) {
        for (auto&& spe : sp.getMostInnerDims()) {
            auto ret = Finn::pack<Finn::DatatypeUInt<8>>(spe.begin(), spe.end());
        }
    }
}
BENCHMARK(BM_MDWithDynMdSpan2x500kFinnVec)->Iterations(iterations);

static void BM_MDWithDynMdSpan500kx2FinnVec(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    Finn::DynamicMdSpan sp(inp.begin(), inp.end(), {500000, 2});
    for (auto _ : state) {
        for (auto&& spe : sp.getMostInnerDims()) {
            auto ret = Finn::pack<Finn::DatatypeUInt<8>>(spe.begin(), spe.end());
        }
    }
}
BENCHMARK(BM_MDWithDynMdSpan500kx2FinnVec)->Iterations(iterations);

static void BM_MDWithDynMdSpan500kx2FinnVecMT(benchmark::State& state) {
    std::uniform_int_distribution<uint8_t> dist{0, 255};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<uint8_t> inp(1000000);
    std::generate(inp.begin(), inp.end(), gen);
    Finn::DynamicMdSpan sp(inp.begin(), inp.end(), {500000, 2});
    for (auto _ : state) {
#pragma omp parallel for
        for (auto&& spe : sp.getMostInnerDims()) {
            auto ret = Finn::pack<Finn::DatatypeUInt<8>>(spe.begin(), spe.end());
        }
    }
}
BENCHMARK(BM_MDWithDynMdSpan500kx2FinnVecMT)->Iterations(iterations);

constexpr std::size_t batchSize = 16;

static void BM_MDWithoutDynMdSpanBx10INT5FinnVec(benchmark::State& state) {
    std::uniform_int_distribution<int8_t> dist{-8, 8};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<int8_t> inp(batchSize * 10);
    std::generate(inp.begin(), inp.end(), gen);
    for (auto _ : state) {
        auto ret = Finn::pack<Finn::DatatypeInt<5>>(inp.begin(), inp.end());
        benchmark::DoNotOptimize(ret);
        // auto ret2 = Finn::unpack<Finn::DatatypeInt<5>>(ret);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_MDWithoutDynMdSpanBx10INT5FinnVec)->Iterations(iterations);

static void BM_MDWithDynMdSpanBx5x2INT5FinnVec(benchmark::State& state) {
    std::uniform_int_distribution<int8_t> dist{-8, 8};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<int8_t> inp(batchSize * 10);
    std::generate(inp.begin(), inp.end(), gen);
    Finn::DynamicMdSpan sp(inp.begin(), inp.end(), {batchSize, 5, 2});
    for (auto _ : state) {
        auto vec = sp.getMostInnerDims();
        for (auto&& spe : vec) {
            auto ret = Finn::pack<Finn::DatatypeInt<5>>(spe.begin(), spe.end());
            benchmark::DoNotOptimize(ret);
            // auto ret2 = Finn::unpack<Finn::DatatypeInt<5>>(ret);
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_MDWithDynMdSpanBx5x2INT5FinnVec)->Iterations(iterations);

static void BM_MDWithDynMdSpanBx10INT5FinnVec(benchmark::State& state) {
    std::uniform_int_distribution<int8_t> dist{-8, 8};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<int8_t> inp(batchSize * 10);
    std::generate(inp.begin(), inp.end(), gen);
    Finn::DynamicMdSpan sp(inp.begin(), inp.end(), {batchSize, 10});
    for (auto _ : state) {
        for (auto&& spe : sp.getMostInnerDims()) {
            auto ret = Finn::pack<Finn::DatatypeInt<5>>(spe.begin(), spe.end());
            benchmark::DoNotOptimize(ret);
            // auto ret2 = Finn::unpack<Finn::DatatypeInt<5>>(ret);
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_MDWithDynMdSpanBx10INT5FinnVec)->Iterations(iterations);

static void BM_MDWithDynMdSpanBx5x2INT5FinnVecMT(benchmark::State& state) {
    std::uniform_int_distribution<int8_t> dist{-8, 8};
    auto gen = [&dist]() { return dist(mersenne_engine); };
    Finn::vector<int8_t> inp(batchSize * 10);
    std::generate(inp.begin(), inp.end(), gen);
    Finn::DynamicMdSpan sp(inp.begin(), inp.end(), {batchSize, 5, 2});
    for (auto _ : state) {
        auto vec = sp.getMostInnerDims();
#pragma omp parallel for if (vec.size() > 100)
        for (auto&& spe : vec) {
            auto ret = Finn::pack<Finn::DatatypeInt<5>>(spe.begin(), spe.end());
            benchmark::DoNotOptimize(ret);
            // auto ret2 = Finn::unpack<Finn::DatatypeInt<5>>(ret);
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_MDWithDynMdSpanBx5x2INT5FinnVecMT)->Iterations(iterations);

BENCHMARK_MAIN();