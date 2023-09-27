// #include <concepts>
// #include <iostream>
// #include <numeric>
// #include <stdexcept>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

// Helper
#include <boost/program_options.hpp>

#include "core/BaseDriver.hpp"
#include "core/DeviceBuffer.hpp"
#include "utils/FinnDatatypes.hpp"
#include "utils/Logger.h"

// Created by FINN during compilation
#include "config/FinnDriverUsedDatatypes.h"

// XRT
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

using std::string;

/**
 * @brief A short prefix usable with the logger to determine the source of the log write 
 * 
 * @return std::string 
 */
std::string finnMainLogPrefix() {
    return "[FINNDriver] ";
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

/**
 * @brief A simple helper function to create a Finn Driver from a given config file.
 *
 * @param configFilePath
 * @param hostBufferSize
 * @return Finn::Driver
 */
Finn::Driver createDriverFromConfig(const std::filesystem::path& configFilePath, unsigned int hostBufferSize) { return {configFilePath, hostBufferSize}; }

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
    auto filler = FinnUtils::BufferFiller(0, 2);
    std::vector<uint8_t> data;
    data.resize(baseDriver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, "StreamingDataflowPartition_0:{idma0}"));

    // Do a test run with random data and raw inference (no packing no folding)
    filler.fillRandom(data);
    FinnUtils::logResults<uint8_t>(logger, data, 5000, "INPUT DATA: ");

    auto results = baseDriver.inferRaw(data, 0, "StreamingDataflowPartition_0:{idma0}", 0, "StreamingDataflowPartition_2:{odma0}", 9, true);
    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Received " << results.size() << " results!";

    // Print Results
    int counter = 0;
    for (auto& resultVector : results) {
        FinnUtils::logResults<uint8_t>(logger, resultVector, 8, finnMainLogPrefix() + "Vec " + std::to_string(counter++)); 
    }
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
        auto driver = createDriverFromConfig(configFilePath, varMap["buffersize"].as<unsigned int>());
        runWithInputFile(driver, logger);
    } else if (varMap["mode"].as<std::string>() == "test") {
        auto driver = createDriverFromConfig(configFilePath, varMap["buffersize"].as<unsigned int>());
        runFiletest(driver, logger);
    } else {
        FinnUtils::logAndError<std::invalid_argument>("Unknown driver mode: " + varMap["mode"].as<std::string>());
    }
}
