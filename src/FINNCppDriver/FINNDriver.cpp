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
#include <functional>
#include <omp.h>

 // Helper
#include <FINNCppDriver/core/DeviceHandler.h>          // for DeviceHandler
#include <FINNCppDriver/utils/ConfigurationStructs.h>  // for Config
#include <FINNCppDriver/utils/DoNotOptimize.h>         // for DoNotOptimize
#include <FINNCppDriver/utils/FinnUtils.h>             // for logAndError
#include <FINNCppDriver/utils/Logger.h>                // for FINN_LOG, ...
#include <FINNCppDriver/utils/Types.h>                 // for shape_t

#include <FINNCppDriver/core/BaseDriver.hpp>      // IWYU pragma: keep
#include <FINNCppDriver/utils/DataPacking.hpp>    // for AutoReturnType
#include <FINNCppDriver/utils/DynamicMdSpan.hpp>  // for DynamicMdSpan
#include <boost/program_options.hpp>              // for variables_map
#include <ext/alloc_traits.h>                     // for __alloc_tr...
#include <xtensor/xadapt.hpp>                     // for adapt
#include <xtensor/xarray.hpp>                     // for xarray_ada...
#include <xtensor/xiterator.hpp>                  // for operator==
#include <xtensor/xlayout.hpp>                    // for layout_type
#include <xtensor/xnpy.hpp>                       // for dump_npy, ...
#include <xtl/xiterator_base.hpp>                 // for operator!=

#include "thresholds.h"

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
    // driver.setBatchSize(hostBufferSize);
    driver.setForceAchieval(true);
    return driver;
}

constinit float a = 254 / (thresholds[254] - thresholds[0]);

Finn::vector<int8_t> multithresholdLinearPerTensor(const Finn::vector<float>& inp) {
    const size_t size = inp.size();
    Finn::vector<int8_t> ret(size, -127);
#pragma omp simd
    for (size_t i = 0; i < size; ++i) {
        ret[i] += std::clamp(static_cast<int>((inp[i] - first_thresholds[0]) * a), 0, 254);
    }
    return ret;
}

template<typename O>
using destribution_t = typename std::conditional_t<std::is_same_v<O, float>, std::uniform_real_distribution<O>, std::uniform_int_distribution<O>>;

size_t runtime = 90;

