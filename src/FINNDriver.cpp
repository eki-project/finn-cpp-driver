// #include <concepts>
// #include <iostream>
// #include <numeric>
// #include <stdexcept>
#include <string>

// Helper
// #include "core/Accelerator.h"
#include "core/DeviceBuffer.hpp"
#include "utils/FinnDatatypes.hpp"
#include "utils/Logger.h"

// Created by FINN during compilation
// #include "config/config.h"

// XRT
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

using std::string;


/* template<typename T>
BOMemoryDefinitionArguments<T> toVariant(const shape_list_t& inp) {
    std::vector<std::variant<shape_t, MemoryMap<T>>> ret;
    ret.reserve(inp.size());

    std::transform(inp.begin(), inp.end(), std::back_inserter(ret), [](const shape_t& shape) { return std::variant<shape_t, MemoryMap<T>>(shape); });
    return ret;
} */

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

    // Set parameters
    const std::string filename = "bitfile/finn-accel.xclbin";


    // Load the device
    auto device = xrt::device(0);
    FINN_LOG(logger, loglevel::info) << "Device found.";

    // Debug print the BDF of the device
    auto bdfInfo = device.get_info<xrt::info::device::bdf>();
    FINN_LOG(logger, loglevel::info) << "BDF: " << bdfInfo;

    // Load xclbin for debug info on kernels
    auto xclbin = xrt::xclbin(filename);
    auto kernels = xclbin.get_kernels();
    for (auto&& knl : kernels) {
        FINN_LOG(logger, loglevel::info) << "Kernel: " << knl.get_name() << "\n";
        for (auto&& arg : knl.get_args()) {
            FINN_LOG(logger, loglevel::info) << "\t\t\tArg: " << arg.get_name() << " Size: " << arg.get_size() << "\n";
        }

        for (auto&& compUnit : knl.get_cus()) {
            FINN_LOG(logger, loglevel::info) << " \t\t\tCU: " << compUnit.get_name() << " Size: " << compUnit.get_size() << "\n";
        }
    }

    // Connect and load kernel
    auto uuid = device.load_xclbin("bitfile/finn-accel.xclbin");
    auto kern = xrt::kernel(device, uuid, "StreamingDataflowPartition_0:{idma0}", xrt::kernel::cu_access_mode::shared);
    FINN_LOG(logger, loglevel::info) << "Device successfully programmed! UUID: " << uuid;


    // Execution
    shape_t myShape = std::vector<unsigned int>{1, 300};
    shape_t myShapeFolded = std::vector<unsigned int>{1, 10, 30};
    shape_t myShapePacked = std::vector<unsigned int>{1, 10, 8};
    // Finn::DeviceBuffer<uint8_t, DatatypeInt<2>> dbuffer = Finn::DeviceBuffer<uint8_t, DatatypeInt<2>>("MyDeviceBuffer", myDevice, myShape, 100, IO::INPUT);

    auto mydb = Finn::DeviceInputBuffer<uint8_t, DatatypeInt<2>>("My Buffer", device, kern, myShape, myShapeFolded, myShapePacked, 100);
    std::cout << mydb.isBufferFull() << std::endl;

    auto data = std::vector<uint8_t>(mydb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    std::fill(data.begin(), data.end(), 12);

    mydb.store(data, 0, false);
    FINN_LOG(logger, loglevel::info) << "Storing data";
    mydb.loadMap(0, true);
    FINN_LOG(logger, loglevel::info) << "Syncing data";
    mydb.sync();
    FINN_LOG(logger, loglevel::info) << "Executing data";
    mydb.execute();

    FINN_LOG(logger, loglevel::info) << "Creating output buffer";
    auto myodb = Finn::DeviceOutputBuffer<uint8_t, DatatypeInt<2>>("Output Buffer", device, kern, myShape, myShapeFolded, myShapePacked, 100);

    myodb.read(1);
    myodb.archiveValidBufferParts();
    auto res = myodb.retrieveArchive();
    FINN_LOG(logger, loglevel::info) << "Reading output!";
    for (auto& resv : res[0]) {
        FINN_LOG(logger, loglevel::info) << resv;
    }

    // auto mydb = Finn::DeviceInputBuffer<uint8_t, DatatypeInt<2>>();


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
