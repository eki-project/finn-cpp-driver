#include "DeviceHandler.h"

#include <xrt/xrt_device.h>  // for device
#include <xrt/xrt_kernel.h>  // for kernel

#include <boost/cstdint.hpp>  // for uint8_t
#include <chrono>
#include <iosfwd>
#include <memory>
#include <stdexcept>
#include <system_error>

#include "../utils/ConfigurationStructs.h"
#include "../utils/Logger.h"  // for operator<<, FINN_LOG, FINN_DEBUG_LOG
#include "../utils/Types.h"   // for shape_t


namespace fs = std::filesystem;

namespace Finn {
    DeviceHandler::DeviceHandler(const DeviceWrapper& devWrap, unsigned int hostBufferSize = 100) : xrtDeviceIndex(devWrap.xrtDeviceIndex), xclbinPath(devWrap.xclbin) {
        checkDeviceWrapper(devWrap);
        initializeDevice();
        loadXclbinSetUUID();
        initializeBufferObjects(devWrap, hostBufferSize);
        FINN_LOG(Logger::getLogger(), loglevel::info) << "Finished setting up device " << xrtDeviceIndex;
    }

    /****** INITIALIZERS ******/
    void DeviceHandler::checkDeviceWrapper(const DeviceWrapper& devWrap) {
        if (devWrap.xclbin.empty()) {
            throw fs::filesystem_error("Empty filepath to xclbin. Abort.", std::error_code());
        }
        if (!fs::exists(devWrap.xclbin) || !fs::is_regular_file(devWrap.xclbin)) {
            throw fs::filesystem_error("File " + std::string(devWrap.xclbin.c_str()) + " not found. Abort.", std::error_code());
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
        FINN_LOG(Logger::getLogger(), loglevel::info) << "(" << xrtDeviceIndex << ") "
                                                      << "Initializing xrt::device, loading xclbin and assigning IP\n";
        device = xrt::device(xrtDeviceIndex);
    }

    void DeviceHandler::loadXclbinSetUUID() {
        FINN_LOG(Logger::getLogger(), loglevel::info) << "(" << xrtDeviceIndex << ") "
                                                      << "Loading XCLBIN and setting uuid\n";
        uuid = device.load_xclbin(xclbinPath);
    }

    void DeviceHandler::initializeBufferObjects(const DeviceWrapper& devWrap, unsigned int hostBufferSize) {
        FINN_LOG(Logger::getLogger(), loglevel::info) << "(" << xrtDeviceIndex << ") "
                                                      << "Initializing buffer objects\n";
        for (auto&& ebdptr : devWrap.idmas) {
            auto tmpKern = xrt::kernel(device, uuid, ebdptr->kernelName, xrt::kernel::cu_access_mode::shared);
            inputBufferMap.emplace(std::make_pair(ebdptr->kernelName, Finn::DeviceInputBuffer<uint8_t>(ebdptr->kernelName, device, tmpKern, ebdptr->packedShape, hostBufferSize)));
        }
        for (auto&& ebdptr : devWrap.odmas) {
            auto tmpKern = xrt::kernel(device, uuid, ebdptr->kernelName, xrt::kernel::cu_access_mode::exclusive);
            outputBufferMap.emplace(std::make_pair(ebdptr->kernelName, Finn::DeviceOutputBuffer<uint8_t>(ebdptr->kernelName, device, tmpKern, ebdptr->packedShape, hostBufferSize)));
        }
        FINN_LOG(Logger::getLogger(), loglevel::info) << "Finished initializing buffer objects on device " << xrtDeviceIndex;
#ifndef NDEBUG
        for (size_t index = 0; index < inputBufferMap.bucket_count(); ++index) {
            if (inputBufferMap.bucket_size(index) > 1) {
                FINN_LOG_DEBUG(Logger::getLogger(), loglevel::error) << "(" << xrtDeviceIndex << ") "
                                                                     << "Hash collision in inputBufferMap. This access to the inputBufferMap is no longer constant time!";
            }
        }
        for (size_t index = 0; index < outputBufferMap.bucket_count(); ++index) {
            if (outputBufferMap.bucket_size(index) > 1) {
                FINN_LOG_DEBUG(Logger::getLogger(), loglevel::error) << "(" << xrtDeviceIndex << ") "
                                                                     << "Hash collision in outputBufferMap. This access to the outputBufferMap is no longer constant time!";
            }
        }
#endif
    }

    /****** GETTER / SETTER ******/
    [[maybe_unused]] xrt::device& DeviceHandler::getDevice() { return device; }

    [[maybe_unused]] bool DeviceHandler::containsBuffer(const std::string& kernelBufferName, IO ioMode) {
        if (ioMode == IO::INPUT) {
            return inputBufferMap.contains(kernelBufferName);
        } else if (ioMode == IO::OUTPUT) {
            return outputBufferMap.contains(kernelBufferName);
        }
        return false;
    }

    std::unordered_map<std::string, DeviceInputBuffer<uint8_t>>& DeviceHandler::getInputBufferMap() {
        return inputBufferMap;
    }

    std::unordered_map<std::string, DeviceOutputBuffer<uint8_t>>& DeviceHandler::getOutputBufferMap() {
        return outputBufferMap;
    }

    /****** USER METHODS ******/
    bool DeviceHandler::store(const std::vector<uint8_t>& data, const std::string& inputBufferKernelName) {
        if (!inputBufferMap.contains(inputBufferKernelName)) {
            std::string existingNames = "Existing buffer names: \n";
            for (auto& nm : inputBufferMap) {
                existingNames += nm.first + "\n";
            }
            FinnUtils::logAndError<std::runtime_error>("[store] Tried accessing kernel/buffer with name " + inputBufferKernelName + " but this kernel / buffer does not exist! " + existingNames);
        }
        return inputBufferMap.at(inputBufferKernelName).store(data);
    }

    bool DeviceHandler::storeUnchecked(const std::vector<uint8_t>& data, const std::string& inputBufferKernelName) { return inputBufferMap.at(inputBufferKernelName).store(data); }

    [[maybe_unused]] unsigned int DeviceHandler::getDeviceIndex() const { return xrtDeviceIndex; }

    bool DeviceHandler::run(const std::string& inputBufferKernelName) {
        if (!inputBufferMap.contains(inputBufferKernelName)) {
            std::string existingNames = "Existing buffer names: \n";
            for (auto& nm : inputBufferMap) {
                existingNames += nm.first + "\n";
            }
            FinnUtils::logAndError<std::runtime_error>("[run] Tried accessing kernel/buffer with name " + inputBufferKernelName + " but this kernel / buffer does not exist! " + existingNames);
        }
        return inputBufferMap.at(inputBufferKernelName).run();
    }

    std::vector<std::vector<uint8_t>> DeviceHandler::retrieveResults(const std::string& outputBufferKernelName, bool forceArchival) {
        if (!outputBufferMap.contains(outputBufferKernelName)) {
            std::string existingNames = "Existing buffer names: \n";
            for (auto& nm : outputBufferMap) {
                existingNames += nm.first + "\n";
            }
            FinnUtils::logAndError<std::runtime_error>("[retrieve] Tried accessing kernel/buffer with name " + outputBufferKernelName + " but this kernel / buffer does not exist! " + existingNames);
        }
        if (forceArchival) {
            outputBufferMap.at(outputBufferKernelName).archiveValidBufferParts();
        }
        return outputBufferMap.at(outputBufferKernelName).retrieveArchive();
    }

    ert_cmd_state DeviceHandler::read(const std::string& outputBufferKernelName, unsigned int samples) {
        if (!outputBufferMap.contains(outputBufferKernelName)) {
            std::string existingNames = "Existing buffer names: \n";
            for (auto& nm : outputBufferMap) {
                existingNames += nm.first + "\n";
            }
            FinnUtils::logAndError<std::runtime_error>("[readread] Tried accessing kernel/buffer with name " + outputBufferKernelName + " but this kernel / buffer does not exist! " + existingNames);
        }
        return outputBufferMap.at(outputBufferKernelName).read(samples);
    }

    size_t DeviceHandler::size(SIZE_SPECIFIER ss, const std::string& bufferName) {
        if (inputBufferMap.contains(bufferName)) {
            return inputBufferMap.at(bufferName).size(ss);
        } else if (outputBufferMap.contains(bufferName)) {
            return outputBufferMap.at(bufferName).size(ss);
        }
        return 0;
    }

    

    #ifdef NDEBUG
    DeviceInputBuffer<uint8_t>& DeviceHandler::getInputBuffer(const std::string& name) {
        return inputBufferMap.at(name);
    }

    #endif
}  // namespace Finn