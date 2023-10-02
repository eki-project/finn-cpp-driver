#include "../src/core/DeviceBuffer.hpp"
#include "../src/utils/FinnDatatypes.hpp"
#include "../src/utils/Logger.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include <chrono>
#include <fstream>
#include <thread>

// Provides config and shapes for testing
#include "core/UnittestConfig.h"
using namespace FinnUnittest;

#include "../src/utils/Timer.hpp"

/**
 * @brief Calculate the number of bytes processed per second 
 * 
 * @param executionDurationMs 
 * @param batchsize 
 * @param bytesPerSample 
 * @return unsigned int 
 */
unsigned int bytesPerSecond(long int executionDurationMs, unsigned int batchsize, long unsigned int bytesPerSample) {
    return static_cast<unsigned int>((1000.0/static_cast<double>(executionDurationMs)) * batchsize * bytesPerSample);
}

void logBenchmark(std::fstream& logfile, const std::string& description, long int executionDurationMs, unsigned int batchsize, long unsigned int bytesPerSample) {
    logfile << "[BENCHMARK] " << description << "\n";
    logfile << "\tExecution time: " << executionDurationMs << "ms\n";
    logfile << "\tBatchsize: " << batchsize << "\n";
    logfile << "\tBytes per sample: " << bytesPerSample << "\n";
    logfile << "\tBytes per second: " << bytesPerSecond(executionDurationMs, batchsize, bytesPerSample) << " bytes/s\n";
    logfile << "\tKilobytes per second: " << bytesPerSecond(executionDurationMs, batchsize, bytesPerSample)/1000.0 << " kilobytes/s\n";
    logfile << "\tMegabytes per second: " << bytesPerSecond(executionDurationMs, batchsize, bytesPerSample)/1000000.0 << " megabytes/s\n\n";
}

const unsigned int benchmarkBufferSize = 10000;