template<typename T>
void runThroughputTestImpl(Finn::Driver<true>& baseDriver, std::size_t elementCount, uint batchSize) {
    using dtype = T;
    Finn::vector<dtype> testInputs(elementCount * batchSize);

    std::random_device rndDevice;
    std::mt19937 mersenneEngine{ rndDevice() };  // Generates random integers

    destribution_t<dtype> dist{ static_cast<dtype>(InputFinnType().min()), static_cast<dtype>(InputFinnType().max()) };

    auto gen = [&dist, &mersenneEngine]() { return dist(mersenneEngine); };

    // constexpr size_t nTestruns = 5000;
    // std::chrono::duration<double> sumRuntimeEnd2End{};

    constexpr size_t elementsPerRet = 5;
    std::vector<float> retFinal(batchSize * elementsPerRet);
    constexpr float scale = 0.02659747190773487f;
    constexpr std::array<float, 5> addVec = { 0.03121155872941017f,
    -0.16459396481513977f,
    0.30250221490859985f,
    0.09384322166442871f,
    -0.3423689603805542f };

    // Warmup
    std::fill(testInputs.begin(), testInputs.end(), 1);
    for (size_t i = 0; i < 10; ++i) {
        auto quantInputs = multithresholdLinearPerTensor(testInputs);
        auto warmup = baseDriver.inferSynchronous(quantInputs.begin(), quantInputs.end());
        #pragma omp simd collapse(2)
        for (size_t i = 0; i < batchSize; ++i) {
            for (size_t j = 0; j < elementsPerRet; ++j) {
                retFinal[i * elementsPerRet + j] = warmup[i * elementsPerRet + j] * scale + addVec[j];
            }
        }
        Finn::DoNotOptimize(retFinal);
    }

    std::vector<std::chrono::duration<double>> preprocessing;
    std::vector<std::chrono::duration<double>> datatransferToFPGA;
    std::vector<std::chrono::duration<double>> inference;
    std::vector<std::chrono::duration<double>> datatransferFromFPGA;
    std::vector<std::chrono::duration<double>> postprocessing;

    std::cout << "Running for batch size " << batchSize << "\n";
    for (size_t i = 0; i < 100; ++i) {
        std::generate(testInputs.begin(), testInputs.end(), gen);
        const auto start = std::chrono::high_resolution_clock::now();
        auto quantInputsReal = multithresholdLinearPerTensor(testInputs);
        static auto foldedShape = static_cast<Finn::ExtendedBufferDescriptor*>(baseDriver.getConfig().deviceWrappers[0].idmas[0].get())->foldedShape;
        foldedShape[0] = batchSize;
        const Finn::DynamicMdSpan reshapedInput(quantInputsReal.begin(), quantInputsReal.end(), foldedShape);
        auto packed = Finn::packMultiDimensionalInputs<InputFinnType>(quantInputsReal.begin(), quantInputsReal.end(), reshapedInput, foldedShape.back());
        const auto endPreprocessing = std::chrono::high_resolution_clock::now();

        auto result = baseDriver.infer(packed.begin(), packed.end(), 0, baseDriver.defaultInputKernelName, 0, baseDriver.defaultOutputKernelName, batchSize, true);
        const auto endDFF = std::chrono::high_resolution_clock::now();
        const auto endDTF = baseDriver.endcopy;
        const auto endInf = baseDriver.endinf;

        static auto packedOutput = baseDriver.getConfig().deviceWrappers[0].odmas[0]->packedShape;
        packedOutput[0] = batchSize;
        static auto foldedOutput = static_cast<Finn::ExtendedBufferDescriptor*>(baseDriver.getConfig().deviceWrappers[0].odmas[0].get())->foldedShape;
        foldedOutput[0] = batchSize;
        const Finn::DynamicMdSpan reshapedOutput(result.begin(), result.end(), packedOutput);
        auto unpacked = Finn::unpackMultiDimensionalOutputs<OutputFinnType>(result.begin(), result.end(), reshapedOutput, foldedOutput);
        #pragma omp simd collapse(2)
        for (size_t i = 0; i < batchSize; ++i) {
            for (size_t j = 0; j < elementsPerRet; ++j) {
                retFinal[i * elementsPerRet + j] = unpacked[i * elementsPerRet + j] * scale + addVec[j];
            }
        }
        Finn::DoNotOptimize(retFinal);
        const auto endPostprocessing = std::chrono::high_resolution_clock::now();

        preprocessing.emplace_back(endPreprocessing - start);
        datatransferToFPGA.emplace_back(endDTF - endPreprocessing);
        inference.emplace_back(endInf - endDTF);
        datatransferFromFPGA.emplace_back(endDFF - endInf);
        postprocessing.emplace_back(endPostprocessing - endDFF);

    }

    std::cout << "%&%&%&%\n";
    std::cout << "preprocessing = [";
    for (auto&& elem : preprocessing) {
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(elem).count() << ",";
    }
    std::cout << "]\n";
    std::cout << "datatransferToFPGA = [";
    for (auto&& elem : datatransferToFPGA) {
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(elem).count() << ",";
    }
    std::cout << "]\n";
    std::cout << "inference = [";
    for (auto&& elem : inference) {
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(elem).count() << ",";
    }
    std::cout << "]\n";
    std::cout << "datatransferFromFPGA = [";
    for (auto&& elem : datatransferFromFPGA) {
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(elem).count() << ",";
    }
    std::cout << "]\n";
    std::cout << "postprocessing = [";
    for (auto&& elem : postprocessing) {
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(elem).count() << ",";
    }
    std::cout << "]\n";
    std::cout << "%&%&%&%\n";


    

    //omp_set_num_threads(2);

//     Finn::vector<int8_t> testInputsInt(elementCount * batchSize);
//     std::fill(testInputsInt.begin(), testInputsInt.end(), 1);

//     size_t counter = 0;
//     const auto start = std::chrono::high_resolution_clock::now();
//     while (std::chrono::high_resolution_clock::now() - start < std::chrono::duration<float>(runtime)) {
//         //auto quantInputsReal = multithresholdLinearPerTensor(testInputs);
//         auto ret = baseDriver.inferSynchronous(testInputsInt.begin(), testInputsInt.end());
// // #pragma omp simd collapse(2)
// //         for (size_t i = 0; i < batchSize; ++i) {
// //             for (size_t j = 0; j < elementsPerRet; ++j) {
// //                 retFinal[i * elementsPerRet + j] = ret[i * elementsPerRet + j] * scale + addVec[j];
// //             }
// //         }
//         Finn::DoNotOptimize(ret);
//         ++counter;
//     }
//     const auto stop = std::chrono::high_resolution_clock::now();
//     std::chrono::duration<double> sumRuntimeEnd2End = (stop - start);

//     size_t infered = counter * batchSize;
//     double throughput = infered / (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeEnd2End).count()) * 1e-9);
//     size_t starttime = std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count();
//     size_t stoptime = std::chrono::duration_cast<std::chrono::nanoseconds>(stop.time_since_epoch()).count();

//     std::cout << throughput << " " << starttime << " " << stoptime << " " << infered;

    // for (size_t i = 0; i < nTestruns; ++i) {

    //     const auto start = std::chrono::high_resolution_clock::now();
    //     auto quantInputsReal = multithresholdLinearPerTensor(testInputs);
    //     auto ret = baseDriver.inferSynchronous(quantInputsReal.begin(), quantInputsReal.end());
    //     Finn::DoNotOptimize(ret);
    //     const auto end = std::chrono::high_resolution_clock::now();

    //     sumRuntimeEnd2End += (end - start);
    // }

    // std::chrono::duration<double> sumRuntimePacking{};
    // std::chrono::duration<double> sumRuntimeUnpacking{};
    // std::chrono::duration<double> sumRuntimeReshaping{};

    // for (size_t i = 0; i < nTestruns; ++i) {
    //     std::generate(testInputs.begin(), testInputs.end(), gen);
    //     const auto start = std::chrono::high_resolution_clock::now();
    //     auto quantInputsReal = multithresholdLinearPerTensor(testInputs);
    //     static auto foldedShape = static_cast<Finn::ExtendedBufferDescriptor*>(baseDriver.getConfig().deviceWrappers[0].idmas[0].get())->foldedShape;
    //     foldedShape[0] = batchSize;
    //     const Finn::DynamicMdSpan reshapedInput(quantInputsReal.begin(), quantInputsReal.end(), foldedShape);
    //     const auto reshape = std::chrono::high_resolution_clock::now();
    //     auto packed = Finn::packMultiDimensionalInputs<InputFinnType>(quantInputsReal.begin(), quantInputsReal.end(), reshapedInput, foldedShape.back());
    //     Finn::DoNotOptimize(packed);
    //     const auto end = std::chrono::high_resolution_clock::now();

    //     sumRuntimeReshaping += (reshape - start);
    //     sumRuntimePacking += (end - reshape);
    // }

    // auto packedOutput = baseDriver.getConfig().deviceWrappers[0].odmas[0]->packedShape;
    // packedOutput[0] = batchSize;
    // std::vector<uint8_t> unpackingInputs(FinnUtils::shapeToElements(packedOutput));
    // for (size_t i = 0; i < nTestruns; ++i) {
    //     const auto start = std::chrono::high_resolution_clock::now();
    //     auto foldedOutput = static_cast<Finn::ExtendedBufferDescriptor*>(baseDriver.getConfig().deviceWrappers[0].odmas[0].get())->foldedShape;
    //     foldedOutput[0] = batchSize;
    //     const Finn::DynamicMdSpan reshapedOutput(unpackingInputs.begin(), unpackingInputs.end(), packedOutput);
    //     auto unpacked = Finn::unpackMultiDimensionalOutputs<OutputFinnType>(unpackingInputs.begin(), unpackingInputs.end(), reshapedOutput, foldedOutput);
    //     Finn::DoNotOptimize(unpacked);
    //     const auto end = std::chrono::high_resolution_clock::now();
    //     sumRuntimeUnpacking += (end - start);
    // }

    // std::cout << "Avg. end2end latency: " << (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeEnd2End).count()) / nTestruns / 1000) << "us\n";
    // std::cout << "Avg. end2end throughput: " << 1 / (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeEnd2End).count()) / nTestruns / batchSize / 1000 / 1000 / 1000) << " inferences/s\n";
    // std::cout << "Avg. packing latency: " << (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimePacking).count()) / nTestruns) << "ns\n";
    // std::cout << "Avg. folding latency: " << (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeReshaping).count()) / nTestruns) << "ns\n";
    // std::cout << "Avg. unpacking latency: " << (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeUnpacking).count()) / nTestruns) << "ns\n";
    // std::cout << "Avg. raw inference latency:"
    //           << (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeEnd2End).count()) / nTestruns) -
    //                  (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimePacking).count()) / nTestruns) -
    //                  (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeReshaping).count()) / nTestruns) -
    //                  (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sumRuntimeUnpacking).count()) / nTestruns)
    //           << "ns\n";
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
    uint batchSize = baseDriver.getBatchSize();
    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Input element count " << std::to_string(elementcount);
    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Batch size: " << batchSize;

    // constexpr bool isInteger = InputFinnType().isInteger();
    // if constexpr (isInteger) {
    //     using dtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;
    //     runThroughputTestImpl<dtype>(baseDriver, elementcount, batchSize);
    //     // benchmark each step in call chain for int
    // } else {
    runThroughputTestImpl<float>(baseDriver, elementcount, batchSize);
    //}
}

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
    }
    else if (size == 8) {
        // double
        loadInferDump<double>(baseDriver, loadedNpyFile, outputFile);
    }
    else {
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
        loadInferDump<int8_t>(baseDriver, loadedNpyFile, outputFile);
    }
    else if (size == 2) {
        // int16_t
        loadInferDump<int16_t>(baseDriver, loadedNpyFile, outputFile);
    }
    else if (size == 4) {
        // int32_t
        loadInferDump<int32_t>(baseDriver, loadedNpyFile, outputFile);
    }
    else if (size == 8) {
        // int64_t
        loadInferDump<int64_t>(baseDriver, loadedNpyFile, outputFile);
    }
    else {
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
        loadInferDump<uint8_t>(baseDriver, loadedNpyFile, outputFile);
    }
    else if (size == 2) {
        // uint16_t
        loadInferDump<uint16_t>(baseDriver, loadedNpyFile, outputFile);
    }
    else if (size == 4) {
        // uint32_t
        loadInferDump<uint32_t>(baseDriver, loadedNpyFile, outputFile);
    }
    else if (size == 8) {
        // uint64_t
        loadInferDump<uint64_t>(baseDriver, loadedNpyFile, outputFile);
    }
    else {
        FinnUtils::logAndError<std::runtime_error>("Unsupported floating point type detected when loading input npy file!");
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
void runWithInputFile(Finn::Driver<true>& baseDriver, logger_type& logger, const std::vector<std::string>& inputFiles, const std::vector<std::string>& outputFiles) {
    FINN_LOG(logger, loglevel::info) << finnMainLogPrefix() << "Running driver on input files";
    logDeviceInformation(logger, baseDriver.getDeviceHandler(0).getDevice(), baseDriver.getConfig().deviceWrappers[0].xclbin);

    for (auto&& [inp, out] = std::tuple{ inputFiles.begin(), outputFiles.begin() }; inp != inputFiles.end(); ++inp, ++out) {
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
        }
        else {
            // all other endians
            FinnUtils::logAndError<std::runtime_error>("At the moment only files created on little endian systems are supported!\n");
        }
    }
}

