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

/*
    Finn::DeviceWrapper devWrap;
    devWrap.xclbin = "design.xclbin";
    devWrap.name = "SomeName";
    devWrap.idmas = Config::idmaNames;
    devWrap.odmas = Config::odmaNames;

    Finn::Accelerator acc(devWrap);
*/
    auto xclbin = xrt::xclbin("bitfile/finn-accel.xclbin");
    auto device = xrt::device(0);
    auto uuid = device.load_xclbin("bitfile/finn-accel.xclbin");
    auto kern = xrt::kernel(device, uuid, "idma0", xrt::kernel::cu_access_mode::shared);

    shape_t myShape = std::vector<unsigned int>{1, 300};
    shape_t myShapeFolded = std::vector<unsigned int>{1, 10, 30};
    shape_t myShapePacked = std::vector<unsigned int>{1, 10, 8};
    // Finn::DeviceBuffer<uint8_t, DatatypeInt<2>> dbuffer = Finn::DeviceBuffer<uint8_t, DatatypeInt<2>>("MyDeviceBuffer", myDevice, myShape, 100, IO::INPUT);

    auto mydb = Finn::DeviceInputBuffer<uint8_t,DatatypeInt<2>>("My Buffer", device, kern, myShape, myShapeFolded, myShapePacked, 100);
    std::cout << mydb.isBufferFull() << std::endl;

    auto data =  std::vector<uint8_t>(mydb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    std::fill(data.begin(), data.end(), 12);

    mydb.store(data, 0);
    FINN_LOG(logger, loglevel::info) << "Storing data";
    mydb.loadMap(0, true);
    FINN_LOG(logger, loglevel::info) << "Syncing data";
    mydb.sync();
    FINN_LOG(logger, loglevel::info) << "Executing data";
    mydb.execute();

    //auto mydb = Finn::DeviceInputBuffer<uint8_t, DatatypeInt<2>>();


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
        myData[index] >> dbuffer;
        index++;
    }
    */


    // Example usage 3
    // The buffer automatically executes data when the ring buffer is full, to use the first batch and then instatnly replace the last one with the new data
    /*
    dbuffer.setAutoExecute(true);
    for (auto dat : myData) {
        myData >> dbuffer;
    }
    results = dbuffer.fetchStoredResults();
    */
    // Common operation in usage 2 AND 3: Internal pointer gets increased by one part for ever << operation.


    return 0;
}
