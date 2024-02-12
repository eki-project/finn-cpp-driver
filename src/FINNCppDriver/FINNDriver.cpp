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

#include <algorithm>    // for generate
#include <chrono>       // for nanoseconds, ...
#include <cstdint>      // for uint64_t, uint8_t, ...
#include <exception>    // for exception
#include <filesystem>   // for path, exists
#include <iostream>     // for streamsize
#include <memory>       // for allocator_trai...
#include <random>       // for random_device, ...
#include <stdexcept>    // for invalid_argument
#include <string>       // for string
#include <tuple>        // for tuple
#include <type_traits>  // for remove_ref...
#include <utility>      // for move
#include <vector>       // for vector

// Helper
#include <FINNCppDriver/core/DeviceHandler.h>          // for DeviceHandler
#include <FINNCppDriver/utils/ConfigurationStructs.h>  // for Config
#include <FINNCppDriver/utils/FinnUtils.h>             // for logAndError
#include <FINNCppDriver/utils/Logger.h>                // for FINN_LOG, ...
#include <FINNCppDriver/utils/Types.h>                 // for shape_t

#include <FINNCppDriver/core/BaseDriver.hpp>    // IWYU pragma: keep
#include <FINNCppDriver/utils/DataPacking.hpp>  // for AutoReturnType
#include <boost/program_options.hpp>            // for variables_map
#include <xtensor/xadapt.hpp>                   // for adapt
#include <xtensor/xarray.hpp>                   // for xarray_ada...
#include <xtensor/xiterator.hpp>                // for operator==
#include <xtensor/xlayout.hpp>                  // for layout_type
#include <xtensor/xnpy.hpp>                     // for dump_npy, ...
#include <xtl/xclosure.hpp>                     // for closure_ty
#include <xtl/xiterator_base.hpp>               // for operator!=


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
 * @brief A simple helper function to create a Finn Driver from a given config file
 *
 * @tparam SynchronousInference true=Sync Mode; false=Async Mode
 * @param configFilePath
 * @param hostBufferSize
 * @return Finn::Driver
 */
