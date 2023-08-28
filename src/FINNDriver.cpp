#include <concepts>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

// Helper
#include "core/Accelerator.h"
#include "core/DeviceBuffer.hpp"
#include "utils/FinnDatatypes.hpp"
#include "utils/Logger.h"

// Created by FINN during compilation
#include "config/config.h"

// XRT
#include "xrt/xrt_device.h"

using std::string;


template<typename T>
BOMemoryDefinitionArguments<T> toVariant(const shape_list_t& inp) {
    std::vector<std::variant<shape_t, MemoryMap<T>>> ret;
    ret.reserve(inp.size());

    std::transform(inp.begin(), inp.end(), std::back_inserter(ret), [](const shape_t& shape) { return std::variant<shape_t, MemoryMap<T>>(shape); });
    return ret;
}

int main() {
    auto logger = Logger::getLogger();
    FINN_LOG(logger, loglevel::info) << "C++ Driver started";
    FINN_LOG_DEBUG(logger, loglevel::info) << "Test";

    Finn::DeviceWrapper devWrap;
    devWrap.xclbin = "design.xclbin";
    devWrap.name = "SomeName";
    devWrap.idmas = Config::idmaNames;
    devWrap.odmas = Config::odmaNames;

    Finn::Accelerator acc(devWrap);


    auto myDevice = xrt::device();
    shape_t myShape = std::vector<unsigned int>{1, 2, 3};
    DatatypeInt<2> myDatatype = DatatypeInt<2>();
    Finn::DeviceBuffer<uint8_t, DatatypeInt<2>> dbuffer = Finn::DeviceBuffer<uint8_t, DatatypeInt<2>>("MyDeviceBuffer", myDevice, myShape, 100, IO::INPUT);

    // Example usage 1
    /*
    dbuffer[1] = myData;
    print(dbuffer[1])
    dbuffer.loadToMemory(1);
    dbuffer.syncToDevice(1)
    dbuffer.startRun(1, times=10)
    results = dbuffer.awaitRun()
    */

    // Example usage 2
    /*
    int index = 0;
    while (!dbuffer.isFull()) {
        dbuffer << myData[index];
        index++;
    }
    for (;;) {
        dbuffer.loadToMemory();
        dbuffer.syncToDevice();
        dbuffer.startRun();
        results += dbuffer.awaitRun();
        dbuffer << myData[index];
        index++;
    }
    */


    // Example usage 3
    // The buffer automatically executes data when the ring buffer is full, to use the first batch and then instatnly replace the last one with the new data
    /*
    dbuffer.setAutoExecute(true);
    for (auto dat : myData) {
        dbuffer << myData;
    }
    results = dbuffer.fetchStoredResults();
    */
    // Common operation in usage 2 AND 3: Internal pointer gets increased by one part for ever << operation.



    return 0;
}
