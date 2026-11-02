/**
 * @file BaseDriver.hpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de), Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements the base driver for FINN
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef BASEDRIVER_HPP
#define BASEDRIVER_HPP

#include <FINNCppDriver/utils/ConfigurationStructs.h>
#include <FINNCppDriver/utils/FinnUtils.h>
#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/utils/DataPacking.hpp>
#include <FINNCppDriver/utils/DynamicMdSpan.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <FINNCppDriver/utils/Logger.hpp>  // for FINN_LOG, loglevel, ...
#include <FINNCppDriver/utils/join.hpp>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>

#include "Accelerator.h"
#include "omp.h"


namespace Finn {
    /**
     * @brief A class to represent a basic FINN-Driver
     *
     * @tparam bool SynchronousInference mode switch
     * @tparam F The FINN input datatype
     * @tparam S The FINN output datatype
     * @tparam T The C-datatype used to pass data to the FPGA
     */
    template<bool SynchronousInference, IsDatatype F, IsDatatype S, typename T = uint8_t>
    class BaseDriver {
        Accelerator accelerator;
        // Also holds the input/output device and kernel indices
        Config configuration;

        uint batchElements = 1;


        /**
         * @brief A logger prefix to determine the source of a log write
         *
         * @return std::string
         */
        static std::string loggerPrefix() { return "[BaseDriver] "; }

         public:
        /**
         * @brief Defines the automatic return type for external use
         *
         */
        using AutoDeducedRetType = Finn::UnpackingAutoRetType::AutoRetType<S>;


        /**
         * @brief Get the xrtDeviceIndex of the input device selected in the configuration
         *
         * @return uint
         */
        uint getInputDeviceIndex() const { return configuration.inputDeviceIndex; }

        /**
         * @brief Get the xrtDeviceIndex of the output device selected in the configuration
         *
         * @return uint
         */
        uint getOutputDeviceIndex() const { return configuration.outputDeviceIndex; }

        /**
         * @brief Retrieve the kernel index from the kernel name on the device with the given xrtDeviceIndex.
         * @param deviceIndex xrtDeviceIndex of the device to search
         * @param name Name of the IDMA kernel
         * @throws std::runtime_error if no such device or kernel exists.
         */
        uint getInputKernelIndexFromName(uint deviceIndex, const std::string& name) {
            const std::vector<std::shared_ptr<BufferDescriptor>>& idmas = configuration.getDeviceWrapper(deviceIndex).idmas;
            for (uint i = 0; i < idmas.size(); ++i) {
                if (idmas[i]->kernelName == name) {
                    return i;
                }
            }
            throw std::runtime_error(std::format("No IDMA of name {} was found in device {}.", name, deviceIndex));
        }

        /**
         * @brief Retrieve the kernel index from the kernel name on the input device.
         * @throws std::runtime_error if no such kernel exists.
         */
        uint getInputKernelIndexFromName(const std::string& name) { return getInputKernelIndexFromName(configuration.inputDeviceIndex, name); }

        /**
         * @brief Retrieve the kernel index from the kernel name on the device with the given xrtDeviceIndex.
         * @param deviceIndex xrtDeviceIndex of the device to search
         * @param name Name of the ODMA kernel
         * @throws std::runtime_error if no such device or kernel exists.
         */
        uint getOutputKernelIndexFromName(uint deviceIndex, const std::string& name) {
            const std::vector<std::shared_ptr<BufferDescriptor>>& odmas = configuration.getDeviceWrapper(deviceIndex).odmas;
            for (uint i = 0; i < odmas.size(); ++i) {
                if (odmas[i]->kernelName == name) {
                    return i;
                }
            }
            throw std::runtime_error(std::format("No ODMA of name {} was found in device {}.", name, deviceIndex));
        }

        /**
         * @brief Retrieve the kernel index from the kernel name on the output device.
         * @throws std::runtime_error if no such kernel exists.
         */
        uint getOutputKernelIndexFromName(const std::string& name) { return getOutputKernelIndexFromName(configuration.outputDeviceIndex, name); }


        /**
         * @brief Return an IDMA buffer descriptor.
         * @param deviceIndex Optional device index. Otherwise the input device is used.
         * @param kernelIndex Optional kernel index. Otherwise the input kernel index is used.
         */
        std::shared_ptr<ExtendedBufferDescriptor> getIDMABufferDescriptor(std::optional<uint> deviceIndex = std::nullopt, std::optional<uint> kernelIndex = std::nullopt) {
            return std::static_pointer_cast<ExtendedBufferDescriptor>(configuration.getDeviceWrapper(deviceIndex.value_or(configuration.inputDeviceIndex)).idmas.at(kernelIndex.value_or(configuration.inputKernelIndex)));
        }

        /**
         * @brief Return an ODMA buffer descriptor.
         * @param deviceIndex Optional device index. Otherwise the output device is used.
         * @param kernelIndex Optional kernel index. Otherwise the output kernel index is used.
         */
        std::shared_ptr<ExtendedBufferDescriptor> getODMABufferDescriptor(std::optional<uint> deviceIndex = std::nullopt, std::optional<uint> kernelIndex = std::nullopt) {
            return std::static_pointer_cast<ExtendedBufferDescriptor>(configuration.getDeviceWrapper(deviceIndex.value_or(configuration.outputDeviceIndex)).odmas.at(kernelIndex.value_or(configuration.outputKernelIndex)));
        }

        /**
         * @brief Return the name of the input kernel.
         */
        std::string getInputKernelName() { return getIDMABufferDescriptor()->kernelName; }

        /**
         * @brief Return the name of the output kernel.
         */
        std::string getOutputKernelName() { return getODMABufferDescriptor()->kernelName; }

        shape_t getInputNormalShape(bool withBatchSize = false, std::optional<uint> deviceIndex = std::nullopt, std::optional<uint> kernelIndex = std::nullopt) {
            auto shape = getIDMABufferDescriptor(deviceIndex, kernelIndex)->normalShape;
            if (withBatchSize) {
                shape[0] = getBatchSize();
            }
            return shape;
        }
        shape_t getInputFoldedShape(bool withBatchSize = false, std::optional<uint> deviceIndex = std::nullopt, std::optional<uint> kernelIndex = std::nullopt) {
            auto shape = getIDMABufferDescriptor(deviceIndex, kernelIndex)->foldedShape;
            if (withBatchSize) {
                shape[0] = getBatchSize();
            }
            return shape;
        }
        shape_t getInputPackedShape(bool withBatchSize = false, std::optional<uint> deviceIndex = std::nullopt, std::optional<uint> kernelIndex = std::nullopt) {
            auto shape = getIDMABufferDescriptor(deviceIndex, kernelIndex)->packedShape;
            if (withBatchSize) {
                shape[0] = getBatchSize();
            }
            return shape;
        }
        shape_t getOutputNormalShape(bool withBatchSize = false, std::optional<uint> deviceIndex = std::nullopt, std::optional<uint> kernelIndex = std::nullopt) {
            auto shape = getODMABufferDescriptor(deviceIndex, kernelIndex)->normalShape;
            if (withBatchSize) {
                shape[0] = getBatchSize();
            }
            return shape;
        }
        shape_t getOutputFoldedShape(bool withBatchSize = false, std::optional<uint> deviceIndex = std::nullopt, std::optional<uint> kernelIndex = std::nullopt) {
            auto shape = getODMABufferDescriptor(deviceIndex, kernelIndex)->foldedShape;
            if (withBatchSize) {
                shape[0] = getBatchSize();
            }
            return shape;
        }
        shape_t getOutputPackedShape(bool withBatchSize = false, std::optional<uint> deviceIndex = std::nullopt, std::optional<uint> kernelIndex = std::nullopt) {
            auto shape = getODMABufferDescriptor(deviceIndex, kernelIndex)->packedShape;
            if (withBatchSize) {
                shape[0] = getBatchSize();
            }
            return shape;
        }

        /**
         * @brief Initializes the base driver. The input/output devices and kernels are the ones selected in the configuration.
         *
         * @param batchSize
         * @throws std::runtime_error if the configuration selects a device or kernel that does not exist
         */
        void initializeBaseDriver(uint batchSize) {
            // A Config does not have to come from createConfigFromPath, so validate its device and kernel selection before the Accelerator resets the FPGAs.
            // All device indices in this class are xrtDeviceIndex values, i.e. the keys of configuration.deviceWrappers.
            configuration.validate();
            accelerator = Accelerator(configuration.deviceWrappers, SynchronousInference, batchSize);
            batchElements = batchSize;
#ifdef UNITTEST
            logDriver();
#endif
        }

        /**
         * @brief Create a BaseDriver from a config file. This needs to be templated by the FINN-datatypes. The corresponding header file is generated by the FINN compiler
         *
         * @param configPath
         * @param batchSize
         */
        BaseDriver(const std::filesystem::path& configPath, uint batchSize) : configuration(createConfigFromPath(configPath)) { initializeBaseDriver(batchSize); };

        /**
         * @brief Create a BaseDriver from a config file. This needs to be templated by the FINN-datatypes. The corresponding header file is generated by the FINN compiler
         *
         * @param configPath
         * @param batchSize
         */
        BaseDriver(const std::string& configPath, uint batchSize) : configuration(createConfigFromPath(configPath)) { initializeBaseDriver(batchSize); };

        /**
         * @brief Create a new base driver based on an existing configuration
         *
         * @param pConfig
         * @param batchSize
         */
        BaseDriver(const Config& pConfig, uint batchSize) : configuration(pConfig) { initializeBaseDriver(batchSize); }

        /**
         * @brief Construct a new Base Driver object
         *
         */
        BaseDriver(BaseDriver&&) noexcept = default;
        /**
         * @brief Construct a new Base Driver object (deleted)
         *
         */
        BaseDriver(const BaseDriver&) noexcept = delete;
        /**
         * @brief Defaulted move assignment operator
         *
         * @return BaseDriver&
         */
        BaseDriver& operator=(BaseDriver&&) noexcept = default;
        /**
         * @brief Defaulted copy assignment operator
         *
         * @return BaseDriver&
         */
        BaseDriver& operator=(const BaseDriver&) = delete;
        /**
         * @brief Destroy the Base Driver object
         *
         */
        virtual ~BaseDriver() = default;


        /**
         * @brief Set the Batch Size
         *
         * @param elements
         */
        void setBatchSize(uint elements) {
            batchElements = elements;
            accelerator.setBatchSize(batchElements);
        }

        /**
         * @brief Get the Batch Size
         *
         * @return Returns the Batch size
         */
        uint getBatchSize() { return batchElements; }

        /**
         * @brief Get the Config object. Simple getter to check things outside the driver
         *
         * @return Config
         */
        Config getConfig() const { return configuration; }

        /**
         * @brief Get the Device object, specified by its index
         *
         * @param index
         * @return DeviceHandler&
         */
        DeviceHandler& getDeviceHandler(uint index) { return accelerator.getDeviceHandler(index); }

        /**
         * @brief Get a specific buffer object specified by its name and the device it is on
         *
         * @param deviceIndex
         * @param bufferName
         * @return DeviceInputBuffer<uint8_t>
         */
        std::shared_ptr<DeviceInputBuffer<uint8_t>> getInputBuffer(uint deviceIndex, const std::string& bufferName) { return getDeviceHandler(deviceIndex).getInputBuffer(bufferName); }

        size_t getSizeInBytes(unsigned int deviceIndex, const std::string& bufferName) { return accelerator.getSizeInBytes(deviceIndex, bufferName); }

        size_t getFeatureMapSize(unsigned int deviceIndex, const std::string& bufferName) { return accelerator.getFeatureMapSize(deviceIndex, bufferName); }

        size_t getBatchSize(unsigned int deviceIndex, const std::string& bufferName) { return accelerator.getBatchSize(deviceIndex, bufferName); }

        size_t getTotalDataSize(unsigned int deviceIndex, const std::string& bufferName) { return accelerator.getTotalDataSize(deviceIndex, bufferName); }

        /**
         * @brief Register a callback function to be called when the inference of a batch is finished
         */
        template<bool Sync = SynchronousInference, typename = std::enable_if_t<!Sync>>
        void registerCallback(unsigned int deviceIndex, const std::string& bufferName, std::function<void(std::size_t)> callback) {
            accelerator.registerCallback(deviceIndex, bufferName, callback);
        }

        /**
         * @brief Store input into the driver for asynchronous inference
         *
         * @tparam IteratorType
         * @param first
         * @param last
         * @param inputDeviceIndex FPGA device to be used for inference
         * @param inputBufferKernelName Identifier of the input kernel
         * @param batchSize Batch size contained in the input
         */
        template<typename IteratorType, bool Sync = SynchronousInference, typename = std::enable_if_t<!Sync>>
        void input(IteratorType first, IteratorType last, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint batchSize) {
            FINN_LOG_DEBUG(loglevel::info) << loggerPrefix() << "Store data for asynchronous inference.";
            // Pack per innermost dimension exactly like the synchronous path, so sub-byte types with inner rows that are not byte aligned get the padding the accelerator expects
            auto foldedShape = getInputFoldedShape(false, inputDeviceIndex, getInputKernelIndexFromName(inputDeviceIndex, inputBufferKernelName));
            foldedShape[0] = batchSize;
            const Finn::DynamicMdSpan reshapedInput(first, last, foldedShape);
            auto packed = Finn::packMultiDimensionalInputs<F, IteratorType>(first, last, reshapedInput, foldedShape.back());
            auto storeFunc = accelerator.storeFactory(inputDeviceIndex, inputBufferKernelName);

            if (packed.size() != getFeatureMapSize(inputDeviceIndex, inputBufferKernelName) * batchSize) {
                Finn::logAndError<std::runtime_error>("Input length (" + std::to_string(packed.size()) + ") does not match up with batches*inputsize_per_batch (" + std::to_string(getFeatureMapSize(inputDeviceIndex, inputBufferKernelName)) +
                                                      "*" + std::to_string(batchSize) + "=" + std::to_string(getFeatureMapSize(inputDeviceIndex, inputBufferKernelName) * batchSize) + ")");
            }

            if (!storeFunc(packed.begin(), packed.end())) {
                Finn::logAndError<std::runtime_error>(loggerPrefix() + "Input could not be stored: the device buffer is shutting down");
            }
        }

        /**
         * @brief Store input into the driver for asynchronous inference
         *
         * @tparam IteratorType
         * @tparam typename
         * @param first
         * @param last
         */
        template<typename IteratorType, bool Sync = SynchronousInference, typename = std::enable_if_t<!Sync>>
        void input(IteratorType first, IteratorType last) {
            input(first, last, configuration.inputDeviceIndex, getInputKernelName(), batchElements);
        }

        /**
         * @brief Get the results of a asynchronous inference
         *
         * @tparam V
         * @param outputDeviceIndex FPGA device from which data should be received
         * @param outputBufferKernelName Identifier of the output kernel
         * @return Finn::vector<V>
         */
        template<typename V = Finn::UnpackingAutoRetType::AutoRetType<S>, bool Sync = SynchronousInference, typename = std::enable_if_t<!Sync>>
        [[nodiscard]] Finn::vector<V> getResults(uint outputDeviceIndex, const std::string& outputBufferKernelName) {
            // TODO(linusjun): maybe this method should block until data is available?
            auto result = accelerator.getOutputData(outputDeviceIndex, outputBufferKernelName);
            // NOTE: These shapes used to be `static`, i.e. computed once per template instantiation and then reused. That silently returned stale shapes after a different device/kernel was passed, after setBatchSize(),
            // or when another driver object of the same type was used. They are now recomputed on every call (kernel name lookup + shape copies), which may have performance implications in benchmarks.
            // If that shows up, cache them per driver object and invalidate the cache in setBatchSize().
            auto packedOutput = getOutputPackedShape(true, outputDeviceIndex, getOutputKernelIndexFromName(outputDeviceIndex, outputBufferKernelName));
            auto foldedOutput = getOutputFoldedShape(true, outputDeviceIndex, getOutputKernelIndexFromName(outputDeviceIndex, outputBufferKernelName));
            const Finn::DynamicMdSpan reshapedOutput(result.begin(), result.end(), packedOutput);
            auto unpacked = Finn::unpackMultiDimensionalOutputs<S, Finn::vector<uint8_t>::iterator, false, V>(result.begin(), result.end(), reshapedOutput, foldedOutput);
            return unpacked;
        }

        /**
         * @brief Get the results of a asynchronous inference
         *
         * @tparam V
         * @tparam typename
         * @return Finn::vector<V>
         */
        template<typename V = Finn::UnpackingAutoRetType::AutoRetType<S>, bool Sync = SynchronousInference, typename = std::enable_if_t<!Sync>>
        [[nodiscard]] Finn::vector<V> getResults() {
            // TODO(linusjun): maybe this method should block until data is available?
            auto result = accelerator.getOutputData(configuration.outputDeviceIndex, getOutputKernelName());
            // NOTE: These shapes used to be `static`, i.e. computed once per template instantiation and then reused. That silently returned stale shapes after the default device/kernel changed, after setBatchSize(),
            // or when another driver object of the same type was used. They are now recomputed on every call (shape copies), which may have performance implications in benchmarks.
            // If that shows up, cache them per driver object and invalidate the cache in setBatchSize().
            auto packedOutput = getOutputPackedShape(true);
            auto foldedOutput = getOutputFoldedShape(true);
            const Finn::DynamicMdSpan reshapedOutput(result.begin(), result.end(), packedOutput);
            auto unpacked = Finn::unpackMultiDimensionalOutputs<S, Finn::vector<uint8_t>::iterator, false, V>(result.begin(), result.end(), reshapedOutput, foldedOutput);

            return unpacked;
        }

        /**
         * @brief Drains the output buffer of the specified device. This is only available in asynchronous inference mode.
         */
        template<bool Sync = SynchronousInference, typename = std::enable_if_t<!Sync>>
        void drain(uint outputDeviceIndex, const std::string& outputBufferKernelName) {
            accelerator.drain(outputDeviceIndex, outputBufferKernelName);
        }

        /**
         * @brief Drains the output buffer of the output device. This is only available in asynchronous inference mode.
         *
         * @tparam typename
         */
        template<bool Sync = SynchronousInference, typename = std::enable_if_t<!Sync>>
        void drain() {
            accelerator.drain(configuration.outputDeviceIndex, getOutputKernelName());
        }

        /**
         * @brief Implements the synchronous inference operation
         *
         * @tparam IteratorType
         * @tparam V Return datatype, usually automatically determined
         * @tparam typename
         * @param first Iterator to first element of input
         * @param last  Iterator to end of input
         * @param inputDeviceIndex index of input FPGA
         * @param inputBufferKernelName name of input kernel
         * @param outputDeviceIndex index of output FPGA
         * @param outputBufferKernelName name of output kernel
         * @return Finn::vector<V>
         */
        template<typename IteratorType, typename V = Finn::UnpackingAutoRetType::AutoRetType<S>, bool Sync = SynchronousInference, typename = std::enable_if_t<Sync>>
        [[nodiscard]] Finn::vector<V> inferSynchronous(IteratorType first, IteratorType last, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint outputDeviceIndex, const std::string& outputBufferKernelName) {
            // NOTE: The shapes in this method used to be `static`, i.e. computed once per template instantiation and then reused. That silently returned stale shapes after a different device/kernel was passed,
            // after setBatchSize(), or when another driver object of the same type was used. They are now recomputed on every call (kernel name lookup + shape copies), which may have performance implications in
            // benchmarks. If that shows up, cache them per driver object and invalidate the cache in setBatchSize().
            auto foldedShape = getInputFoldedShape(true, inputDeviceIndex, getInputKernelIndexFromName(inputDeviceIndex, inputBufferKernelName));
            const Finn::DynamicMdSpan reshapedInput(first, last, foldedShape);

            auto packed = Finn::packMultiDimensionalInputs<F, IteratorType>(first, last, reshapedInput, foldedShape.back());

            auto result = infer(packed.begin(), packed.end(), inputDeviceIndex, inputBufferKernelName, outputDeviceIndex, outputBufferKernelName, batchElements);

            // NOTE: No longer static, see the note for foldedShape above.
            auto packedOutput = getOutputPackedShape(true, outputDeviceIndex, getOutputKernelIndexFromName(outputDeviceIndex, outputBufferKernelName));
            auto foldedOutput = getOutputFoldedShape(true, outputDeviceIndex, getOutputKernelIndexFromName(outputDeviceIndex, outputBufferKernelName));
            const Finn::DynamicMdSpan reshapedOutput(result.begin(), result.end(), packedOutput);
            auto unpacked = Finn::unpackMultiDimensionalOutputs<S, Finn::vector<uint8_t>::iterator, false, V>(result.begin(), result.end(), reshapedOutput, foldedOutput);
            return unpacked;
        }

        /**
         * @brief Implements the synchronous inference operation
         *
         * @tparam IteratorType
         * @tparam V
         * @tparam typename
         * @param first
         * @param last
         * @return Finn::vector<V>
         */
        template<typename IteratorType, typename V = Finn::UnpackingAutoRetType::AutoRetType<S>, bool Sync = SynchronousInference, typename = std::enable_if_t<Sync>>
        [[nodiscard]] Finn::vector<V> inferSynchronous(IteratorType first, IteratorType last) {
            return inferSynchronous(first, last, configuration.inputDeviceIndex, getInputKernelName(), configuration.outputDeviceIndex, getOutputKernelName());
        }

        /**
         * @brief Implements the synchronous inference operation
         *
         * @tparam U
         * @tparam V
         * @tparam typename
         * @param data
         * @param inputDeviceIndex
         * @param inputBufferKernelName
         * @param outputDeviceIndex
         * @param outputBufferKernelName
         * @return Finn::vector<V>
         */
        template<typename U, typename V = Finn::UnpackingAutoRetType::AutoRetType<S>, bool Sync = SynchronousInference, typename = std::enable_if_t<Sync>>
        [[nodiscard]] Finn::vector<V> inferSynchronous(const Finn::vector<U>& data, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint outputDeviceIndex, const std::string& outputBufferKernelName) {
            return inferSynchronous<typename Finn::vector<U>::const_iterator, V>(data.begin(), data.end(), inputDeviceIndex, inputBufferKernelName, outputDeviceIndex, outputBufferKernelName);
        }

        /**
         * @brief Implements the synchronous inference operation
         *
         * @tparam U
         * @tparam V
         * @tparam typename
         * @param data
         * @return Finn::vector<V>
         */
        template<typename U, typename V = Finn::UnpackingAutoRetType::AutoRetType<S>, bool Sync = SynchronousInference, typename = std::enable_if_t<Sync>>
        [[nodiscard]] Finn::vector<V> inferSynchronous(const Finn::vector<U>& data) {
            return inferSynchronous<U, V>(data, configuration.inputDeviceIndex, getInputKernelName(), configuration.outputDeviceIndex, getOutputKernelName());
        }

         protected:
        /**
         *
         * @brief Do an inference with the given data. This assumes already flattened data in uint8_t's. Specify inputs and outputs.
         *
         * @param first Iterator to first element in input sequence
         * @param last  Iterator to last element in input sequence
         * @param inputDeviceIndex
         * @param inputBufferKernelName
         * @param outputDeviceIndex
         * @param outputBufferKernelName
         * @param batchSize
         * @return Finn::vector<uint8_t>
         */
        template<typename IteratorType, bool Sync = SynchronousInference, typename = std::enable_if_t<Sync>>
        [[nodiscard]] Finn::vector<uint8_t> infer(IteratorType first, IteratorType last, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint outputDeviceIndex, const std::string& outputBufferKernelName, uint batchSize) {
            FINN_LOG_DEBUG(loglevel::info) << loggerPrefix() << "Starting inference (raw data). Input length: " << std::abs(std::distance(first, last));
            auto storeFunc = accelerator.storeFactory(inputDeviceIndex, inputBufferKernelName);

            if (std::abs(std::distance(first, last)) != getTotalDataSize(inputDeviceIndex, inputBufferKernelName)) {
                Finn::logAndError<std::runtime_error>(loggerPrefix() + " Input length (" + std::to_string(std::abs(std::distance(first, last))) + ") does not match up with batches*inputsize_per_batch (" +
                                                      std::to_string(getFeatureMapSize(inputDeviceIndex, inputBufferKernelName)) + "*" + std::to_string(batchSize) + "=" +
                                                      std::to_string(getTotalDataSize(inputDeviceIndex, inputBufferKernelName)) + ")");
            }

            if (!storeFunc(first, last)) {
                Finn::logAndError<std::runtime_error>(loggerPrefix() + "Input could not be stored in the device buffer");
            }

            accelerator.run();

#ifdef UNITTEST
            Finn::vector<uint8_t> data(first, last);
            FINN_LOG(loglevel::info) << "Readback from device buffer confirming data was written to board successfully: " << isSyncedDataEquivalent(inputDeviceIndex, inputBufferKernelName, data);
#endif
            if (!accelerator.wait()) {
                Finn::logAndError<std::runtime_error>(loggerPrefix() + "Waiting for the accelerator was interrupted");
            }

            FINN_LOG_DEBUG(loglevel::info) << "Reading out buffers";
            accelerator.read();
            return accelerator.getOutputData(outputDeviceIndex, outputBufferKernelName, getTotalDataSize(outputDeviceIndex, outputBufferKernelName));
        }

        /**
         *
         * @brief Do inference with the given data. This assumes already flattened data in uint8_t's. Specify inputs and outputs.
         *
         * @param data
         * @param inputDeviceIndex
         * @param inputBufferKernelName
         * @param outputDeviceIndex
         * @param outputBufferKernelName
         * @param batchSize
         * @return Finn::vector<uint8_t>
         */
        template<bool Sync = SynchronousInference, typename = std::enable_if_t<Sync>>
        [[nodiscard]] Finn::vector<uint8_t> infer(const Finn::vector<uint8_t>& data, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint outputDeviceIndex, const std::string& outputBufferKernelName, uint batchSize) {
            return infer(data.begin(), data.end(), inputDeviceIndex, inputBufferKernelName, outputDeviceIndex, outputBufferKernelName, batchSize);
        }

#ifdef UNITTEST
        /**
         * @brief Return whether the data that is currently held on the FPGA is equivalent to the passed data
         *
         * @param deviceIndex
         * @param bufferName
         * @param data
         * @return true
         * @return false
         */
        bool isSyncedDataEquivalent(uint deviceIndex, const std::string& bufferName, const Finn::vector<uint8_t>& data) {
            auto devInBuf = getInputBuffer(deviceIndex, bufferName);
            devInBuf->testSyncBackFromDevice();
            return devInBuf->testGetMap() == data;
        }

        /**
         * @brief Log out the entire structure of the driver (devices and their buffers)
         *
         */
        void logDriver() {
            FINN_LOG(loglevel::info) << loggerPrefix() << "Driver Overview:\n";
            for (DeviceHandler& devHandler : accelerator) {
                FINN_LOG(loglevel::info) << "\tDevice Index: " << devHandler.getDeviceIndex();
                for (auto& keyValuePair : devHandler.getInputBufferMap()) {
                    FINN_LOG(loglevel::info) << "\t\tInput buffers: ";
                    FINN_LOG(loglevel::info) << "\t\t\tName: " << keyValuePair.second->getName() << " (in hashmap as " << keyValuePair.first << ")";
                    FINN_LOG(loglevel::info) << "\t\t\tShape packed: " << FinnUtils::shapeToString(keyValuePair.second->getPackedShape());
                    FINN_LOG(loglevel::info) << "\t\t\tElements of type T (usually uint8_t) per sample: " << keyValuePair.second->getFeatureMapSize();
                    FINN_LOG(loglevel::info) << "\t\t\tElements of type T (usually uint8_t) in buffer overall: " << keyValuePair.second->getTotalDataSize();
                }
                for (auto& keyValuePair : devHandler.getOutputBufferMap()) {
                    FINN_LOG(loglevel::info) << "\t\tOutput buffers: ";
                    FINN_LOG(loglevel::info) << "\t\t\tName: " << keyValuePair.second->getName() << " (in hashmap as " << keyValuePair.first << ")";
                    FINN_LOG(loglevel::info) << "\t\t\tShape packed: " << FinnUtils::shapeToString(keyValuePair.second->getPackedShape());
                    FINN_LOG(loglevel::info) << "\t\t\tElements of type T (usually uint8_t) per sample: " << keyValuePair.second->getFeatureMapSize();
                    FINN_LOG(loglevel::info) << "\t\t\tElements of type T (usually uint8_t) in buffer overall: " << keyValuePair.second->getTotalDataSize();
                }
            }
        }
#endif
    };
}  // namespace Finn

#endif  // BASEDRIVER_H