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
#include <FINNCppDriver/utils/Logger.h>
#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/core/DeviceBuffer/AsyncDeviceBuffers.hpp>
#include <FINNCppDriver/core/DeviceBuffer/DeviceBuffer.hpp>
#include <FINNCppDriver/core/DeviceBuffer/SyncDeviceBuffers.hpp>
#include <algorithm>  // for copy
#include <boost/cstdint.hpp>
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

#include "xrt/xrt_device.h"
#include "xrt/xrt_uuid.h"  // for uuid


namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace Finn {
    DeviceHandler::DeviceHandler(const DeviceWrapper& devWrap, bool pSynchronousInference, unsigned int hostBufferSize)
        : synchronousInference(pSynchronousInference), devInformation(devWrap), xrtDeviceIndex(devWrap.xrtDeviceIndex), xclbinPath(devWrap.xclbin) {
        checkDeviceWrapper(devWrap);
        initializeDevice();
        loadXclbinSetUUID();
        initializeBufferObjects(devWrap, hostBufferSize, pSynchronousInference);
        FINN_LOG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "Finished setting up device " << xrtDeviceIndex;
    }

    std::string DeviceHandler::loggerPrefix() { return "[DeviceHandler] "; }

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
        FINN_LOG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "(" << xrtDeviceIndex << ") "
                                                      << "Initializing xrt::device, loading xclbin and assigning IP\n";
        device = xrt::device(xrtDeviceIndex);
    }

    void DeviceHandler::loadXclbinSetUUID() {
        FINN_LOG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "(" << xrtDeviceIndex << ") "
                                                      << "Loading XCLBIN and setting uuid\n";
        uuid = device.load_xclbin(xclbinPath);
    }

    void DeviceHandler::initializeBufferObjects(const DeviceWrapper& devWrap, unsigned int hostBufferSize, bool pSynchronousInference) {
        FINN_LOG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "(" << xrtDeviceIndex << ") "
                                                      << "Initializing buffer objects with buffer size " << hostBufferSize << "\n";
        for (auto&& ebdptr : devWrap.idmas) {
            if (pSynchronousInference) {
                inputBufferMap.emplace(std::make_pair(ebdptr->kernelName, std::make_shared<Finn::SyncDeviceInputBuffer<uint8_t>>(ebdptr->kernelName, device, uuid, ebdptr->packedShape, hostBufferSize)));
            } else {
                inputBufferMap.emplace(std::make_pair(ebdptr->kernelName, std::make_shared<Finn::AsyncDeviceInputBuffer<uint8_t>>(ebdptr->kernelName, device, uuid, ebdptr->packedShape, hostBufferSize)));
            }
        }
        for (auto&& ebdptr : devWrap.odmas) {
            if (pSynchronousInference) {
                auto ptr = std::make_shared<Finn::SyncDeviceOutputBuffer<uint8_t>>(ebdptr->kernelName, device, uuid, ebdptr->packedShape, hostBufferSize);
                outputBufferMap.emplace(std::make_pair(ebdptr->kernelName, ptr));
            } else {
                auto ptr = std::make_shared<Finn::AsyncDeviceOutputBuffer<uint8_t>>(ebdptr->kernelName, device, uuid, ebdptr->packedShape, hostBufferSize);
                ptr->allocateLongTermStorage(hostBufferSize * 5);
                outputBufferMap.emplace(std::make_pair(ebdptr->kernelName, ptr));
            }
        }
        FINN_LOG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "Finished initializing buffer objects on device " << xrtDeviceIndex;

#ifndef NDEBUG
        isBufferMapCollisionFree();
#endif
    }

    /****** GETTER / SETTER ******/

    void DeviceHandler::setBatchSize(uint pBatchsize) {
        if (this->batchsize == pBatchsize) {
            return;
        } else {
            FINN_LOG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "(" << xrtDeviceIndex << ") "
                                                          << "Change batch size to " << pBatchsize << "\n";
            this->batchsize = pBatchsize;
            inputBufferMap.clear();
            outputBufferMap.clear();

            std::this_thread::sleep_for(2000ms);
            initializeBufferObjects(this->devInformation, pBatchsize, this->synchronousInference);
        }
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
            ret &= value->run();
        }
        // cppcheck-suppress unusedVariable
        for (auto&& [key, value] : inputBufferMap) {
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


    [[maybe_unused]] Finn::vector<uint8_t> DeviceHandler::retrieveResults(const std::string& outputBufferKernelName, bool forceArchival) {
        if (!outputBufferMap.contains(outputBufferKernelName)) {
            auto newlineFold = [](std::string a, const auto& b) { return std::move(a) + '\n' + std::move(b.first); };
            std::string existingNames = "Existing buffer names:";
            std::accumulate(inputBufferMap.begin(), inputBufferMap.end(), existingNames, newlineFold);
            FinnUtils::logAndError<std::runtime_error>(loggerPrefix() + " [retrieve] Tried accessing kernel/buffer with name " + outputBufferKernelName + " but this kernel / buffer does not exist! " + existingNames);
        }
        if (forceArchival) {
            // TODO(linusjun): Fix for asynchronous inference
            // outputBufferMap.at(outputBufferKernelName)->archiveValidBufferParts();
        }
        return outputBufferMap.at(outputBufferKernelName)->getData();
    }

    size_t DeviceHandler::size(SIZE_SPECIFIER ss, const std::string& bufferName) {
        if (inputBufferMap.contains(bufferName)) {
            return inputBufferMap.at(bufferName)->size(ss);
        } else if (outputBufferMap.contains(bufferName)) {
            return outputBufferMap.at(bufferName)->size(ss);
        }
        return 0;
    }


#ifndef NDEBUG
    bool DeviceHandler::isBufferMapCollisionFree() {
        bool collisionFound = false;
        for (size_t index = 0; index < inputBufferMap.bucket_count(); ++index) {
            if (inputBufferMap.bucket_size(index) > 1) {
                FINN_LOG_DEBUG(Logger::getLogger(), loglevel::error) << loggerPrefix() << "(" << xrtDeviceIndex << ") "
                                                                     << "Hash collision in inputBufferMap. This access to the inputBufferMap is no longer constant time!";
                collisionFound = true;
            }
        }
        for (size_t index = 0; index < outputBufferMap.bucket_count(); ++index) {
            if (outputBufferMap.bucket_size(index) > 1) {
                FINN_LOG_DEBUG(Logger::getLogger(), loglevel::error) << loggerPrefix() << "(" << xrtDeviceIndex << ") "
                                                                     << "Hash collision in outputBufferMap. This access to the outputBufferMap is no longer constant time!";
                collisionFound = true;
            }
        }
        return collisionFound;
    }
#endif
}  // namespace Finn