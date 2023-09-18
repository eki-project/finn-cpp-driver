// #include <concepts>
// #include <iostream>
// #include <numeric>
// #include <stdexcept>
#include <string>
#include <thread>

// Helper
#include "core/BaseDriver.hpp"
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

int main() {
#undef NDEBUG
    auto logger = Logger::getLogger();
    FINN_LOG(logger, loglevel::info) << "C++ Driver started";
    FINN_LOG_DEBUG(logger, loglevel::info) << "Test";

    // For random number generation
    auto filler = FinnUtils::BufferFiller(0, 2);

    // Set parameters
    const std::string filename = "bitfile/finn-accel.xclbin";
    const unsigned int runs = 20;

    // Shape
    shapeNormal_t myShape = std::vector<unsigned int>{1, 300};
    shapeFolded_t myShapeFolded = std::vector<unsigned int>{1, 10, 30};
    shapePacked_t myShapePacked = std::vector<unsigned int>{1, 10, 8};
    shapeNormal_t oMyShape = std::vector<unsigned int>{1, 10};
    shapeFolded_t oMyShapeFolded = std::vector<unsigned int>{1, 10, 1};
    shapePacked_t oMyShapePacked = std::vector<unsigned int>{1, 10, 1};

    // Load the device
    auto device = xrt::device(0);

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
    auto kernOut = xrt::kernel(device, uuid, "StreamingDataflowPartition_2:{odma0}", xrt::kernel::cu_access_mode::shared);

    // Creating the devicebuffers
    auto mydb = Finn::DeviceInputBuffer<uint8_t>("My Buffer", device, kern, myShapePacked, 100);
    auto myodb = Finn::DeviceOutputBuffer<uint8_t>("Output Buffer", device, kernOut, oMyShapePacked, runs);
    auto data = std::vector<uint8_t>(mydb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));

    /***** MANUAL TEST ****/
    filler.fillRandom(data);                                     // Fill input buffer data
    auto x = xrt::bo(device, 4096, 0);                  // Create input buffer
    auto y = xrt::bo(device, 4096, 0);                  // Create output buffer
    x.write(data.data());                                       // Write test data into input buffer
    x.sync(xclBOSyncDirection::XCL_BO_SYNC_BO_TO_DEVICE);       // Sync test data to device
    auto myres = kern(x, 1);                            // Run the data into the kernel
    myres.wait();                                               // Wait for the kernel results
    auto myresout = kernOut(y, 1);                      // Run the output kernel
    myresout.wait();                                            // Wait for the output
    y.sync(xclBOSyncDirection::XCL_BO_SYNC_BO_FROM_DEVICE);     // Sync data from device back
    uint8_t* buf = new uint8_t[4096];                           // Create temporary buffer
    y.read(buf);                                                // Read data from output buffer into memory

    for (unsigned int i = 0; i < 100; i++) {
        FINN_LOG(logger, loglevel::info) << "TEST: " << static_cast<unsigned int>(buf[i]);
    }
    delete[] buf;


    /***** MANUAL TEST 2 ****/
    mydb.store(data);
    mydb.run();
    myodb.read(1);
    myodb.archiveValidBufferParts();
    auto rr = myodb.retrieveArchive();
    for (auto rrval : rr[0]) {
        FINN_LOG(logger, loglevel::info) << "TEST 2:  " << static_cast<unsigned int>(rrval);
    }

    /***** REAL TEST ****/
    FINN_LOG(logger, loglevel::info) << "Starting write thread";
    auto writeThread = std::thread([&filler, &data, &mydb]() {
        for (size_t i = 0; i < 100; i++) {
            filler.fillRandom(data);
            while (!mydb.store(data))
                ;
        }
    });

    FINN_LOG(logger, loglevel::info) << "Starting execute thread";
    auto executeThread = std::thread([&mydb]() {
        for (size_t j = 0; j < 100; j++) {
            while (!mydb.run())
                ;
        }
    });

    FINN_LOG(logger, loglevel::info) << "Starting read thread";
    auto readThread = std::thread([&myodb]() {
        for (size_t k = 0; k < 100; k++) {
            myodb.read(1);
        }
    });

    writeThread.join();
    executeThread.join();
    readThread.join();

    auto res = myodb.retrieveArchive();
    FINN_LOG(logger, loglevel::info) << "Reading output!";

    for (size_t i = 0; i < runs; i++) {
        FINN_LOG(logger, loglevel::info) << "Reading output of run " << i;
        for (auto& resvalue : res[i]) {
            FINN_LOG(logger, loglevel::info) << static_cast<int>(resvalue);
        }
    }
    return 0;
}