template<bool SynchronousInference>
Finn::Driver<SynchronousInference> createDriverFromConfig(const std::filesystem::path& configFilePath, unsigned int hostBufferSize) {
    Finn::Driver<SynchronousInference> driver(configFilePath, hostBufferSize);
    driver.setBatchSize(hostBufferSize);
    driver.setForceAchieval(true);
    return driver;
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
    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Input element count " << std::to_string(elementcount);

    constexpr bool isInteger = InputFinnType().isInteger();
    if constexpr (isInteger) {
        using dtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;

        std::vector<dtype> testInputs(elementcount);

        std::random_device rndDevice;
        std::mt19937 mersenneEngine{rndDevice()};  // Generates random integers
        std::uniform_int_distribution<dtype> dist{static_cast<dtype>(InputFinnType().min()), static_cast<dtype>(InputFinnType().max())};

        auto gen = [&dist, &mersenneEngine]() { return dist(mersenneEngine); };

        constexpr size_t nTestruns = 1;
        std::chrono::nanoseconds sumRuntimeEnd2End;

        for (size_t i = 0; i < nTestruns; ++i) {
            // TODO(linusjun): Revert debug stuff
            // std::generate(testInputs.begin(), testInputs.end(), gen);
            std::fill(testInputs.begin(), testInputs.end(), 1);
            const auto start = std::chrono::high_resolution_clock::now();
            // volatile should stop the compiler from optimizing this code away
            volatile auto ret = baseDriver.inferSynchronous(testInputs.begin(), testInputs.end());
            const auto end = std::chrono::high_resolution_clock::now();

            sumRuntimeEnd2End += end - start;
        }

        std::chrono::nanoseconds sumRuntimePacking;

        for (size_t i = 0; i < nTestruns; ++i) {
            std::generate(testInputs.begin(), testInputs.end(), gen);
            const auto start = std::chrono::high_resolution_clock::now();
            // volatile should stop the compiler from optimizing this code away
            volatile auto packed = Finn::pack<InputFinnType>(testInputs.begin(), testInputs.end());
            const auto end = std::chrono::high_resolution_clock::now();

            sumRuntimePacking += end - start;
        }

        std::cout << "Avg. end2end latency: " << (static_cast<unsigned long>(sumRuntimeEnd2End.count()) / nTestruns) << "ns\n";
        std::cout << "Avg. end2end throughput: " << 1 / (static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::seconds>(sumRuntimeEnd2End).count()) / nTestruns) << " inferences/s\n";
        std::cout << "Avg. packing latency: " << (static_cast<unsigned long>(sumRuntimePacking.count()) / nTestruns) << "ns\n";


        // benchmark each step in call chain for int
    } else {
        std::vector<float> testInputs(elementcount);
        std::random_device rndDevice;
        std::mt19937 mersenneEngine{rndDevice()};  // Generates random integers
        std::uniform_real_distribution<float> dist{static_cast<float>(InputFinnType().min()), static_cast<float>(InputFinnType().max())};

        auto gen = [&dist, &mersenneEngine]() { return dist(mersenneEngine); };

        std::generate(testInputs.begin(), testInputs.end(), gen);

        constexpr size_t nTestruns = 1000;
        std::chrono::nanoseconds sumRuntimeEnd2End;

        for (size_t i = 0; i < nTestruns; ++i) {
            std::generate(testInputs.begin(), testInputs.end(), gen);
            const auto start = std::chrono::high_resolution_clock::now();
            // volatile should stop the compiler from optimizing this code away
            volatile auto ret = baseDriver.inferSynchronous(testInputs.begin(), testInputs.end());
            const auto end = std::chrono::high_resolution_clock::now();

            sumRuntimeEnd2End += end - start;
        }

        std::cout << "Avg. end2end latency: " << (static_cast<unsigned long>(sumRuntimeEnd2End.count()) / nTestruns) << "ns\n";
        std::cout << "Avg. end2end throughput: " << 1 / (static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::seconds>(sumRuntimeEnd2End).count()) / nTestruns) << " inferences/s\n";


        // benchmark each step in call chain for float
    }
}

constexpr size_t typeStringByteSizePos = 2;
/**
 * @brief Executes inference on the input file if input type is a floating point type
 * @attention This function does no checking of the datatype contained in the loadedNpyFile! Passing a npy file containing a non floating point type is UB.
 *
 * @param loadedNpyFile
 */
void inferFloatingPoint(Finn::Driver<true>& baseDriver, xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
    size_t sizePos = typeStringByteSizePos;
    int size = std::stoi(loadedNpyFile.m_typestring, &sizePos);
    if (size == 4) {
        // float
        auto xtensorArray = std::move(loadedNpyFile).cast<float, xt::layout_type::dynamic>();
        Finn::vector<float> vec(xtensorArray.begin(), xtensorArray.end());
        auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
        auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
        xt::dump_npy(outputFile, xarr);
    } else if (size == 8) {
        // double
        auto xtensorArray = std::move(loadedNpyFile).cast<double, xt::layout_type::dynamic>();
        Finn::vector<double> vec(xtensorArray.begin(), xtensorArray.end());
        auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
        auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
        xt::dump_npy(outputFile, xarr);
    } else {
        FinnUtils::logAndError<std::runtime_error>("Unsupported floating point type detected when loading input npy file!");
    }
}

/**
 * @brief Executes inference on the input file if input type is a signed integer type
 * @attention This function does no checking of the datatype contained in the loadedNpyFile! Passing a npy file containing a non signed integer type is UB.
 *
 * @param baseDriver
 * @param loadedNpyFile
 * @param outputFile
 */
