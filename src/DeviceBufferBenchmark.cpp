#include "core/DeviceBuffer.hpp"
#include "utils/FinnDatatypes.hpp"
#include "utils/Logger.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include <chrono>
#include <fstream>

// Provides config and shapes for testing
#include "../unittests/core/UnittestConfig.h"
using namespace FinnUnittest;


#include "../../src/utils/Timer.hpp"

const unsigned int benchmarkBufferSize = 10000;



//TODO: Fix:
//! Currently for this to work, the sanitizers have to be disabled!



int main() {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    FINN_LOG(Logger::getLogger(), loglevel::info) << "Starting benchmark!";


    std::fstream logfile("benchmark_log.txt", std::fstream::out);

    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    FINN_LOG(Logger::getLogger(), loglevel::info) << "DeviceBuffer created!";

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
    auto t2 = std::vector<uint8_t>();
    t2.resize(data.size());
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        //idb3.store(data);
        for (unsigned int j = 0; j  < data.size(); j++) {
            t2[j] = data[j];
        }
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