/**
 * @file FINNDriver.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de), Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Main file for the pre packaged C++ FINN driver
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <algorithm>    // for generate
#include <chrono>       // for nanoseconds, ...
#include <cstddef>      // for size_t
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
#include <FINNCppDriver/utils/DoNotOptimize.h>         // for DoNotOptimize
#include <FINNCppDriver/utils/FinnUtils.h>             // for logAndError
#include <FINNCppDriver/utils/Types.h>                 // for shape_t

#include <FINNCppDriver/core/BaseDriver.hpp>      // IWYU pragma: keep
#include <FINNCppDriver/utils/DataPacking.hpp>    // for AutoReturnType
#include <FINNCppDriver/utils/DynamicMdSpan.hpp>  // for DynamicMdSpan
#include <FINNCppDriver/utils/Logger.hpp>         // for FINN_LOG, ...
#include <ext/alloc_traits.h>                     // for __alloc_tr...
#include <popl.hpp>                               // for program options
#include <xtensor/containers/xadapt.hpp>          // for adapt
#include <xtensor/containers/xarray.hpp>          // for xarray_ada...
#include <xtensor/core/xiterator.hpp>             // for operator==
#include <xtensor/core/xlayout.hpp>               // for layout_type
#include <xtensor/io/xnpy.hpp>                    // for dump_npy, ...
#include <xtl/xiterator_base.hpp>                 // for operator!=


// Created by FINN during compilation
// Use the default testing Driver type when none is specified.
/**
 * @brief Converts CMake definition into string
 *
 */
// NOLINTBEGIN
#define MSTR(x) #x
/**
 * @brief Converts CMake definition into string
 *
 */
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
void logDeviceInformation(xrt::device& device, const std::string& filename) {
    auto bdfInfo = device.get_info<xrt::info::device::bdf>();
    FINN_LOG(loglevel::info) << "BDF: " << bdfInfo;
    auto xclbin = xrt::xclbin(filename);
    auto kernels = xclbin.get_kernels();

    for (auto&& knl : kernels) {
        FINN_LOG(loglevel::info) << "Kernel: " << knl.get_name() << "\n";
        for (auto&& arg : knl.get_args()) {
            FINN_LOG(loglevel::info) << "\t\t\tArg: " << arg.get_name() << " Size: " << arg.get_size() << "\n";
        }

        for (auto&& compUnit : knl.get_cus()) {
            FINN_LOG(loglevel::info) << " \t\t\tCU: " << compUnit.get_name() << " Size: " << compUnit.get_size() << "\n";
        }
    }
}

/**
 * @brief A simple helper function to create a Finn Driver from a given config file
 *
 * @tparam SynchronousInference true=Sync Mode; false=Async Mode
 * @param configFilePath
 * @param batchSize
 * @return Finn::Driver
 */
template<bool SynchronousInference>
Finn::Driver<SynchronousInference> createDriverFromConfig(const std::filesystem::path& configFilePath, unsigned int batchSize) {
    return Finn::Driver<SynchronousInference>(configFilePath, batchSize);
}

template<typename O>
using destribution_t = typename std::conditional_t<std::is_same_v<O, float>, std::uniform_real_distribution<O>, std::uniform_int_distribution<O>>;

/**
 * @brief Implementation function for running throughput tests
 *
 * @tparam T Data type for the test inputs
 * @param baseDriver Reference to the FINN driver
 * @param elementCount Number of elements in test data
 * @param batchSize Batch size for inference
 */
