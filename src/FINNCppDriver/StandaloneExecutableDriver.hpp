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
#include <ext/alloc_traits.h>                     // for __alloc_tr...
#include <popl.hpp>                               // for program options
#include <xtensor/containers/xadapt.hpp>          // for adapt
#include <xtensor/containers/xarray.hpp>          // for xarray_ada...
#include <xtensor/core/xiterator.hpp>             // for operator==
#include <xtensor/core/xlayout.hpp>               // for layout_type
#include <xtensor/io/xnpy.hpp>                    // for dump_npy, ...
#include <xtl/xiterator_base.hpp>                 // for operator!=

#include <FINNCppDriver/core/BaseDriver.hpp>

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

    template<typename O>
    using distribution_t = typename std::conditional_t<std::is_same_v<O, float>, std::uniform_real_distribution<O>, std::uniform_int_distribution<O>>;

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

        private:

        /**
         * @brief A short prefix usable with the logger to determine the source of the log write
         *
         * @return std::string
         */
        std::string finnMainLogPrefix() { return "[FINNDriver] "; }

        /**
        * @brief Index position in string that contains the byte size of the datatype stored in the numpy input file
        *
        */
        static constexpr size_t typeStringByteSizePos = 2;

        /**
         * @brief Implementation function for running throughput tests
         *
         * @tparam T Data type for the test inputs
         * @param elementCount Number of elements in test data
         * @param batchSize Batch size for inference
         */
        template<typename DT>
        void runThroughputTestImpl(std::size_t elementCount, uint batchSize, std::size_t nTestruns = 5000) {
            Finn::vector<DT> testInputs(elementCount * batchSize);

            std::random_device rndDevice;
            std::mt19937 mersenneEngine{rndDevice()};  // Generates random integers

            distribution_t<DT> dist{static_cast<DT>(InputFinnType().min()), static_cast<DT>(InputFinnType().max())};

            auto gen = [&dist, &mersenneEngine]() { return dist(mersenneEngine); };

            std::chrono::duration<double> sumRuntimeEnd2End{};

            // Warmup
            std::fill(testInputs.begin(), testInputs.end(), 1);
            auto warmup = this->inferSynchronous(testInputs.begin(), testInputs.end());
            Finn::DoNotOptimize(warmup);

            for (size_t i = 0; i < nTestruns; ++i) {
                std::generate(testInputs.begin(), testInputs.end(), gen);
                const auto start = std::chrono::high_resolution_clock::now();
                auto ret = this->inferSynchronous(testInputs.begin(), testInputs.end());
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
                static auto foldedShape = static_cast<Finn::ExtendedBufferDescriptor*>(this->getConfig().deviceWrappers[0].idmas[0].get())->foldedShape;
                foldedShape[0] = batchSize;
                const Finn::DynamicMdSpan reshapedInput(testInputs.begin(), testInputs.end(), foldedShape);
                const auto reshape = std::chrono::high_resolution_clock::now();
                auto packed = Finn::packMultiDimensionalInputs<InputFinnType>(testInputs.begin(), testInputs.end(), reshapedInput, foldedShape.back());
                Finn::DoNotOptimize(packed);
                const auto end = std::chrono::high_resolution_clock::now();

                sumRuntimeReshaping += (reshape - start);
                sumRuntimePacking += (end - reshape);
            }

            auto packedOutput = this->getConfig().deviceWrappers[0].odmas[0]->packedShape;
            packedOutput[0] = batchSize;
            std::vector<uint8_t> unpackingInputs(FinnUtils::shapeToElements(packedOutput));
            for (size_t i = 0; i < nTestruns; ++i) {
                const auto start = std::chrono::high_resolution_clock::now();
                auto foldedOutput = static_cast<Finn::ExtendedBufferDescriptor*>(this->getConfig().deviceWrappers[0].odmas[0].get())->foldedShape;
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

        public:
        /**
        * @brief Run a throughput test to test the performance of the driver
        *
        * @param logger
        */
        void runThroughputTest() {
            FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Device Information: ";
            logDeviceInformation(this->getDeviceHandler(0).getDevice(), this->getConfig().deviceWrappers[0].xclbin);

            size_t elementcount = FinnUtils::shapeToElements((std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(this->getConfig().deviceWrappers[0].idmas[0]))->normalShape);
            uint batchSize = this->getBatchSize();
            FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Input element count " << std::to_string(elementcount);
            FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Batch size: " << batchSize;

            constexpr bool isInteger = InputFinnType().isInteger();
            if constexpr (isInteger) {
                using dtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;
                runThroughputTestImpl<dtype>(elementcount, batchSize);
                // benchmark each step in call chain for int
            } else {
                runThroughputTestImpl<float>(elementcount, batchSize);
            }
        }

        /**
        * @brief Return a shared_ptr to the default ODMA (default device, ODMA kernel index 0).
        * 
        * TODO(bwintermann): default ODMA kernel index should be configurable
        * 
        */
        std::shared_ptr<Finn::ExtendedBufferDescriptor> getDefaultODMA() {
            return std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(this->getConfig().deviceWrappers[this->getDefaultOutputDeviceIndex()].odmas[0]);
        }

        /**
        * @brief Return a shared_ptr to the default IDMA (default device, IDMA kernel index 0).
        * 
        * TODO(bwintermann): default IDMA kernel index should be configurable
        * 
        */
        std::shared_ptr<Finn::ExtendedBufferDescriptor> getDefaultIDMA() {
            return std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(this->getConfig().deviceWrappers[this->getDefaultInputDeviceIndex()].idmas[0]);
        }


        /**
        * @brief Load data from numpy file, run inference, and dump results
        *
        * @tparam T Data type for the loaded data
        * @param loadedNpyFile Loaded numpy file containing input data
        * @param outputFile Path to output file for results
        */
        template<typename DT>
        void loadInferDump(xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
            auto xtensorArray = std::move(loadedNpyFile).cast<DT, xt::layout_type::dynamic>();
            Finn::vector<DT> vec(xtensorArray.begin(), xtensorArray.end());
            auto ret = this->inferSynchronous(vec.begin(), vec.end());
            auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(this->getConfig().deviceWrappers[0].odmas[0]))->normalShape);
            xt::dump_npy(outputFile, xarr);
        }

        /**
        * @brief Executes inference on the input file if input type is a floating point type
        * @attention This function does no checking of the datatype contained in the loadedNpyFile! Passing a npy file containing a non floating point type is UB.
        *
        * @param loadedNpyFile Input file
        * @param outputFile Name of output file
        */
        void inferFloatingPoint(xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
            size_t sizePos = typeStringByteSizePos;
            int size = std::stoi(loadedNpyFile.m_typestring, &sizePos);
            if (size == 4) {
                // float
                loadInferDump<float>(loadedNpyFile, outputFile);
            } else if (size == 8) {
                // double
                loadInferDump<double>(loadedNpyFile, outputFile);
            } else {
                Finn::logAndError<std::runtime_error>("Unsupported floating point type detected when loading input npy file!");
            }
        }

        /**
        * @brief Executes inference on the input file if input type is a signed integer type
        * @attention This function does no checking of the datatype contained in the loadedNpyFile! Passing a npy file containing a non signed integer type is UB.
        *
        * @param loadedNpyFile
        * @param outputFile
        */
        void inferSignedInteger(xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
            size_t sizePos = typeStringByteSizePos;
            int size = std::stoi(loadedNpyFile.m_typestring, &sizePos);
            if (size == 1) {
                // int8_t
                loadInferDump<int8_t>(loadedNpyFile, outputFile);
            } else if (size == 2) {
                // int16_t
                loadInferDump<int16_t>(loadedNpyFile, outputFile);
            } else if (size == 4) {
                // int32_t
                loadInferDump<int32_t>(loadedNpyFile, outputFile);
            } else if (size == 8) {
                // int64_t
                loadInferDump<int64_t>(loadedNpyFile, outputFile);
            } else {
                Finn::logAndError<std::runtime_error>("Unsupported signed integer type detected when loading input npy file!");
            }
        }

        /**
        * @brief Executes inference on the input file if input type is a unsigned integer type
        * @attention This function does no checking of the datatype contained in the loadedNpyFile! Passing a npy file containing a non unsigned integer type is UB.
        *
        * @param loadedNpyFile
        * @param outputFile
        */
        void inferUnsignedInteger(xt::detail::npy_file& loadedNpyFile, const std::string& outputFile) {
            size_t sizePos = typeStringByteSizePos;
            int size = std::stoi(loadedNpyFile.m_typestring, &sizePos);
            if (size == 1) {
                // uint8_t
                loadInferDump<uint8_t>(loadedNpyFile, outputFile);
            } else if (size == 2) {
                // uint16_t
                loadInferDump<uint16_t>(loadedNpyFile, outputFile);
            } else if (size == 4) {
                // uint32_t
                loadInferDump<uint32_t>(loadedNpyFile, outputFile);
            } else if (size == 8) {
                // uint64_t
                loadInferDump<uint64_t>(loadedNpyFile, outputFile);
            } else {
                Finn::logAndError<std::runtime_error>("Unsupported floating point type detected when loading input npy file!");
            }
        }

        /**
        * @brief Run inference on an input file
        *
        * @param logger Logger to be used
        * @param inputFiles Files used for inference input
        * @param outputFiles Filenames used for output files
        */
        void runWithInputFile(const std::vector<std::string>& inputFiles, const std::vector<std::string>& outputFiles) {
            FINN_LOG(loglevel::info) << finnMainLogPrefix() << "Running driver on input files";
            logDeviceInformation(this->getDeviceHandler(0).getDevice(), this->getConfig().deviceWrappers[0].xclbin);

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
                            inferFloatingPoint(loadedFile, *out);
                            break;
                        }
                        case 'i': {
                            inferSignedInteger(loadedFile, *out);
                            break;
                        }
                        case 'b': {
                            auto xtensorArray = std::move(loadedFile).cast<bool, xt::layout_type::dynamic>();
                            Finn::vector<uint8_t> vec(xtensorArray.begin(), xtensorArray.end());
                            auto ret = this->inferSynchronous(vec.begin(), vec.end());
                            auto xarr = xt::adapt(ret, (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(this->getConfig().deviceWrappers[0].odmas[0]))->normalShape);
                            xt::dump_npy(*out, xarr);
                            break;
                        }
                        case 'u': {
                            inferUnsignedInteger(loadedFile, *out);
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
    };
};
#endif