int main() {
    std::fstream logfile("benchmark_log_execution.txt", std::fstream::out);
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto idb = Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);
    const auto bytesPerSample = idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART);
     
    logfile << "Bytes per sample: " << bytesPerSample << " (calculated by device buffer), " << FinnUtils::shapeToElements(myShapePacked) << " (calculated by shape util func)\n\n";

    auto timer = Timer();
    
    FINN_LOG(Logger::getLogger(), loglevel::info) << "Starting benchmark!";

    // Sanity check
    timer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    logfile << "[BENCHMARK] Sanity check (should be around 1000ms): " << timer.stopMs() << "ms\n";

    // Base comparison
    auto dat1 = std::vector<uint8_t>();
    auto dat2 = std::vector<uint8_t>();
    dat1.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    dat2.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    filler.fillRandom(dat1);
    filler.fillRandom(dat2);
    timer.start();
    for (unsigned int part = 0; part < benchmarkBufferSize; part++) {
        for (unsigned int index = 0; index < dat1.size(); index++) {
            dat2[index] = dat1[index];
        }
    }
    auto durationBaseline = timer.stopMs();
    logfile << "[BENCHMARK] Base comparison (copying a vector with length " << dat1.size() << " for " << benchmarkBufferSize << " times: " << durationBaseline << "ms)\n"; 
    logfile << "\t\t(" << bytesPerSecond(durationBaseline, benchmarkBufferSize, bytesPerSample) << " bytes/s)\n\n";

    // Setup
    std::vector<uint8_t> data;
    data.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    auto makeDB = [&device, &kernel]() {return Finn::DeviceInputBuffer<uint8_t>("Tester", device, kernel, myShapePacked, benchmarkBufferSize);};

    // CHANGING VECTOR, REFERENCE PASSING
    auto idb1 = makeDB();
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        filler.fillRandom(data);
        idb1.store(data);
    }
    logBenchmark(logfile, "Changing vector (random refill every sample) | Reference Passing", timer.stopMs(), benchmarkBufferSize, bytesPerSample);


    // CHANGING VECTOR, ITERATOR PASSING
    auto idb2 = makeDB();
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        filler.fillRandom(data);
        idb2.store(data.begin(), data.end());
    } 
    logBenchmark(logfile, "Changing vector (random refill every sample) | Iterator Passing", timer.stopMs(), benchmarkBufferSize, bytesPerSample);


    // CONST VECTOR, REFERENCE PASSING
    auto idb3 = makeDB();
    auto t2 = std::vector<uint8_t>();
    t2.resize(data.size());
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        idb3.store(data);
    }
    logBenchmark(logfile, "Constant vector | Reference Passing", timer.stopMs(), benchmarkBufferSize, bytesPerSample);


    // CONST VECTOR, ITERATOR PASSING
    auto idb4 = makeDB();
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        idb4.store(data.begin(), data.end());
    }
    logBenchmark(logfile, "Constant vector | Iterator Passing", timer.stopMs(), benchmarkBufferSize, bytesPerSample);

    // Prepare multithreading (allocate beforehand to improve vector performance)
    using Sample = std::vector<uint8_t>;
    using MultipleSamples = std::vector<Sample>;
    unsigned int countThreads = 10;
    unsigned int countSamplesPerThread = benchmarkBufferSize / countThreads;

    auto threads = std::vector<std::thread>();
    threads.resize(countThreads);

    auto idb5 = makeDB();

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

    // MULTITHREAD, CONSTANT VECTOR, REFERENCE PASSING
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
    logBenchmark(logfile, "Multithreaded | Constant vectors | Reference Passing", timer.stopMs(), benchmarkBufferSize, bytesPerSample);

    
    // Second multithread pass - see if amortization kicks in
    unsigned int largeBenchmarkBufferSize = benchmarkBufferSize * 10;
    countThreads = 10;
    countSamplesPerThread = largeBenchmarkBufferSize / countThreads;

    auto threads2 = std::vector<std::thread>();
    threads2.resize(countThreads);

    auto idb6 = makeDB();

    // Each of datas entries is a list of samples for the given thread
    auto datas2 = std::vector<MultipleSamples>();
    datas2.resize(countThreads);
    for (unsigned int i = 0; i < countThreads; i++) {
        datas2[i] = MultipleSamples();
        datas2[i].resize(countSamplesPerThread);
        for (unsigned int j = 0; j < countSamplesPerThread; j++) {
            datas2[i][j] = Sample();
            datas2[i][j].resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
            filler.fillRandom(datas2[i][j]);
        }
    }

    // MULTITHREAD, CONSTANT VECTOR, REFERENCE PASSING [larger quantity]
    timer.start();
    for (unsigned int batch = 0; batch < countThreads; batch++) {
        threads2[batch] = std::thread([&idb6, &datas2, batch, countSamplesPerThread]() {
            for (unsigned int sampleNumber = 0; sampleNumber < countSamplesPerThread; sampleNumber++) {
                idb6.store(datas2[batch][sampleNumber]);
            }
        });
    }
    for (unsigned int i = 0; i < countThreads; i++) {
        threads2[i].join();
    }
    logBenchmark(logfile, "Multithreaded | Constant vectors | Reference Passing | Large quantity of data", timer.stopMs(), largeBenchmarkBufferSize, bytesPerSample);


    // FACTORY STORE, CONSTANT VECTOR, REFERENCE PASSING
    auto idbFac = makeDB();
    filler.fillRandom(data);
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        idbFac.storeFast(data);        
    }
    logBenchmark(logfile, "Fast store method | Constant vector | Reference Passing", timer.stopMs(), benchmarkBufferSize, bytesPerSample);


    // FACTORY STORE, CONSTANT VECTOR, ITERATOR PASSING
    auto idbFacIt = makeDB();
    filler.fillRandom(data);
    timer.start();
    for (unsigned int i = 0; i < benchmarkBufferSize; i++) {
        idbFacIt.storeFast(data.begin(), data.end());        
    }
    logBenchmark(logfile, "Fast store method | Constant vector | Iterator Passing", timer.stopMs(), benchmarkBufferSize, bytesPerSample);



    logfile.close();
}