template<typename T>
void runThroughputTestImpl(Finn::Driver<true>& baseDriver, std::size_t elementCount, uint batchSize) {
    using dtype = T;
    Finn::vector<dtype> testInputs(elementCount * batchSize);

    std::random_device rndDevice;
    std::mt19937 mersenneEngine{rndDevice()};  // Generates random integers

    destribution_t<dtype> dist{static_cast<dtype>(InputFinnType().min()), static_cast<dtype>(InputFinnType().max())};

    auto gen = [&dist, &mersenneEngine]() { return dist(mersenneEngine); };

    constexpr size_t nTestruns = 5000;
    std::chrono::duration<double> sumRuntimeEnd2End{};

    // Warmup
    std::fill(testInputs.begin(), testInputs.end(), 1);
    auto warmup = baseDriver.inferSynchronous(testInputs.begin(), testInputs.end());
    Finn::DoNotOptimize(warmup);

    for (size_t i = 0; i < nTestruns; ++i) {
        std::generate(testInputs.begin(), testInputs.end(), gen);
        const auto start = std::chrono::high_resolution_clock::now();
        auto ret = baseDriver.inferSynchronous(testInputs.begin(), testInputs.end());
        Finn::DoNotOptimize(ret);
        const auto end = std::chrono::high_resolution_clock::now();

        sumRuntimeEnd2End += (end - start);
    }

    std::chrono::duration<double> sumRuntimePacking{};
    std::chrono::duration<double> sumRuntimeUnpacking{};
    std::chrono::duration<double> sumRuntimeReshaping{};

    for (size_t i = 0; i < nTestruns; ++i) {
        std::generate(testInputs.begin(), testInputs.end(), gen);
        const auto start = std::chrono::high_resolution_clock::now();
        static auto foldedShape = static_cast<Finn::ExtendedBufferDescriptor*>(baseDriver.getConfig().deviceWrappers[0].idmas[0].get())->foldedShape;
        foldedShape[0] = batchSize;
        const Finn::DynamicMdSpan reshapedInput(testInputs.begin(), testInputs.end(), foldedShape);
        const auto reshape = std::chrono::high_resolution_clock::now();
        auto packed = Finn::packMultiDimensionalInputs<InputFinnType>(testInputs.begin(), testInputs.end(), reshapedInput, foldedShape.back());
        Finn::DoNotOptimize(packed);
        const auto end = std::chrono::high_resolution_clock::now();

        sumRuntimeReshaping += (reshape - start);
        sumRuntimePacking += (end - reshape);
    }

    auto packedOutput = baseDriver.getConfig().deviceWrappers[0].odmas[0]->packedShape;
    packedOutput[0] = batchSize;
    std::vector<uint8_t> unpackingInputs(FinnUtils::shapeToElements(packedOutput));
    for (size_t i = 0; i < nTestruns; ++i) {
        const auto start = std::chrono::high_resolution_clock::now();
        auto foldedOutput = static_cast<Finn::ExtendedBufferDescriptor*>(baseDriver.getConfig().deviceWrappers[0].odmas[0].get())->foldedShape;
        foldedOutput[0] = batchSize;
        const Finn::DynamicMdSpan reshapedOutput(unpackingInputs.begin(), unpackingInputs.end(), packedOutput);
        auto unpacked = Finn::unpackMultiDimensionalOutputs<OutputFinnType>(unpackingInputs.begin(), unpackingInputs.end(), reshapedOutput, foldedOutput);
        Finn::DoNotOptimize(unpacked);
        const auto end = std::chrono::high_resolution_clock::now();
        sumRuntimeUnpacking += (end - start);
    }

    std::cout << "Avg. end2end latency: " << (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeEnd2End).count()) / nTestruns / 1000) << "us\n";
    std::cout << "Avg. end2end throughput: " << 1 / (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeEnd2End).count()) / nTestruns / batchSize / 1000 / 1000 / 1000) << " inferences/s\n";
    std::cout << "Avg. packing latency: " << (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimePacking).count()) / nTestruns) << "ns\n";
    std::cout << "Avg. folding latency: " << (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeReshaping).count()) / nTestruns) << "ns\n";
    std::cout << "Avg. unpacking latency: " << (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeUnpacking).count()) / nTestruns) << "ns\n";
    std::cout << "Avg. raw inference latency:"
              << (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeEnd2End).count()) / nTestruns) -
                     (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimePacking).count()) / nTestruns) -
                     (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeReshaping).count()) / nTestruns) -
                     (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeUnpacking).count()) / nTestruns)
              << "ns\n";
}

/**
 * @brief Run a throughput test to test the performance of the driver
 *
 * @param baseDriver
 * @param logger
 */
void runThroughputTest(Finn::Driver<true>& baseDriver) {
    FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Device Information: ";
    logDeviceInformation(baseDriver.getDeviceHandler(0).getDevice(), baseDriver.getConfig().deviceWrappers[0].xclbin);

    size_t elementcount = FinnUtils::shapeToElements((std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].idmas[0]))->normalShape);
    uint batchSize = baseDriver.getBatchSize();
    FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Input element count " << std::to_string(elementcount);
    FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Batch size: " << batchSize;

    constexpr bool isInteger = InputFinnType().isInteger();
    if constexpr (isInteger) {
        using dtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;
        runThroughputTestImpl<dtype>(baseDriver, elementcount, batchSize);
        // benchmark each step in call chain for int
    } else {
        runThroughputTestImpl<float>(baseDriver, elementcount, batchSize);
    }
}

