/**
 * @file DeviceHandler.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Encapsulates and manages a complete FPGA device
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <FINNCppDriver/core/DeviceHandler.h>
#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/core/DeviceBuffer/AsyncDeviceBuffers.hpp>
#include <FINNCppDriver/core/DeviceBuffer/DeviceBuffer.hpp>
#include <FINNCppDriver/core/DeviceBuffer/SyncDeviceBuffers.hpp>
#include <FINNCppDriver/utils/FPGAReset.hpp>
#include <FINNCppDriver/utils/Logger.hpp>
#include <algorithm>  // for copy
#include <cerrno>
#include <chrono>
#include <filesystem>  // for path
#include <iosfwd>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <system_error>
#include <utility>  // for move
#include <vector>   // for vector

#include "experimental/xrt_xclbin.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_uuid.h"  // for uuid


namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace Finn {
    DeviceHandler::DeviceHandler(const DeviceWrapper& devWrap, bool pSynchronousInference, unsigned int hostBufferSize)
        : synchronousInference(pSynchronousInference), devInformation(devWrap), batchsize(hostBufferSize), xrtDeviceIndex(devWrap.xrtDeviceIndex), xclbinPath(devWrap.xclbin) {
        checkDeviceWrapper(devWrap);
        initializeDevice();
        loadXclbinSetUUID();
        initializeBufferObjects(devWrap, hostBufferSize, pSynchronousInference);
        FINN_LOG(loglevel::info) << "Finished setting up device " << xrtDeviceIndex;
    }

    /****** INITIALIZERS ******/
    void DeviceHandler::checkDeviceWrapper(const DeviceWrapper& devWrap) {
        // Execute tests on filepath for xclbin in release mode!
        if (devWrap.xclbin.empty()) {
            throw fs::filesystem_error("Empty filepath to xclbin. Abort.", std::error_code(ENOENT, std::generic_category()));
        }
        if (!fs::exists(devWrap.xclbin) || !fs::is_regular_file(devWrap.xclbin)) {
            throw fs::filesystem_error("File " + std::string(fs::absolute(devWrap.xclbin).c_str()) + " not found. Abort.", std::error_code(ENOENT, std::generic_category()));
        }
        if (devWrap.idmas.empty()) {
            throw std::invalid_argument("Empty input kernel list. Abort.");
        }
        for (auto&& bufDesc : devWrap.idmas) {
            if (bufDesc->kernelName.empty()) {
                throw std::invalid_argument("Empty kernel name. Abort.");
            }
            if (bufDesc->packedShape.empty()) {
                throw std::invalid_argument("Empty buffer shape. Abort.");
            }
        }
        if (devWrap.odmas.empty()) {
            throw std::invalid_argument("Empty output kernel list. Abort.");
        }
        for (auto&& bufDesc : devWrap.odmas) {
            if (bufDesc->kernelName.empty()) {
                throw std::invalid_argument("Empty kernel name. Abort.");
            }
            if (bufDesc->packedShape.empty()) {
                throw std::invalid_argument("Empty buffer shape. Abort.");
            }
        }
    }

    void DeviceHandler::initializeDevice() {
        FINN_LOG(loglevel::info) << "(" << xrtDeviceIndex << ") "
                                 << "Initializing xrt::device, loading xclbin and assigning IP\n";
        try {
            resetFPGAS(static_cast<int>(xrtDeviceIndex));
        } catch (const std::exception& e) {
            FINN_LOG(loglevel::error) << "Failed to reset FPGA: " << e.what();
            throw;  // Rethrow the exception to propagate the error
        }
        device = xrt::device(xrtDeviceIndex);
    }

    void DeviceHandler::loadXclbinSetUUID() {
        FINN_LOG(loglevel::info) << "(" << xrtDeviceIndex << ") "
                                 << "Loading XCLBIN and setting uuid\n";
        uuid = device.load_xclbin(xclbinPath);
    }

    namespace {
        /**
         * @brief Memory group (bank) that the memory port of a DMA compute unit reaches, from the xclbin metadata
         *
         * The build may place each DMA in a bank of its own, and a DMA only reaches the bank of its port. The metadata is read
         * from the file because an xrt::kernel destroyed before the buffers are allocated breaks every later allocation.
         *
         * @param xclbin Loaded xclbin
         * @param kernelName Compute unit as the configuration names it ("kernel:{cu}")
         * @return unsigned int Memory group to allocate the DMA's buffer in
         */
        unsigned int memoryGroup(const xrt::xclbin& xclbin, const std::string& kernelName) {
            std::string cuName = kernelName;  // the xclbin names it "kernel:cu"
            std::erase(cuName, '{');
            std::erase(cuName, '}');
            for (const auto& ip : xclbin.get_ips()) {
                if (ip.get_name() != cuName) {
                    continue;
                }
                for (const auto& arg : ip.get_args()) {
                    for (const auto& mem : arg.get_mems()) {
                        if (mem.get_type() != xrt::xclbin::mem::memory_type::streaming_connection) {
                            return static_cast<unsigned int>(mem.get_index());
                        }
                    }
                }
                throw std::runtime_error("Compute unit " + kernelName + " has no memory port");
            }
            throw std::runtime_error("Compute unit " + kernelName + " is not in the xclbin");
        }
    }  // namespace

    void DeviceHandler::initializeBufferObjects(const DeviceWrapper& devWrap, unsigned int hostBufferSize, bool pSynchronousInference) {
        FINN_LOG(loglevel::info) << "(" << xrtDeviceIndex << ") "
                                 << "Initializing buffer objects with buffer size " << hostBufferSize << "\n";
        // Orders asynchronous batch starts after the previous batch's completion; outputs are created first so that every
        // output is registered before the first input worker consults the gate
        auto gate = pSynchronousInference ? nullptr : std::make_shared<Finn::detail::BatchGate>(devWrap.idmas.size(), devWrap.odmas.size());
        const xrt::xclbin xclbin(xclbinPath);
        for (auto&& ebdptr : devWrap.odmas) {
            if (pSynchronousInference) {
                auto ptr = std::make_shared<Finn::SyncDeviceOutputBuffer<uint8_t>>(ebdptr->kernelName, device, uuid, ebdptr->packedShape, hostBufferSize, ebdptr->ring, memoryGroup(xclbin, ebdptr->kernelName));
                outputBufferMap.emplace(std::make_pair(ebdptr->kernelName, ptr));
            } else {
                auto ptr = std::make_shared<Finn::AsyncDeviceOutputBuffer<uint8_t>>(ebdptr->kernelName, device, uuid, ebdptr->packedShape, hostBufferSize, gate, ebdptr->ring, memoryGroup(xclbin, ebdptr->kernelName));
                outputBufferMap.emplace(std::make_pair(ebdptr->kernelName, ptr));
            }
        }
        for (auto&& ebdptr : devWrap.idmas) {
            if (pSynchronousInference) {
                inputBufferMap.emplace(
                    std::make_pair(ebdptr->kernelName, std::make_shared<Finn::SyncDeviceInputBuffer<uint8_t>>(ebdptr->kernelName, device, uuid, ebdptr->packedShape, hostBufferSize, ebdptr->ring, memoryGroup(xclbin, ebdptr->kernelName))));
            } else {
                inputBufferMap.emplace(std::make_pair(
                    ebdptr->kernelName, std::make_shared<Finn::AsyncDeviceInputBuffer<uint8_t>>(ebdptr->kernelName, device, uuid, ebdptr->packedShape, hostBufferSize, gate, ebdptr->ring, memoryGroup(xclbin, ebdptr->kernelName))));
            }
        }
        FINN_LOG(loglevel::info) << "Finished initializing buffer objects on device " << xrtDeviceIndex;

#ifndef NDEBUG
        isBufferMapCollisionFree();
#endif
    }

    /****** GETTER / SETTER ******/

    void DeviceHandler::setBatchSize(uint pBatchsize) {
        if (this->batchsize == pBatchsize) {
            return;
        }
        FINN_LOG(loglevel::info) << "(" << xrtDeviceIndex << ") "
                                 << "Change batch size to " << pBatchsize << "\n";
        this->batchsize = pBatchsize;
        shutdownBuffers();
        // XRT 2.16 quirk: once the last xrt::ip of a device handle is destroyed, every further buffer allocation on that
        // handle fails with "failed to allocate userptr bo: Operation not permitted", and reloading the xclbin on the
        // same handle does not help. Re-opening the handle (no FPGA reset) and reloading the already programmed xclbin
        // restores a working state; verified on U55C / XRT 2.16.13.
        device = xrt::device(xrtDeviceIndex);
        loadXclbinSetUUID();
        initializeBufferObjects(this->devInformation, pBatchsize, this->synchronousInference);
    }

    void DeviceHandler::shutdownBuffers() {
        // Stop worker threads first so no buffer is destroyed while its thread still uses it
        for (auto& [name, buffer] : inputBufferMap) {
            buffer->prepareForShutdown();
        }
        for (auto& [name, buffer] : outputBufferMap) {
            buffer->prepareForShutdown();
        }
        inputBufferMap.clear();
        outputBufferMap.clear();
    }

    [[maybe_unused]] xrt::device& DeviceHandler::getDevice() { return device; }

    [[maybe_unused]] bool DeviceHandler::containsBuffer(const std::string& kernelBufferName, IO ioMode) {
        if (ioMode == IO::INPUT) {
            return inputBufferMap.contains(kernelBufferName);
        } else if (ioMode == IO::OUTPUT) {
            return outputBufferMap.contains(kernelBufferName);
        }
        return false;
    }

    [[maybe_unused]] std::unordered_map<std::string, std::shared_ptr<DeviceInputBuffer<uint8_t>>>& DeviceHandler::getInputBufferMap() { return inputBufferMap; }

    [[maybe_unused]] std::unordered_map<std::string, std::shared_ptr<DeviceOutputBuffer<uint8_t>>>& DeviceHandler::getOutputBufferMap() { return outputBufferMap; }

    [[maybe_unused]] std::shared_ptr<DeviceInputBuffer<uint8_t>>& DeviceHandler::getInputBuffer(const std::string& name) { return inputBufferMap.at(name); }

    [[maybe_unused]] std::shared_ptr<DeviceOutputBuffer<uint8_t>>& DeviceHandler::getOutputBuffer(const std::string& name) { return outputBufferMap.at(name); }

    [[maybe_unused]] unsigned int DeviceHandler::getDeviceIndex() const { return xrtDeviceIndex; }

    bool DeviceHandler::run() {
        // Start the output kernels before the input to overlap the execution in a better way
        bool ret = true;
        // cppcheck-suppress unusedVariable
        for (auto&& [key, value] : outputBufferMap) {
            FINN_LOG_DEBUG(loglevel::info) << "Running kernel: " << key;
            ret &= value->run();
        }
        // cppcheck-suppress unusedVariable
        for (auto&& [key, value] : inputBufferMap) {
            FINN_LOG_DEBUG(loglevel::info) << "Running kernel: " << key;
            ret &= value->run();
        }
        return ret;
    }

    bool DeviceHandler::wait() {
        // We only need to wait for the outputs, because inputs have to finish before outputs
        bool ret = true;
        // cppcheck-suppress unusedVariable
        for (auto&& [key, value] : outputBufferMap) {
            ret &= value->wait();
        }
        return ret;
    }

    bool DeviceHandler::read() {
        // Sync data back from the FPGA
        bool ret = true;
        // cppcheck-suppress unusedVariable
        for (auto&& [key, value] : outputBufferMap) {
            ret &= value->read();
        }
        return ret;
    }


    [[maybe_unused]] Finn::vector<uint8_t> DeviceHandler::retrieveResults(const std::string& outputBufferKernelName, const std::size_t& numItems) {
        if (!outputBufferMap.contains(outputBufferKernelName)) {
            auto newlineFold = [](std::string a, const auto& b) { return std::move(a) + '\n' + b.first; };
            const std::string existingNames = std::accumulate(outputBufferMap.begin(), outputBufferMap.end(), std::string("Existing output buffer names:"), newlineFold);
            Finn::logAndError<std::runtime_error>("Tried accessing kernel/buffer with name " + outputBufferKernelName + " but this kernel / buffer does not exist! " + existingNames);
        }
        return outputBufferMap.at(outputBufferKernelName)->getData(numItems);
    }

    size_t DeviceHandler::getSizeInBytes(const std::string& bufferName) {
        if (inputBufferMap.contains(bufferName)) {
            return inputBufferMap.at(bufferName)->getSizeInBytes();
        } else if (outputBufferMap.contains(bufferName)) {
            return outputBufferMap.at(bufferName)->getSizeInBytes();
        }
        return 0;
    }

    size_t DeviceHandler::getFeatureMapSize(const std::string& bufferName) {
        if (inputBufferMap.contains(bufferName)) {
            return inputBufferMap.at(bufferName)->getFeatureMapSize();
        } else if (outputBufferMap.contains(bufferName)) {
            return outputBufferMap.at(bufferName)->getFeatureMapSize();
        }
        return 0;
    }

    size_t DeviceHandler::getBatchSize(const std::string& bufferName) {
        if (inputBufferMap.contains(bufferName)) {
            return inputBufferMap.at(bufferName)->getBatchSize();
        } else if (outputBufferMap.contains(bufferName)) {
            return outputBufferMap.at(bufferName)->getBatchSize();
        }
        return 0;
    }

    size_t DeviceHandler::getTotalDataSize(const std::string& bufferName) {
        if (inputBufferMap.contains(bufferName)) {
            return inputBufferMap.at(bufferName)->getTotalDataSize();
        } else if (outputBufferMap.contains(bufferName)) {
            return outputBufferMap.at(bufferName)->getTotalDataSize();
        }
        return 0;
    }

    void DeviceHandler::registerCallback(const std::string& bufferName, std::function<void(std::size_t)> callback) {
        if (inputBufferMap.contains(bufferName)) {
            Finn::logAndError<std::runtime_error>("Tried registering a callback on an input buffer! This is not allowed! Queried KernelBufferName: " + bufferName);
        } else if (outputBufferMap.contains(bufferName)) {
            outputBufferMap.at(bufferName)->registerCallback(callback);
        } else {
            Finn::logAndError<std::runtime_error>("Tried registering a callback on a buffer which does not exist! Queried KernelBufferName: " + bufferName);
        }
    }

    void DeviceHandler::drain(const std::string& bufferName) {
        if (inputBufferMap.contains(bufferName)) {
            Finn::logAndError<std::runtime_error>("Tried draining an input buffer! This is not allowed! Queried KernelBufferName: " + bufferName);
        } else if (outputBufferMap.contains(bufferName)) {
            outputBufferMap.at(bufferName)->drain();
        } else {
            Finn::logAndError<std::runtime_error>("Tried draining a buffer which does not exist! Queried KernelBufferName: " + bufferName);
        }
    }

#ifndef NDEBUG
    bool DeviceHandler::isBufferMapCollisionFree() {
        bool collisionFound = false;
        for (size_t index = 0; index < inputBufferMap.bucket_count(); ++index) {
            if (inputBufferMap.bucket_size(index) > 1) {
                FINN_LOG_DEBUG(loglevel::error) << "(" << xrtDeviceIndex << ") "
                                                << "Hash collision in inputBufferMap. This access to the inputBufferMap is no longer constant time!";
                collisionFound = true;
            }
        }
        for (size_t index = 0; index < outputBufferMap.bucket_count(); ++index) {
            if (outputBufferMap.bucket_size(index) > 1) {
                FINN_LOG_DEBUG(loglevel::error) << "(" << xrtDeviceIndex << ") "
                                                << "Hash collision in outputBufferMap. This access to the outputBufferMap is no longer constant time!";
                collisionFound = true;
            }
        }
        return collisionFound;
    }
#endif
}  // namespace Finn