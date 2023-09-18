#include "DeviceHandler.h"

#include <stdexcept>
#include <utility>

#include "../utils/Logger.h"

namespace fs = std::filesystem;

namespace Finn {
    DeviceHandler::DeviceHandler(const fs::path& xclbinPath, const std::string& pName, const std::size_t deviceIndex, const std::vector<BufferDescriptor>& inputBufDescr, const std::vector<BufferDescriptor>& outputBufDescr,
                                 const std::size_t hostBufferSize)
        : name(pName) {
        if (xclbinPath.empty()) {
            throw fs::filesystem_error("Empty filepath to xclbin. Abort.", std::error_code());
        }
        if (!fs::exists(xclbinPath) || !fs::is_regular_file(xclbinPath)) {
            throw fs::filesystem_error("File " + std::string(xclbinPath.c_str()) + " not found. Abort.", std::error_code());
        }
        if (pName.empty()) {
            throw std::invalid_argument("Empty device name. Abort.");
        }
        if (inputBufDescr.empty()) {
            throw std::invalid_argument("Empty input kernel list. Abort.");
        }
        for (auto&& bufDesc : inputBufDescr) {
            if (bufDesc.kernelName.empty()) {
                throw std::invalid_argument("Empty kernel name. Abort.");
            }
            if (bufDesc.elementShape.empty()) {
                throw std::invalid_argument("Empty buffer shape. Abort.");
            }
        }
        if (outputBufDescr.empty()) {
            throw std::invalid_argument("Empty output kernel list. Abort.");
        }
        for (auto&& bufDesc : outputBufDescr) {
            if (bufDesc.kernelName.empty()) {
                throw std::invalid_argument("Empty kernel name. Abort.");
            }
            if (bufDesc.elementShape.empty()) {
                throw std::invalid_argument("Empty buffer shape. Abort.");
            }
        }
        initializeDevice(deviceIndex, xclbinPath);
        initializeBufferObjects(inputBufDescr, outputBufDescr, hostBufferSize);
    }

    void DeviceHandler::initializeDevice(const std::size_t deviceIndex, const fs::path& xclbinPath) {
        auto log = Logger::getLogger();
        FINN_LOG(log, loglevel::info) << "(" << name << ") "
                                      << "Initializing xrt::device, loading xclbin and assigning IP\n";
        device = xrt::device(static_cast<unsigned int>(deviceIndex));
        uuid = device.load_xclbin(xclbinPath);
        FINN_LOG(log, loglevel::info) << "(" << name << ") "
                                      << "Successfully initialized device programmed device.\n";
    }

    void DeviceHandler::initializeBufferObjects(const std::vector<BufferDescriptor>& inputBufDescr, const std::vector<BufferDescriptor>& outputBufDescr, const std::size_t hostBufferSize) {
        auto log = Logger::getLogger();
        FINN_LOG_DEBUG(log, loglevel::info) << "(" << name << ") "
                                            << "Initializing buffer objects\n";
        std::size_t index = 0;
        for (auto&& bufDesc : inputBufDescr) {
            auto tmpKern = xrt::kernel(device, uuid, bufDesc.kernelName);
            inputBufferMap.emplace(std::make_pair(bufDesc.kernelName, Finn::DeviceInputBuffer<uint8_t>(name + "_InputBuffer_" + std::to_string(index++), device, tmpKern, bufDesc.elementShape, static_cast<unsigned int>(hostBufferSize))));
        }
        index = 0;
        for (auto&& bufDesc : outputBufDescr) {
            auto tmpKern = xrt::kernel(device, uuid, bufDesc.kernelName);
            outputBufferMap.emplace(std::make_pair(bufDesc.kernelName, Finn::DeviceOutputBuffer<uint8_t>(name + "_OutputBuffer_" + std::to_string(index++), device, tmpKern, bufDesc.elementShape, static_cast<unsigned int>(hostBufferSize))));
        }

#ifndef NDEBUG
        for (index = 0; index < inputBufferMap.bucket_count(); ++index) {
            if (inputBufferMap.bucket_size(index) > 1) {
                FINN_LOG_DEBUG(log, loglevel::error) << "(" << name << ") "
                                                     << "Hash collision in inputBufferMap. This access to the inputBufferMap is no longer constant time!";
            }
        }
        for (index = 0; index < outputBufferMap.bucket_count(); ++index) {
            if (outputBufferMap.bucket_size(index) > 1) {
                FINN_LOG_DEBUG(log, loglevel::error) << "(" << name << ") "
                                                     << "Hash collision in outputBufferMap. This access to the outputBufferMap is no longer constant time!";
            }
        }
#endif

        FINN_LOG_DEBUG(log, loglevel::info) << "(" << name << ") "
                                            << "Device Buffers successfully initialised\n";
    }

    bool DeviceHandler::write(const std::vector<uint8_t>& inputVec) { return inputBufferMap.begin()->second.store(inputVec) && inputBufferMap.begin()->second.run(); }

    bool DeviceHandler::write(const std::vector<uint8_t>& inputVec, const std::string& inputBufferName) {
        // Access using at is important, because operator[] may modify Map
        return inputBufferMap.at(inputBufferName).store(inputVec) && inputBufferMap.at(inputBufferName).run();
    }
}  // namespace Finn