/**
 * @brief Load data from numpy file, run inference, and dump results
 *
 * @tparam T Data type for the loaded data
 * @param baseDriver Reference to the FINN driver
 * @param loadedNpyFile Loaded numpy file containing input data
 * @param outputFile Path to output file for results
 */
template<typename T>
void loadInferDump(Finn::Driver<true>& baseDriver, xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
    auto xtensorArray = std::move(loadedNpyFile).cast<T, xt::layout_type::dynamic>();
    Finn::vector<T> vec(xtensorArray.begin(), xtensorArray.end());
    auto ret = baseDriver.inferSynchronous(vec.begin(), vec.end());
    auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(baseDriver.getConfig().deviceWrappers[0].odmas[0]))->normalShape);
    xt::dump_npy(outputFile, xarr);
}

/**
 * @brief Index position in string that contains the byte size of the datatype stored in the numpy input file
 *
 */
constexpr size_t typeStringByteSizePos = 2;
/**
 * @brief Executes inference on the input file if input type is a floating point type
 * @attention This function does no checking of the datatype contained in the loadedNpyFile! Passing a npy file containing a non floating point type is UB.
 *
 * @param loadedNpyFile Input file
 * @param baseDriver Reference to driver used for inference
 * @param outputFile Name of output file
 */
void inferFloatingPoint(Finn::Driver<true>& baseDriver, xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
    size_t sizePos = typeStringByteSizePos;
    int size = std::stoi(loadedNpyFile.m_typestring, &sizePos);
    if (size == 4) {
        // float
        loadInferDump<float>(baseDriver, loadedNpyFile, outputFile);
    } else if (size == 8) {
        // double
        loadInferDump<double>(baseDriver, loadedNpyFile, outputFile);
    } else {
        Finn::logAndError<std::runtime_error>("Unsupported floating point type detected when loading input npy file!");
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
        loadInferDump<int8_t>(baseDriver, loadedNpyFile, outputFile);
    } else if (size == 2) {
        // int16_t
        loadInferDump<int16_t>(baseDriver, loadedNpyFile, outputFile);
    } else if (size == 4) {
        // int32_t
        loadInferDump<int32_t>(baseDriver, loadedNpyFile, outputFile);
    } else if (size == 8) {
        // int64_t
        loadInferDump<int64_t>(baseDriver, loadedNpyFile, outputFile);
    } else {
        Finn::logAndError<std::runtime_error>("Unsupported signed integer type detected when loading input npy file!");
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
        loadInferDump<uint8_t>(baseDriver, loadedNpyFile, outputFile);
    } else if (size == 2) {
        // uint16_t
        loadInferDump<uint16_t>(baseDriver, loadedNpyFile, outputFile);
    } else if (size == 4) {
        // uint32_t
        loadInferDump<uint32_t>(baseDriver, loadedNpyFile, outputFile);
    } else if (size == 8) {
        // uint64_t
        loadInferDump<uint64_t>(baseDriver, loadedNpyFile, outputFile);
    } else {
        Finn::logAndError<std::runtime_error>("Unsupported floating point type detected when loading input npy file!");
    }
}

/**
 * @brief Run inference on an input file
 *
 * @param baseDriver Reference to driver
 * @param logger Logger to be used
 * @param inputFiles Files used for inference input
 * @param outputFiles Filenames used for output files
 */
void runWithInputFile(Finn::Driver<true>& baseDriver, const std::vector<std::string>& inputFiles, const std::vector<std::string>& outputFiles) {
    FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Running driver on input files";
    logDeviceInformation(baseDriver.getDeviceHandler(0).getDevice(), baseDriver.getConfig().deviceWrappers[0].xclbin);

    for (auto&& [inp, out] = std::tuple{inputFiles.begin(), outputFiles.begin()}; inp != inputFiles.end(); ++inp, ++out) {
        // load npy file and process it
        // using normal xnpy::load_npy will not work because it requires a destination type
        // instead use xnpy::detail::load_npy_file und then concert by hand based on m_typestring of xnpy::detail::npy_file
        std::ifstream stream(*inp, std::ifstream::binary);
        if (!stream) {
            Finn::logAndError<std::runtime_error>("io error: failed to open a file.");
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
                    Finn::logAndError<std::runtime_error>(errorString);
            }
        } else {
            // all other endians
            Finn::logAndError<std::runtime_error>("At the moment only files created on little endian systems are supported!\n");
        }
    }
}

