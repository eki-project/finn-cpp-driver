// #include <concepts>
// #include <iostream>
// #include <numeric>
// #include <stdexcept>
#include <string>
#include <thread>
#include <functional>
#include <chrono>

// Helper
#include "core/BaseDriver.hpp"
#include "core/DeviceBuffer.hpp"
#include "utils/FinnDatatypes.hpp"
#include "utils/Logger.h"
#include <boost/program_options.hpp>

// Created by FINN during compilation
#include "config/FinnDriverUsedDatatypes.h"

// XRT
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

using std::string;

/**
 * @brief Start a thread that writes 100 random data samples to the device. Returns a thread that can be waited on
 * 
 * @param logger 
 * @param filler 
 * @param mydb 
 * @return std::thread 
 */
std::thread testStartWriteThread(logger_type& logger, FinnUtils::BufferFiller& filler, Finn::DeviceInputBuffer<uint8_t>& mydb) {
    FINN_LOG(logger, loglevel::info) << "Starting write thread";
    std::vector<uint8_t> data;
    data.resize(mydb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    return std::thread([&filler, &data, &mydb]() {
        for (size_t i = 0; i < 100; i++) {
            filler.fillRandom(data);
            while (!mydb.store(data));
        }
    });
}

/**
 * @brief Start a threas to execute and run the kernel 100 times. Returns a thread that can be waited on
 * 
 * @param logger 
 * @param mydb 
 * @return std::thread 
 */
std::thread testStartExecuteThread(logger_type& logger, Finn::DeviceInputBuffer<uint8_t>& mydb) {
    FINN_LOG(logger, loglevel::info) << "Starting execute thread";
    return std::thread([&mydb]() {
        for (size_t j = 0; j < 100; j++) {
            while (!mydb.run())
                ;
        }
    });
}

/**
 * @brief Start a thread to read 100 entries and write them into LTS. Returns a thread that can be waited on 
 * 
 * @param logger 
 * @param mydb 
 * @return std::thread 
 */
std::thread testStartReadThread(logger_type& logger, Finn::DeviceOutputBuffer<uint8_t>& myodb) {
    FINN_LOG(logger, loglevel::info) << "Starting read thread";
    return std::thread([&myodb]() {
        for (size_t k = 0; k < 100; k++) {
            myodb.read(1);
        }
    });
}

/**
 * @brief Run a manual write-read test with the FPGA to see if everything works 
 * 
 * @param logger 
 * @param mydb 
 * @param filler 
 * @param kern 
 * @param kernOut 
 */
void manualTest1(logger_type& logger, Finn::DeviceInputBuffer<uint8_t>& mydb, FinnUtils::BufferFiller& filler, xrt::device& device, xrt::kernel& kern, xrt::kernel& kernOut) {
    std::vector<uint8_t> data;
    data.resize(mydb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
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
}

/**
 * @brief Run a manual write-read test with the FPGA to see if everything works. Use the device buffers now
 * 
 * @param logger 
 * @param mydb 
 * @param filler 
 */
void manualTest2(logger_type& logger, Finn::DeviceInputBuffer<uint8_t>& mydb, Finn::DeviceOutputBuffer<uint8_t>& myodb) {
    std::vector<uint8_t> data;
    data.resize(mydb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    mydb.store(data);
    mydb.run();
    myodb.read(1);
    myodb.archiveValidBufferParts();
    auto rr = myodb.retrieveArchive();
    for (auto rrval : rr[0]) {
        FINN_LOG(logger, loglevel::info) << "TEST 2:  " << static_cast<unsigned int>(rrval);
    }
}

/**
 * @brief Log some initial information about the device and the kernels used
 * 
 * @param logger 
 * @param device 
 * @param filename 
 */
void logDeviceInformation(logger_type& logger, xrt::device& device, const std::string& filename) {
    auto bdfInfo = device.get_info<xrt::info::device::bdf>();
    FINN_LOG(logger, loglevel::info) << "BDF: " << bdfInfo;
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
}

namespace po = finnBoost::program_options;

int main(int argc, char* argv[]) {
    auto logger = Logger::getLogger();
    FINN_LOG(logger, loglevel::info) << "C++ Driver started";
    
    auto logMode = [&logger](std::string m) { FINN_LOG(logger, loglevel::info) << "Driver Mode: " << m; };
    auto logConfig = [&logger](std::string m) { FINN_LOG(logger, loglevel::info) << "Configuration file: " << m; };
    auto logInputFile = [&logger](std::string m) { FINN_LOG(logger, loglevel::info) << "Input file: " << m; };

    po::options_description desc{"Options"};
    desc.add_options()
        ("help,h", "Display help")
        ("mode,m", po::value<std::string>()->default_value("test")->notifier(logMode), "Mode of execution (file or test)")
        ("configpath,c", po::value<std::string>()->notifier(logConfig), "Path to the config.json file emitted by the FINN compiler")
        ("input,i", po::value<std::string>()->notifier(logInputFile), "Path to the input file. Only required if mode is set to \"file\"");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        FINN_LOG(logger, loglevel::info) << desc;
        return 1;
    }

    // Switch on modes
    if (vm.count("mode") && vm["mode"].as<std::string>() == "test") {
        // Set parameters
        const std::string filename = "bitfile/finn-accel.xclbin";
        const unsigned int runs = 20;
        shapeNormal_t myShape = std::vector<unsigned int>{1, 300};
        shapeFolded_t myShapeFolded = std::vector<unsigned int>{1, 10, 30};
        shapePacked_t myShapePacked = std::vector<unsigned int>{1, 10, 8};
        shapeNormal_t oMyShape = std::vector<unsigned int>{1, 10};
        shapeFolded_t oMyShapeFolded = std::vector<unsigned int>{1, 10, 1};
        shapePacked_t oMyShapePacked = std::vector<unsigned int>{1, 10, 1};

        // Set up device, kernels and buffers
        auto device = xrt::device(0);
        auto uuid = device.load_xclbin("bitfile/finn-accel.xclbin");
        auto kern = xrt::kernel(device, uuid, "StreamingDataflowPartition_0:{idma0}", xrt::kernel::cu_access_mode::shared);
        auto kernOut = xrt::kernel(device, uuid, "StreamingDataflowPartition_2:{odma0}", xrt::kernel::cu_access_mode::shared);
        FINN_LOG(logger, loglevel::info) << "Device successfully programmed! UUID: " << uuid;
        auto mydb = Finn::DeviceInputBuffer<uint8_t>("My Buffer", device, kern, myShapePacked, 100);
        auto myodb = Finn::DeviceOutputBuffer<uint8_t>("Output Buffer", device, kernOut, oMyShapePacked, runs);
        auto filler = FinnUtils::BufferFiller(0, 2);
        auto data = std::vector<uint8_t>(mydb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));

        logDeviceInformation(logger, device, filename);

        /***** TESTS ********/
        manualTest1(logger, mydb, filler, device, kern, kernOut);
        manualTest2(logger, mydb, myodb);

        /***** REAL TEST ****/
        auto start = std::chrono::high_resolution_clock::now();
        testStartWriteThread(logger, filler, mydb).join();
        auto end = std::chrono::high_resolution_clock::now();
        long int nsDurationWrite = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        start = std::chrono::high_resolution_clock::now();
        testStartExecuteThread(logger, mydb).join();
        end = std::chrono::high_resolution_clock::now();
        long int nsDurationExecute = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        start = std::chrono::high_resolution_clock::now();
        testStartReadThread(logger, myodb).join();
        end = std::chrono::high_resolution_clock::now();
        long int nsDurationRead = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        FINN_LOG(logger, loglevel::info) << "Write duration (ns): " << nsDurationWrite;
        FINN_LOG(logger, loglevel::info) << "Execute duration (ns): " << nsDurationExecute;
        FINN_LOG(logger, loglevel::info) << "Read duration (ns): " << nsDurationRead;


        // Readout
        auto res = myodb.retrieveArchive();
        FINN_LOG(logger, loglevel::info) << "Reading output!";
        for (size_t i = 0; i < runs; i++) {
            FINN_LOG(logger, loglevel::info) << "Reading output of run " << i;
            for (auto& resvalue : res[i]) {
                FINN_LOG(logger, loglevel::info) << static_cast<int>(resvalue);
            }
        }
        return 0;
    } else if (vm["mode"].as<std::string>() == "file") {
        if (!vm.count("input")) {
            FinnUtils::logAndError<std::invalid_argument>("No input file specified for file execution mode!");
        }

        auto inputFilePath = std::filesystem::path(vm["input"].as<std::string>());
        Finn::BaseDriver baseDriver = Finn::BaseDriver<InputFinnType, OutputFinnType, uint8_t>(inputFilePath, 100);


    } else {
        FinnUtils::logAndError<std::invalid_argument>("Unknown driver mode: " + vm["mode"].as<std::string>());
    }
}
