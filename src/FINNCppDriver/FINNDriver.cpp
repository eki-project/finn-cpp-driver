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

#include <sys/types.h>  // for uint

#include <algorithm>
#include <exception>   // for exception
#include <filesystem>  // for path, exists
#include <iostream>    // for streamsize
#include <memory>      // for allocator_trai...
#include <random>
#include <stdexcept>  // for invalid_argument
#include <string>     // for string
#include <tuple>
#include <vector>  // for vector

// Helper
#include <FINNCppDriver/core/DeviceHandler.h>          // for DeviceHandler
#include <FINNCppDriver/utils/ConfigurationStructs.h>  // for Config
#include <FINNCppDriver/utils/FinnUtils.h>             // for logAndError
#include <FINNCppDriver/utils/Logger.h>                // for FINN_LOG, ...

#include <FINNCppDriver/core/BaseDriver.hpp>    // IWYU pragma: keep
#include <FINNCppDriver/utils/DataPacking.hpp>  // for AutoReturnType
#include <boost/program_options.hpp>
#include <xtensor/xnpy.hpp>


// Created by FINN during compilation
// Use the default testing Driver type when none is specified. Normally this is set by the FINN compiler and available together with the xclbin.
// NOLINTBEGIN
#define MSTR(x)    #x
#define STRNGFY(x) MSTR(x)
// NOLINTEND

#ifndef FINN_HEADER_LOCATION
    #include <FINNCppDriver/config/FinnDriverUsedDatatypes.h>  // IWYU pragma: keep
#else
    #include STRNGFY(FINN_HEADER_LOCATION)  // IWYU pragma: keep
#endif

// XRT
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

/**
 * @brief Configure ASanitizer to circumvent some buggy behavior with std::to_string
 *
 * @return const char*
 */
// NOLINTBEGIN
//  cppcheck-suppress unusedFunction
extern "C" const char* __asan_default_options() { return "detect_odr_violation=1"; }
// NOLINTEND

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
 * @tparam SynchronousInference true=Sync Mode; false=Async Mode
 * @param configFilePath
 * @param hostBufferSize
 * @return Finn::Driver
 */
template<bool SynchronousInference>
Finn::Driver<SynchronousInference> createDriverFromConfig(const std::filesystem::path& configFilePath, unsigned int hostBufferSize) {
    return {configFilePath, hostBufferSize};
}

/**
 * @brief Run a throughput test to test the performance of the driver
 *
 * @param baseDriver
 * @param logger
 */
void runThroughputTest(Finn::Driver<true>& baseDriver, logger_type& logger) {
    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Device Information: ";
    logDeviceInformation(logger, baseDriver.getDeviceHandler(0).getDevice(), baseDriver.getConfig().deviceWrappers[0].xclbin);

    size_t elementcount = FinnUtils::shapeToElements((std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].idmas[0]))->normalShape);

    constexpr bool isInteger = InputFinnType().isInteger();
    if constexpr (isInteger) {
        using dtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;

        std::vector<dtype> testInputs(elementcount);

        std::random_device rndDevice;
        std::mt19937 mersenneEngine{rndDevice()};  // Generates random integers
        std::uniform_int_distribution<dtype> dist{static_cast<dtype>(InputFinnType().min()), static_cast<dtype>(InputFinnType().max())};

        auto gen = [&dist, &mersenneEngine]() { return dist(mersenneEngine); };

        std::generate(testInputs.begin(), testInputs.end(), gen);

        // benchmark each step in call chain for int
    } else {
        std::vector<float> testInputs(elementcount);
        std::random_device rndDevice;
        std::mt19937 mersenneEngine{rndDevice()};  // Generates random integers
        std::uniform_real_distribution<float> dist{static_cast<float>(InputFinnType().min()), static_cast<float>(InputFinnType().max())};

        auto gen = [&dist, &mersenneEngine]() { return dist(mersenneEngine); };

        std::generate(testInputs.begin(), testInputs.end(), gen);

        // benchmark each step in call chain for float
    }
}

/**
 * @brief Run inference on an input file
 *
 * @param baseDriver
 * @param logger
 */
void runWithInputFile(Finn::Driver<true>& baseDriver, logger_type& logger, const std::vector<std::string>& inputFiles, const std::vector<std::string>& outputFiles) {
    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Running driver on input files";
    logDeviceInformation(logger, baseDriver.getDeviceHandler(0).getDevice(), baseDriver.getConfig().deviceWrappers[0].xclbin);

    for (auto [inp, out] = std::tuple{inputFiles.begin(), outputFiles.begin()}; inp != inputFiles.end(); ++inp, ++out) {
        // load npy file and process it
        // using normal xnpy::load_npy will not work because it requires a destination type
        // instead use xnpy::detail::load_npy_file und then concert by hand based on m_typestring of xnpy::detail::npy_file
        std::ifstream stream(*inp, std::ifstream::binary);
        if (!stream) {
            FinnUtils::logAndError<std::runtime_error>("io error: failed to open a file.");
        }

        auto loadedFile = xt::detail::load_npy_file(stream);

        if (loadedFile.m_typestring[0] == '<') {
            // little endian
            size_t sizePos = 2;
            switch (loadedFile.m_typestring[1]) {
                case 'f': {
                    int size = std::stoi(loadedFile.m_typestring, &sizePos);
                    if (size == 4) {
                        // float
                        auto xtensorArray = std::move(loadedFile).cast<float, xt::layout_type::dynamic>();
                        Finn::vector<float> vec(xtensorArray.begin(), xtensorArray.end());
                        baseDriver.inferSynchronous(vec.begin(), vec.end());
                    } else if (size == 8) {
                        // double
                        auto xtensorArray = std::move(loadedFile).cast<double, xt::layout_type::dynamic>();
                    } else {
                        FinnUtils::logAndError<std::runtime_error>("Unsupported floating point type detected when loading input npy file!");
                    }
                    break;
                }
                case 'i': {
                    int size = std::stoi(loadedFile.m_typestring, &sizePos);
                    break;
                }
                case 'b': {
                    break;
                }
                case 'u': {
                    int size = std::stoi(loadedFile.m_typestring, &sizePos);
                    break;
                }
                default:
                    std::string errorString = "Loading a numpy array with type identifier string ";
                    errorString += loadedFile.m_typestring[1];
                    errorString += " is currently not supported.";
                    FinnUtils::logAndError<std::runtime_error>(errorString);
            }
        } else {
            // all other endians
            FinnUtils::logAndError<std::runtime_error>("At the moment only files created on little endian systems are supported!\n");
        }
    }
}