/**
 * @brief Main entrypoint for the frontend of the C++ Finn driver
 *
 * @param argc Number of command line parameters
 * @param argv Array of command line parameters
 * @return int Exit status code
 */
int main(int argc, char* argv[]) {
    Logger::initLogger();
    FINN_LOG(loglevel::info) << "C++ Driver started";

    try {
        // Command Line Argument Parser
        popl::OptionParser options("Options");

        auto help_option = options.add<popl::Switch>("h", "help", "Display help");
        auto mode_option = options.add<popl::Value<std::string>>("e", "exec_mode", R"(Please select functional verification ("execute") or throughput test ("throughput")", "throughput");
        auto config_option = options.add<popl::Value<std::string>>("c", "configpath", "Required: Path to the config.json file emitted by the FINN compiler");
        auto input_option = options.add<popl::Value<std::string>>("i", "input", "Path to one or more input files (npy format). Only required if mode is set to \"file\"");
        auto output_option = options.add<popl::Value<std::string>>("o", "output", "Path to one or more output files (npy format). Only required if mode is set to \"file\"");
        auto batch_option = options.add<popl::Value<unsigned>>("b", "batchsize", "Number of samples for inference", 1);
        auto check_option = options.add<popl::Switch>("", "check", "Outputs the compile time configuration");

        options.parse(argc, argv);

        // Display help screen
        if (help_option->is_set()) {
            std::cout << options << "\n";
            return 0;
        }

        if (check_option->is_set()) {
            std::cout << "input_t: " << Finn::type_name<InputFinnType>() << "\n";
            std::cout << "output_t: " << Finn::type_name<OutputFinnType>() << "\n";
            return 0;
        }


        if (mode_option->count() > 1) {
            throw std::runtime_error("Command Line Argument Error: exec_mode can only be set once!");
        }

        std::string mode = mode_option->value();

        if (mode != "execute" && mode != "throughput") {
            throw std::runtime_error("Command Line Argument Error:'" + mode + "' is not a valid driver mode!");
        }

        FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Driver Mode: " << mode;


        if (config_option->is_set()) {
            if (config_option->count() != 1) {
                throw std::runtime_error("Command Line Argument Error: configpath can only be set once!");
            }

            auto configFilePath = std::filesystem::path(config_option->value());
            if (!std::filesystem::exists(configFilePath)) {
                throw std::runtime_error("Command Line Argument Error: Cannot find config file at " + configFilePath.string());
            }

            FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Config file found at " << configFilePath.string();
        } else {
            throw std::runtime_error("Command Line Argument Error: configpath is required to be set!");
        }

        if (input_option->is_set()) {
            for (size_t i = 0; i < input_option->count(); ++i) {
                std::string elem = input_option->value(i);
                auto inputFilePath = std::filesystem::path(elem);
                if (!std::filesystem::exists(inputFilePath)) {
                    throw std::runtime_error("Command Line Argument Error: Cannot find input file at " + inputFilePath.string());
                }
                FINN_LOG_DEBUG(loglevel::info) << finnMainLogPrefix() << "Input file found at " << inputFilePath.string();
            }
        }

        FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Parsed command line params";

        // Switch on modes
        if (mode_option->value() == "execute") {
            if (!input_option->is_set()) {
                Finn::logAndError<std::invalid_argument>("No input file(s) specified for file execution mode!");
            }
            if (!output_option->is_set()) {
                Finn::logAndError<std::invalid_argument>("No output file(s) specified for file execution mode!");
            }
            if (input_option->count() != output_option->count()) {
                Finn::logAndError<std::invalid_argument>("Same amount of input and output files required!");
            }

            std::vector<std::string> inputVec;
            for (size_t i = 0; i < input_option->count(); ++i) {
                inputVec.emplace_back(input_option->value(i));
            }
            std::vector<std::string> outputVec;
            for (size_t i = 0; i < output_option->count(); ++i) {
                outputVec.emplace_back(output_option->value(i));
            }

            auto driver = createDriverFromConfig<true>(config_option->value(), batch_option->value());
            runWithInputFile(driver, inputVec, outputVec);
        } else if (mode_option->value() == "throughput") {
            auto driver = createDriverFromConfig<true>(config_option->value(), batch_option->value());
            runThroughputTest(driver);
        } else {
            Finn::logAndError<std::invalid_argument>("Unknown driver mode: " + mode_option->value());
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