#include "DeviceHandler.h"

#include <stdexcept>

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
        if (outputBufDescr.empty()) {
            throw std::invalid_argument("Empty output kernel list. Abort.");
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
        // std::transform(inputKernelDescr.begin(), inputKernelDescr.end(), std::back_inserter(inputKernels), [this](const BufferDescriptor& descr) { return xrt::kernel(device, uuid, descr.kernelName); });
        // std::transform(outputKernelDescr.begin(), outputKernelDescr.end(), std::back_inserter(outputKernels), [this](const BufferDescriptor& descr) { return xrt::kernel(device, uuid, descr.kernelName); });
        for (auto&& bufDesc : inputBufDescr) {
            auto tmpKern = xrt::kernel(device, uuid, bufDesc.kernelName);
            inputBuffer.emplace_back(Finn::DeviceInputBuffer<uint8_t>("buffer1", device, tmpKern, bufDesc.elementShape, static_cast<unsigned int>(hostBufferSize)));
        }
        for (auto&& bufDesc : outputBufDescr) {
            auto tmpKern = xrt::kernel(device, uuid, bufDesc.kernelName);
            outputBuffer.emplace_back(Finn::DeviceOutputBuffer<uint8_t>("buffer1", device, tmpKern, bufDesc.elementShape, static_cast<unsigned int>(hostBufferSize)));
        }
    }
}  // namespace Finn