void validateDriverMode(const std::string& mode) {
    if (mode != "execute" && mode != "throughput") {
        throw finnBoost::program_options::error_with_option_name("'" + mode + "' is not a valid driver mode!", "exec_mode");
    }

    FINN_LOG(Logger::getLogger(), loglevel::info) << finnMainLogPrefix() << "Driver Mode: " << mode;
}

void validateBatchSize(int batch) {
    if (batch <= 0) {
        throw finnBoost::program_options::error_with_option_name("Batch size must be positive, but is '" + std::to_string(batch) + "'", "batchsize");
    }
}

void validateConfigPath(const std::string& path) {
    auto configFilePath = std::filesystem::path(path);
    if (!std::filesystem::exists(configFilePath)) {
        throw finnBoost::program_options::error_with_option_name("Cannot find config file at " + configFilePath.string(), "configpath");
    }

    FINN_LOG(Logger::getLogger(), loglevel::info) << finnMainLogPrefix() << "Config file found at " + configFilePath.string();
}

void validateInputPath(const std::vector<std::string>& path) {
    for (auto&& elem : path) {
        auto inputFilePath = std::filesystem::path(elem);
        if (!std::filesystem::exists(inputFilePath)) {
            throw finnBoost::program_options::error_with_option_name("Cannot find input file at " + inputFilePath.string());
        }
        FINN_LOG(Logger::getLogger(), loglevel::info) << finnMainLogPrefix() << "Input file found at " + inputFilePath.string();
    }
}


namespace po = finnBoost::program_options;

int main(int argc, char* argv[]) {
    auto logger = Logger::getLogger();
    FINN_LOG(logger, loglevel::info) << "C++ Driver started";

    try {
        // Command Line Argument Parser
        po::options_description desc{"Options"};
        //clang-format off
        desc.add_options()("help,h", "Display help")("exec_mode,e", po::value<std::string>()->default_value("throughput")->notifier(&validateDriverMode),
                                                     R"(Please select functional verification ("execute") or throughput test ("throughput")")("configpath,c", po::value<std::string>()->required()->notifier(&validateConfigPath),
                                                                                                                                              "Required: Path to the config.json file emitted by the FINN compiler")(
            "input,i", po::value<std::vector<std::string>>()->multitoken()->composing()->notifier(&validateInputPath), "Path to one or more input files (npy format). Only required if mode is set to \"file\"")(
            "output,o", po::value<std::vector<std::string>>()->multitoken()->composing(), "Path to one or more output files (npy format). Only required if mode is set to \"file\"")(
            "batchsize,b", po::value<int>()->default_value(1)->notifier(&validateBatchSize), "Number of samples for inference");
        //clang-format on
        po::variables_map varMap;
        po::store(po::parse_command_line(argc, argv, desc), varMap);

        // Display help screen
        // Help option has to be processed before po::notify call to not enforce required options in combination with help
        if (varMap.count("help") != 0) {
            FINN_LOG(logger, loglevel::info) << desc;
            return 1;
        }

        po::notify(varMap);

        FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Parsed command line params";

        // Switch on modes
        if (varMap["exec_mode"].as<std::string>() == "execute") {
            if (varMap.count("input") == 0) {
                FinnUtils::logAndError<std::invalid_argument>("No input file(s) specified for file execution mode!");
            }
            if (varMap.count("output") == 0) {
                FinnUtils::logAndError<std::invalid_argument>("No output file(s) specified for file execution mode!");
            }
            if (varMap.count("input") != varMap.count("output")) {
                FinnUtils::logAndError<std::invalid_argument>("Same amount of input and output files required!");
            }
            auto driver = createDriverFromConfig<true>(varMap["configpath"].as<std::string>(), static_cast<uint>(varMap["batchsize"].as<int>()));
            runWithInputFile(driver, logger, varMap["input"].as<std::vector<std::string>>(), varMap["output"].as<std::vector<std::string>>());
        } else if (varMap["exec_mode"].as<std::string>() == "throughput") {
            auto driver = createDriverFromConfig<true>(varMap["configpath"].as<std::string>(), static_cast<uint>(varMap["batchsize"].as<int>()));
            runThroughputTest(driver, logger);
        } else {
            FinnUtils::logAndError<std::invalid_argument>("Unknown driver mode: " + varMap["exec_mode"].as<std::string>());
        }

        return 1;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 0;
    } catch (...)  // Catch everything that is not an exception class
    {
        std::cerr << "Unknown error!"
                  << "\n";
        return 0;
    }
}
