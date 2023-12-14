/**
 * @file FinnDriver.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de), Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Main file for the pre packaged C++ FINN driver
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <cstdint>     // for uint8_t
#include <filesystem>  // for path, exists
#include <iosfwd>      // for streamsize
#include <memory>      // for allocator_trai...
#include <stdexcept>   // for invalid_argument
#include <string>
#include <vector>  // for vector

// Helper
#include <FINNCppDriver/core/DeviceHandler.h>          // for DeviceHandler
#include <FINNCppDriver/utils/ConfigurationStructs.h>  // for Config
#include <FINNCppDriver/utils/FinnUtils.h>             // for logAndError
#include <FINNCppDriver/utils/Logger.h>
#include <FINNCppDriver/utils/Types.h>  // for SIZE_SPECI...

#include <FINNCppDriver/core/BaseDriver.hpp>  // IWYU pragma: keep
#include <boost/program_options.hpp>


// Created by FINN during compilation
// Use the default testing Driver type when none is specified. Normally this is set by the FINN compiler and available together with the xclbin.
// NOLINTBEGIN
#define MSTR(x)    #x
#define STRNGFY(x) MSTR(x)
// NOLINTEND

#ifndef FINN_HEADER_LOCATION
    #include <FINNCppDriver/config/FinnDriverUsedDatatypes.h>
#else
    #include STRNGFY(FINN_HEADER_LOCATION)
#endif

// XRT
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

using std::string;

/**
 * @brief A short prefix usable with the logger to determine the source of the log write
 *
 * @return std::string
 */
std::string finnMainLogPrefix() { return "[FINNDriver] "; }

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

/**
 * @brief A simple helper function to create a Finn Driver from a given config file.
 *
 * @param configFilePath
 * @param hostBufferSize
 * @return Finn::Driver
 */
Finn::Driver createDriverFromConfig(const std::filesystem::path& configFilePath, unsigned int hostBufferSize, bool synchronousMode) { return {configFilePath, hostBufferSize, synchronousMode}; }

/**
 * @brief Run test inferences. The data used is generated randomly. Useful for testing functionality
 * @attention This does NOT FOLD or PACK. As such it does NOT count towards performance measuring
 *
 * @param baseDriver
 * @param logger
 */
void runFiletest(Finn::Driver& baseDriver, logger_type& logger) {
    // TODO(bwintermann): Remove after debugging
    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Device Information: ";
    logDeviceInformation(logger, baseDriver.getDeviceHandler(0).getDevice(), baseDriver.getConfig().deviceWrappers[0].xclbin);

    // Create vector for inputting data
    auto filler = FinnUtils::BufferFiller(0, 127);
    std::vector<uint8_t> data;
    data.resize(baseDriver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, "StreamingDataflowPartition_0:{idma0}"));

    // Do a test run with random data and raw inference (no packing no folding)
    filler.fillRandom(data);
    FinnUtils::logResults<uint8_t>(logger, data, 5000, "INPUT DATA: ");

    // auto results = baseDriver.infer(data, 0, "StreamingDataflowPartition_0:{idma0}", 0, "StreamingDataflowPartition_2:{odma0}", 9, true);
    // FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Received " << results.size() << " results!";

    // // Print Results
    // int counter = 0;
    // for (auto& resultVector : results) {
    //     FinnUtils::logResults<uint8_t>(logger, resultVector, 8, finnMainLogPrefix() + "Vec " + std::to_string(counter++));
    // }
}

/**
 * @brief Run a test inference and save input and output data in a file which can be checked for results
 *
 * @param baseDriver
 * @param logger
 */
