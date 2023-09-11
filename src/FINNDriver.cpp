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

int main() {

    #undef NDEBUG



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

    // Preparation for throughput test
    std::random_device rd;
    std::mt19937 engine{rd()};
    std::uniform_int_distribution<uint8_t> sampler(0, 0xFF);

    // Set parameters
    const std::string filename = "bitfile/finn-accel.xclbin";
    const unsigned int runs = 20;


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

    // Shape data
    shape_t myShape = std::vector<unsigned int>{1, 300};
    shape_t myShapeFolded = std::vector<unsigned int>{1, 10, 30};
    shape_t myShapePacked = std::vector<unsigned int>{1, 10, 8};
    
    shape_t oMyShape = std::vector<unsigned int>{1, 10};
    shape_t oMyShapeFolded = std::vector<unsigned int>{1, 10, 1};
    shape_t oMyShapePacked = std::vector<unsigned int>{1, 10, 1};

    // Creating the devicebuffers
    auto mydb = Finn::DeviceInputBuffer<uint8_t, DatatypeInt<2>>("My Buffer", device, kern, myShape, myShapeFolded, myShapePacked, 100);
    std::cout << mydb.isBufferFull() << std::endl;

    auto myodb = Finn::DeviceOutputBuffer<uint8_t, DatatypeBinary>("Output Buffer", device, kern, oMyShape, oMyShapeFolded, oMyShapePacked, runs);
    
    auto data =  std::vector<uint8_t>(mydb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));

    // Multithreaded writing to the ring buffer and executing
    // TODO: Multithread into store - execute? Instead of threading _together_ store-execute, which dont necessarily regard the same data then?
    // Visualize it
    std::vector<std::thread> writeThreads;
    for (size_t i = 0; i < runs; i++) { 
        writeThreads.push_back(
            std::thread([&sampler, &engine, &data, &mydb](){
                std::transform(data.begin(), data.end(), data.begin(), [&sampler, &engine](uint8_t x){ return (x-x) + sampler(engine); });
                while (!mydb.store(data, false));
                mydb.loadMap();
                mydb.sync();
                mydb.execute();
            })
        );
    }

    // Wait for all write threads to finish
    for (auto& t : writeThreads) {
        t.join();
    }

    // Read results out
    myodb.read(runs);
    myodb.archiveValidBufferParts();
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