/**
 * @brief Validates the user input for the driver mode switch
 *
 * @param mode User input string for selected mode
 */
void validateDriverMode(const std::string& mode) {
    if (mode != "execute" && mode != "throughput") {
        throw finnBoost::program_options::error_with_option_name("'" + mode + "' is not a valid driver mode!", "exec_mode");
    }

    FINN_LOG(Logger::getLogger(), loglevel::info) << finnMainLogPrefix() << "Driver Mode: " << mode;
}

/**
 * @brief Validates the user input for the batch size
 *
 * @param batch User input batch size
 */
void validateBatchSize(int batch) {
    if (batch <= 0) {
        throw finnBoost::program_options::error_with_option_name("Batch size must be positive, but is '" + std::to_string(batch) + "'", "batchsize");
    }
}

/**
 * @brief Validates the user input for the config path. Also checks if file exists
 *
 * @param path Path string to be validated
 */
void validateConfigPath(const std::string& path) {
    auto configFilePath = std::filesystem::path(path);
    if (!std::filesystem::exists(configFilePath)) {
        throw finnBoost::program_options::error_with_option_name("Cannot find config file at " + configFilePath.string(), "configpath");
    }

    FINN_LOG(Logger::getLogger(), loglevel::info) << finnMainLogPrefix() << "Config file found at " + configFilePath.string();
}