void runIntegrationTest(Finn::Driver& baseDriver, logger_type& logger) {
    std::fstream resultfile("integration_test_outputs.txt", std::fstream::out);

    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Device Information: ";
    logDeviceInformation(logger, baseDriver.getDeviceHandler(0).getDevice(), baseDriver.getConfig().deviceWrappers[0].xclbin);

    // Create vector for inputting data
    auto filler = FinnUtils::BufferFiller(0, 127);
    std::vector<uint8_t> data;
    data.resize(baseDriver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, "StreamingDataflowPartition_0:{idma0}"));

    // Do a test run with random data and raw inference (no packing no folding)
    filler.fillRandom(data);
    // auto results = baseDriver.infer(data, 0, "StreamingDataflowPartition_0:{idma0}", 0, "StreamingDataflowPartition_2:{odma0}", 1, true);

    // // Write data to result file. One line per data, ending with an empty space and a newline
    // // TODO(bwintermann): Check if "uniq" registers the newline too
    // // TODO(bwintermann): Introduce checks for everything
    // for (auto val : data) {
    //     resultfile << val << " ";
    // }
    // resultfile << "\n";

    // for (auto val : results[0]) {
    //     resultfile << val << " ";
    // }
    // resultfile << "\n";
    // resultfile.close();
}

/**
 * @brief Run inference on an input file with folding and packing beforehand and unfolding and unpacking afterwards
 *
 * @param baseDriver
 * @param logger
 */
void runWithInputFile(Finn::Driver& baseDriver, logger_type& logger) {
    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Running driver on input files";
    // TODO(bwintermann): Finish this method

    // baseDriver.infer();
}


namespace po = finnBoost::program_options;

int main(int argc, char* argv[]) {
    auto logger = Logger::getLogger();
    FINN_LOG(logger, loglevel::info) << "C++ Driver started";

    // Helper lambas for debug output
    auto logMode = [&logger](const std::string& m) { FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Driver Mode: " << m; };
    auto logConfig = [&logger](const std::string& m) { FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Configuration file: " << m; };
    auto logInputFile = [&logger](const std::string& m) { FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Input file: " << m; };
    auto logHBufferSize = [&logger](const unsigned int m) { FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Host Buffer Size: " << m; };

    // Command Line Argument Parser
    po::options_description desc{"Options"};
    desc.add_options()("help,h", "Display help")("mode,m", po::value<std::string>()->default_value("test")->notifier(logMode), "Mode of execution (file or test)")("configpath,c", po::value<std::string>()->notifier(logConfig),
                                                                                                                                                                   "Path to the config.json file emitted by the FINN compiler")(
        "input,i", po::value<std::string>()->notifier(logInputFile), "Path to the input file. Only required if mode is set to \"file\"")("buffersize,b", po::value<unsigned int>()->notifier(logHBufferSize)->default_value(100),
                                                                                                                                         "How large (in samples) the host buffer is supposed to be");
    po::variables_map varMap;
    po::store(po::parse_command_line(argc, argv, desc), varMap);
    po::notify(varMap);
    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Parsed command line params";

    // Display help screen
    if (varMap.count("help") > 0) {
        FINN_LOG(logger, loglevel::info) << desc;
        return 1;
    }

    // Checking for the existance of the config file
    auto configFilePath = std::filesystem::path(varMap["configpath"].as<std::string>());
    if (!std::filesystem::exists(configFilePath)) {
        FinnUtils::logAndError<std::runtime_error>("Cannot find config file at " + configFilePath.string());
    }


    // Switch on modes
    if (varMap["mode"].as<std::string>() == "file") {
        if (!(varMap.count("input") > 0)) {
            FinnUtils::logAndError<std::invalid_argument>("No input file specified for file execution mode!");
        }
        auto driver = createDriverFromConfig(configFilePath, varMap["buffersize"].as<unsigned int>(), false);
        runWithInputFile(driver, logger);
    } else if (varMap["mode"].as<std::string>() == "test") {
        auto driver = createDriverFromConfig(configFilePath, varMap["buffersize"].as<unsigned int>(), true);
        runFiletest(driver, logger);
    } else {
        FinnUtils::logAndError<std::invalid_argument>("Unknown driver mode: " + varMap["mode"].as<std::string>());
    }
}