void inferSignedInteger(Finn::Driver<true>& baseDriver, xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
    size_t sizePos = typeStringByteSizePos;
    int size = std::stoi(loadedNpyFile.m_typestring, &sizePos);
    if (size == 1) {
        // int8_t
        auto xtensorArray = std::move(loadedNpyFile).cast<int8_t, xt::layout_type::dynamic>();
        Finn::vector<int8_t> vec(xtensorArray.begin(), xtensorArray.end());
        auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
        auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
        xt::dump_npy(outputFile, xarr);
    } else if (size == 2) {
        // int16_t
        auto xtensorArray = std::move(loadedNpyFile).cast<int16_t, xt::layout_type::dynamic>();
        Finn::vector<int16_t> vec(xtensorArray.begin(), xtensorArray.end());
        auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
        auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
        xt::dump_npy(outputFile, xarr);
    } else if (size == 4) {
        // int32_t
        auto xtensorArray = std::move(loadedNpyFile).cast<int32_t, xt::layout_type::dynamic>();
        Finn::vector<int32_t> vec(xtensorArray.begin(), xtensorArray.end());
        auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
        auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
        xt::dump_npy(outputFile, xarr);
    } else if (size == 8) {
        // int64_t
        auto xtensorArray = std::move(loadedNpyFile).cast<int64_t, xt::layout_type::dynamic>();
        Finn::vector<int64_t> vec(xtensorArray.begin(), xtensorArray.end());
        auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
        auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
        xt::dump_npy(outputFile, xarr);
    } else {
        FinnUtils::logAndError<std::runtime_error>("Unsupported signed integer type detected when loading input npy file!");
    }
}

/**
 * @brief Executes inference on the input file if input type is a unsigned integer type
 * @attention This function does no checking of the datatype contained in the loadedNpyFile! Passing a npy file containing a non unsigned integer type is UB.
 *
 * @param baseDriver
 * @param loadedNpyFile
 * @param outputFile
 */
void inferUnsignedInteger(Finn::Driver<true>& baseDriver, xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
    size_t sizePos = typeStringByteSizePos;
    int size = std::stoi(loadedNpyFile.m_typestring, &sizePos);
    if (size == 1) {
        // uint8_t
        auto xtensorArray = std::move(loadedNpyFile).cast<uint8_t, xt::layout_type::dynamic>();
        Finn::vector<uint8_t> vec(xtensorArray.begin(), xtensorArray.end());
        auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
        auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
        xt::dump_npy(outputFile, xarr);
    } else if (size == 2) {
        // uint16_t
        auto xtensorArray = std::move(loadedNpyFile).cast<uint16_t, xt::layout_type::dynamic>();
        Finn::vector<uint16_t> vec(xtensorArray.begin(), xtensorArray.end());
        auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
        auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
        xt::dump_npy(outputFile, xarr);
    } else if (size == 4) {
        // uint32_t
        auto xtensorArray = std::move(loadedNpyFile).cast<uint32_t, xt::layout_type::dynamic>();
        Finn::vector<uint32_t> vec(xtensorArray.begin(), xtensorArray.end());
        auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
        auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
        xt::dump_npy(outputFile, xarr);
    } else if (size == 8) {
        // uint64_t
        auto xtensorArray = std::move(loadedNpyFile).cast<uint64_t, xt::layout_type::dynamic>();
        Finn::vector<uint64_t> vec(xtensorArray.begin(), xtensorArray.end());
        auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
        auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
        xt::dump_npy(outputFile, xarr);
    } else {
        FinnUtils::logAndError<std::runtime_error>("Unsupported floating point type detected when loading input npy file!");
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

    for (auto&& [inp, out] = std::tuple{inputFiles.begin(), outputFiles.begin()}; inp != inputFiles.end(); ++inp, ++out) {
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
            switch (loadedFile.m_typestring[1]) {
                case 'f': {
                    inferFloatingPoint(baseDriver, loadedFile, *out);
                    break;
                }
                case 'i': {
                    inferSignedInteger(baseDriver, loadedFile, *out);
                    break;
                }
                case 'b': {
                    auto xtensorArray = std::move(loadedFile).cast<bool, xt::layout_type::dynamic>();
                    Finn::vector<uint8_t> vec(xtensorArray.begin(), xtensorArray.end());
                    auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
                    auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
                    xt::dump_npy(*out, xarr);
                    break;
                }
                case 'u': {
                    inferUnsignedInteger(baseDriver, loadedFile, *out);
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
