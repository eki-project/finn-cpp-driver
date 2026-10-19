/**
 * @file StandaloneExecutableDriver.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de), Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Driver class for use in the standalone executable.
 * @date 2026-09-25
 *
 * @copyright Copyright (c) 2026
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef STANDALONE_EXECUTABLE_DRIVER_HPP
#define STANDALONE_EXECUTABLE_DRIVER_HPP
#include <FINNCppDriver/core/DeviceHandler.h>          // for DeviceHandler
#include <FINNCppDriver/utils/ConfigurationStructs.h>  // for Config
#include <FINNCppDriver/utils/DoNotOptimize.h>         // for DoNotOptimize
#include <FINNCppDriver/utils/FinnUtils.h>             // for logAndError
#include <FINNCppDriver/utils/Types.h>                 // for shape_t

#include <FINNCppDriver/core/BaseDriver.hpp>      // IWYU pragma: keep
#include <FINNCppDriver/utils/DataPacking.hpp>    // for AutoReturnType
#include <FINNCppDriver/utils/DynamicMdSpan.hpp>  // for DynamicMdSpan
#include <FINNCppDriver/utils/Logger.hpp>         // for FINN_LOG, ...
#include <chrono>
#include <ext/alloc_traits.h>  // for __alloc_tr...
#include <fstream>
#include <popl.hpp>                       // for program options
#include <random>
#include <xtensor/containers/xadapt.hpp>  // for adapt
#include <xtensor/containers/xarray.hpp>  // for xarray_ada...
#include <xtensor/core/xiterator.hpp>     // for operator==
#include <xtensor/core/xlayout.hpp>       // for layout_type
#include <xtensor/io/xnpy.hpp>            // for dump_npy, ...
#include <xtl/xiterator_base.hpp>         // for operator!=

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


namespace Finn {

    /**
     * @brief Log some initial information about the device and the kernels used
     *
     * @param device
     * @param filename
     */
    inline void logDeviceInformation(xrt::device& device, const std::string& filename) {
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

    template<typename O>
    using distribution_t = typename std::conditional_t<std::is_same_v<O, float>, std::uniform_real_distribution<O>, std::uniform_int_distribution<O>>;

    /**
     * @brief Random value distribution over the representable range of the input datatype
     *
     * uniform_real_distribution requires max - min to fit into the type, so float inputs are drawn from [-1e3, 1e3] at most.
     */
    template<typename DT>
    distribution_t<DT> makeInputDistribution() {
        if constexpr (std::is_same_v<DT, float>) {
            const auto lo = static_cast<float>(std::max(InputFinnType().min(), -1e3));
            const auto hi = static_cast<float>(std::min(InputFinnType().max(), 1e3));
            return std::uniform_real_distribution<float>{lo, hi};
        } else {
            return std::uniform_int_distribution<DT>{static_cast<DT>(InputFinnType().min()), static_cast<DT>(InputFinnType().max())};
        }
    }

    /**
     * @brief A class to represent a full FINN-Driver
     *
     * @tparam bool SynchronousInference mode switch
     * @tparam F The FINN input datatype
     * @tparam S The FINN output datatype
     * @tparam T The C-datatype used to pass data to the FPGA
     */
    template<bool SynchronousInference, IsDatatype F, IsDatatype S, typename T = uint8_t>
    class StandaloneExecutableDriver : public BaseDriver<SynchronousInference, F, S, T> {
        using BaseDriver<SynchronousInference, F, S, T>::BaseDriver;
        using Clock = std::chrono::steady_clock;

         private:
        /**
         * @brief A short prefix usable with the logger to determine the source of the log write
         *
         * @return std::string
         */
        std::string finnMainLogPrefix() { return "[FINNDriver] "; }

         public:
        /**
         * @brief Implementation function for running throughput tests
         *
         * @tparam DT Data type for the test inputs
         * @param elementCount Number of elements in test data
         * @param batchSize Batch size for inference
         */
        template<typename DT>
        void runThroughputTestImpl(std::size_t elementCount, uint batchSize, std::size_t nTestruns = 5000) {
            Finn::vector<DT> testInputs(elementCount * batchSize);

            std::random_device rndDevice;
            std::mt19937 mersenneEngine{rndDevice()};  // Generates random integers

            distribution_t<DT> dist = makeInputDistribution<DT>();

            auto gen = [&dist, &mersenneEngine]() { return dist(mersenneEngine); };

            std::chrono::duration<double> sumRuntimeEnd2End{};

            // Warmup
            std::fill(testInputs.begin(), testInputs.end(), 1);
            auto warmup = this->inferSynchronous(testInputs.begin(), testInputs.end());
            Finn::DoNotOptimize(warmup);

            for (size_t i = 0; i < nTestruns; ++i) {
                std::generate(testInputs.begin(), testInputs.end(), gen);
                const auto start = Clock::now();
                auto ret = this->inferSynchronous(testInputs.begin(), testInputs.end());
                Finn::DoNotOptimize(ret);
                const auto end = Clock::now();

                sumRuntimeEnd2End += (end - start);
            }

            std::chrono::duration<double> sumRuntimePacking{};
            std::chrono::duration<double> sumRuntimeUnpacking{};
            std::chrono::duration<double> sumRuntimeReshaping{};

            auto foldedShape = this->getInputFoldedShape();
            foldedShape[0] = batchSize;
            for (size_t i = 0; i < nTestruns; ++i) {
                std::generate(testInputs.begin(), testInputs.end(), gen);
                const auto start = Clock::now();
                const Finn::DynamicMdSpan reshapedInput(testInputs.begin(), testInputs.end(), foldedShape);
                const auto reshape = Clock::now();
                auto packed = Finn::packMultiDimensionalInputs<InputFinnType>(testInputs.begin(), testInputs.end(), reshapedInput, foldedShape.back());
                Finn::DoNotOptimize(packed);
                const auto end = Clock::now();

                sumRuntimeReshaping += (reshape - start);
                sumRuntimePacking += (end - reshape);
            }

            auto packedOutput = this->getOutputPackedShape();
            packedOutput[0] = batchSize;
            std::vector<uint8_t> unpackingInputs(FinnUtils::shapeToElements(packedOutput));
            for (size_t i = 0; i < nTestruns; ++i) {
                const auto start = Clock::now();
                auto foldedOutput = this->getOutputFoldedShape();
                foldedOutput[0] = batchSize;
                const Finn::DynamicMdSpan reshapedOutput(unpackingInputs.begin(), unpackingInputs.end(), packedOutput);
                auto unpacked = Finn::unpackMultiDimensionalOutputs<OutputFinnType>(unpackingInputs.begin(), unpackingInputs.end(), reshapedOutput, foldedOutput);
                Finn::DoNotOptimize(unpacked);
                const auto end = Clock::now();
                sumRuntimeUnpacking += (end - start);
            }

            const auto avgNs = [nTestruns](const std::chrono::duration<double>& sum) { return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(sum).count()) / static_cast<double>(nTestruns); };
            std::cout << "Avg. end2end latency: " << avgNs(sumRuntimeEnd2End) / 1000 << "us\n";
            std::cout << "Avg. end2end throughput: " << 1e9 * batchSize / avgNs(sumRuntimeEnd2End) << " inferences/s\n";
            std::cout << "Avg. packing latency: " << avgNs(sumRuntimePacking) << "ns\n";
            std::cout << "Avg. folding latency: " << avgNs(sumRuntimeReshaping) << "ns\n";
            std::cout << "Avg. unpacking latency: " << avgNs(sumRuntimeUnpacking) << "ns\n";
            std::cout << "Avg. raw inference latency:" << avgNs(sumRuntimeEnd2End) - avgNs(sumRuntimePacking) - avgNs(sumRuntimeReshaping) - avgNs(sumRuntimeUnpacking) << "ns\n";
        }

        /**
         * @brief Run a throughput test to test the performance of the driver
         */
        void runThroughputTest() {
            FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Device Information: ";
            const uint deviceIndex = this->getDefaultInputDeviceIndex();
            logDeviceInformation(this->getDeviceHandler(deviceIndex).getDevice(), this->getConfig().getDeviceWrapper(deviceIndex).xclbin);

            size_t elementcount = FinnUtils::shapeToElements(this->getInputNormalShape());
            uint batchSize = this->getBatchSize();
            FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Input element count " << std::to_string(elementcount);
            FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Batch size: " << batchSize;

            constexpr bool isInteger = InputFinnType().isInteger();
            if constexpr (isInteger) {
                using dtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;
                runThroughputTestImpl<dtype>(elementcount, batchSize);
            } else {
                runThroughputTestImpl<float>(elementcount, batchSize);
            }
        }

        /**
         * @brief Load data from numpy file, run inference, and dump results
         *
         * @tparam DT Data type for the loaded data
         * @param loadedNpyFile Loaded numpy file containing input data
         * @param outputFile Path to output file for results
         */
        template<typename DT>
        void loadInferDump(xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
            auto xtensorArray = std::move(loadedNpyFile).cast<DT, xt::layout_type::dynamic>();
            const std::size_t expectedElements = FinnUtils::shapeToElements(this->getInputNormalShape(true));
            if (xtensorArray.size() != expectedElements) {
                Finn::logAndError<std::runtime_error>("Input file holds " + std::to_string(xtensorArray.size()) + " elements, but the driver expects " + std::to_string(expectedElements) + " (normal input shape " +
                                                      FinnUtils::shapeToString(this->getInputNormalShape(true)) + " with batch size " + std::to_string(this->getBatchSize()) + ")");
            }
            Finn::vector<DT> vec(xtensorArray.begin(), xtensorArray.end());
            auto ret = this->inferSynchronous(vec.begin(), vec.end());
            auto xarr = xt::adapt(ret, this->getOutputNormalShape(true));
            xt::dump_npy(outputFile, xarr);
        }

        /**
         * @brief Executes inference on the input file, dispatching on the numpy dtype
         *
         * @param loadedNpyFile Input file
         * @param outputFile Name of output file
         */
        void inferNpyFile(xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
            // numpy type strings look like "<f4", "<i2" or, for single-byte types, "|u1" / "|b1"
            const std::string& typestring = loadedNpyFile.m_typestring;
            if (typestring.size() < 2) {
                Finn::logAndError<std::runtime_error>("Unexpected numpy type string '" + typestring + "'");
            }
            const char byteOrder = typestring[0];
            const char kind = typestring[1];
            const std::size_t wordSize = loadedNpyFile.m_word_size;
            if (byteOrder == '>' || (byteOrder == '=' && std::endian::native != std::endian::little)) {
                Finn::logAndError<std::runtime_error>("At the moment only files created on little endian systems are supported!");
            }
            if (byteOrder != '<' && byteOrder != '|' && byteOrder != '=') {
                Finn::logAndError<std::runtime_error>("Unexpected numpy type string '" + typestring + "'");
            }

            switch (kind) {
                case 'f':
                    if (wordSize == 4) {
                        loadInferDump<float>(loadedNpyFile, outputFile);
                    } else if (wordSize == 8) {
                        loadInferDump<double>(loadedNpyFile, outputFile);
                    } else {
                        Finn::logAndError<std::runtime_error>("Unsupported floating point type '" + typestring + "' in input npy file!");
                    }
                    break;
                case 'i':
                    switch (wordSize) {
                        case 1: loadInferDump<int8_t>(loadedNpyFile, outputFile); break;
                        case 2: loadInferDump<int16_t>(loadedNpyFile, outputFile); break;
                        case 4: loadInferDump<int32_t>(loadedNpyFile, outputFile); break;
                        case 8: loadInferDump<int64_t>(loadedNpyFile, outputFile); break;
                        default: Finn::logAndError<std::runtime_error>("Unsupported signed integer type '" + typestring + "' in input npy file!");
                    }
                    break;
                case 'u':
                    switch (wordSize) {
                        case 1: loadInferDump<uint8_t>(loadedNpyFile, outputFile); break;
                        case 2: loadInferDump<uint16_t>(loadedNpyFile, outputFile); break;
                        case 4: loadInferDump<uint32_t>(loadedNpyFile, outputFile); break;
                        case 8: loadInferDump<uint64_t>(loadedNpyFile, outputFile); break;
                        default: Finn::logAndError<std::runtime_error>("Unsupported unsigned integer type '" + typestring + "' in input npy file!");
                    }
                    break;
                case 'b': {
                    auto xtensorArray = std::move(loadedNpyFile).cast<bool, xt::layout_type::dynamic>();
                    Finn::vector<uint8_t> vec(xtensorArray.begin(), xtensorArray.end());
                    auto ret = this->inferSynchronous(vec.begin(), vec.end());
                    auto xarr = xt::adapt(ret, this->getOutputNormalShape(true));
                    xt::dump_npy(outputFile, xarr);
                    break;
                }
                default: Finn::logAndError<std::runtime_error>("Loading a numpy array with type identifier string '" + typestring + "' is currently not supported.");
            }
        }

        /**
         * @brief Run inference on an input file
         *
         * @param inputFiles Files used for inference input
         * @param outputFiles Filenames used for output files
         */
        void runWithInputFile(const std::vector<std::string>& inputFiles, const std::vector<std::string>& outputFiles) {
            FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Running driver on input files";

            const uint deviceIndex = this->getDefaultInputDeviceIndex();
            logDeviceInformation(this->getDeviceHandler(deviceIndex).getDevice(), this->getConfig().getDeviceWrapper(deviceIndex).xclbin);

            for (auto&& [inp, out] = std::tuple{inputFiles.begin(), outputFiles.begin()}; inp != inputFiles.end(); ++inp, ++out) {
                // load npy file and process it
                // using normal xnpy::load_npy will not work because it requires a destination type
                // instead use xnpy::detail::load_npy_file und then concert by hand based on m_typestring of xnpy::detail::npy_file
                std::ifstream stream(*inp, std::ifstream::binary);
                if (!stream) {
                    Finn::logAndError<std::runtime_error>("io error: failed to open input file " + *inp);
                }

                auto loadedFile = xt::detail::load_npy_file(stream);
                inferNpyFile(loadedFile, *out);
            }
        }
    };
}  // namespace Finn
#endif