/**
 * @brief Validates the user input for the input file path. Also checks if input file exists.
 *
 * @param path Path string to be validated
 */
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

/**
 * @brief Main entrypoint for the frontend of the C++ Finn driver
 *
 * @param argc Number of command line parameters
 * @param argv Array of command line parameters
 * @return int Exit status code
 */
int main(int argc, char* argv[]) {
    auto logger = Logger::getLogger();
    FINN_LOG(logger, loglevel::info) << "C++ Driver started";

    try {
        // Command Line Argument Parser
        po::options_description desc{ "Options" };
        //clang-format off
        desc.add_options()("help,h", "Display help")("exec_mode,e", po::value<std::string>()->default_value("throughput")->notifier(&validateDriverMode),
            R"(Please select functional verification ("execute") or throughput test ("throughput")")("configpath,c", po::value<std::string>()->required()->notifier(&validateConfigPath),
                "Required: Path to the config.json file emitted by the FINN compiler")(
                    "input,i", po::value<std::vector<std::string>>()->multitoken()->composing()->notifier(&validateInputPath), "Path to one or more input files (npy format). Only required if mode is set to \"file\"")(
                        "output,o", po::value<std::vector<std::string>>()->multitoken()->composing(), "Path to one or more output files (npy format). Only required if mode is set to \"file\"")(
                            "batchsize,b", po::value<int>()->default_value(1)->notifier(&validateBatchSize), "Number of samples for inference")("time,t", po::value<int>()->default_value(90), "Throughput benchmark time");
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

        runtime = varMap["time"].as<int>();

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
        }
        else if (varMap["exec_mode"].as<std::string>() == "throughput") {
            auto driver = createDriverFromConfig<true>(varMap["configpath"].as<std::string>(), static_cast<uint>(varMap["batchsize"].as<int>()));
            runThroughputTest(driver, logger);
        }
        else {
            FinnUtils::logAndError<std::invalid_argument>("Unknown driver mode: " + varMap["exec_mode"].as<std::string>());
        }

        return 1;
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 0;
    }
    catch (...)  // Catch everything that is not an exception class
    {
        std::cerr << "Unknown error!"
            << "\n";
        return 0;
    }
}