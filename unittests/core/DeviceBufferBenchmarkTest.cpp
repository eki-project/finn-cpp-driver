#include "../../src/core/DeviceBuffer.hpp"
#include "../../src/utils/FinnDatatypes.hpp"
#include "../../src/utils/Logger.h"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include <chrono>
#include <fstream>

// Provides config and shapes for testing
#include "UnittestConfig.h"
using namespace FinnUnittest;


class Timer {
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

     public:
    Timer() = default;
    Timer(Timer&&) = default;
    Timer(const Timer&) = default;
    Timer(Timer&) = default;

    void start() {
        startTime = std::chrono::high_resolution_clock::now();
    }

    long int stopMs() {
        auto res = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
        return res;
    }
};


const unsigned int benchmarkBufferSize = 10000;

// TODO: Move setup into setup class

TEST(DBBenchmarkTest, StoreBenchmark) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();

    std::fstream logfile("benchmark_log.txt", std::fstream::out);

    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);

    // Setup
    std::vector<uint8_t> data;
    data.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    auto timer = Timer();
    

    // CHANGING VECTOR, REFERENCE PASSING
    auto idb1 = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        filler.fillRandom(data);
        idb1.store(data);
    }
    logfile << "[BENCHMARK] Changing Vector & Reference Passing: " << timer.stopMs() << "ms\n";


    // CHANGING VECTOR, ITERATOR PASSING
    auto idb2 = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        filler.fillRandom(data);
        idb2.store(data.begin(), data.end());
    } 
    logfile << "[BENCHMARK] Changing Vector & Iterator Passing: " << timer.stopMs() << "ms\n";


    // CONST VECTOR, REFERENCE PASSING
    auto idb3 = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        idb3.store(data);
    }
    logfile << "[BENCHMARK] Constant Vector & Reference Passing: " << timer.stopMs() << "ms\n";


    // CONST VECTOR, ITERATOR PASSING
    auto idb4 = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        idb4.store(data.begin(), data.end());
    }
    logfile << "[BENCHMARK] Constant Vector & Iterator Passing: " << timer.stopMs() << "ms\n";


    // Prepare multithreading (allocate beforehand to improve vector performance)
    unsigned int countThreads = 10;
    unsigned int countSamplesPerThread = benchmarkBufferSize / countThreads;

    auto threads = std::vector<std::thread>();
    threads.resize(countThreads);

    using Sample = std::vector<uint8_t>;
    using MultipleSamples = std::vector<Sample>;

    auto idb5 = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);

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

    // MULTITHREAD, CHANGING VECTOR, REFERENCE PASSING
    timer.start();
    for (unsigned int batch = 0; batch < countThreads; batch++) {
        threads[batch] = std::thread([&idb5, &datas, batch, countSamplesPerThread]() {
            for (unsigned int sampleNumber = 0; sampleNumber < countSamplesPerThread; sampleNumber++) {
                idb5.store(datas[batch][sampleNumber]);
            }
        });
    }
    for (unsigned int i = 0; i < countThreads; i++) {
        threads[i].join();
    }
    logfile << "[BENCHMARK] Multithreaded, Prepared Data & Reference Passing: " << timer.stopMs() << "ms\n";

    logfile.close();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}