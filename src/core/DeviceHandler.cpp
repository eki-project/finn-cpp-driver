#include "DeviceHandler.h"

#include <stdexcept>

#include "../utils/Logger.h"

namespace fs = std::filesystem;

namespace Finn {
    DeviceHandler::DeviceHandler(const fs::path& xclbinPath, const std::string& pName, const std::size_t deviceIndex, const std::vector<std::string>& inputNames, const std::vector<std::string>& outputNames) : name(pName) {
        if (xclbinPath.empty()) {
            throw fs::filesystem_error("Empty filepath to xclbin. Abort.", std::error_code());
        }
        if (!fs::exists(xclbinPath) || !fs::is_regular_file(xclbinPath)) {
            throw fs::filesystem_error("File " + std::string(xclbinPath.c_str()) + " not found. Abort.", std::error_code());
        }
        if (pName.empty()) {
            throw std::invalid_argument("Empty device name. Abort.");
        }
        if (inputNames.empty()) {
            throw std::invalid_argument("Empty input kernel list. Abort.");
        }
        if (outputNames.empty()) {
            throw std::invalid_argument("Empty output kernel list. Abort.");
        }
        initializeDevice(deviceIndex, xclbinPath, inputNames, outputNames);
        initializeBufferObjects();
    }

    void DeviceHandler::initializeDevice(const std::size_t deviceIndex, const fs::path& xclbinPath, const std::vector<std::string>& inputKernelNames, const std::vector<std::string>& outputKernelNames) {
        auto log = Logger::getLogger();
        FINN_LOG(log, loglevel::info) << "(" << name << ") "
                                      << "Initializing xrt::device, loading xclbin and assigning IP\n";
        device = xrt::device(static_cast<unsigned int>(deviceIndex));
        uuid = device.load_xclbin(xclbinPath);
        std::transform(inputKernelNames.begin(), inputKernelNames.end(), std::back_inserter(inputKernels), [this](const std::string& kname) { return xrt::kernel(device, uuid, kname); });
        std::transform(outputKernelNames.begin(), outputKernelNames.end(), std::back_inserter(inputKernels), [this](const std::string& kname) { return xrt::kernel(device, uuid, kname); });
    }

    void DeviceHandler::initializeBufferObjects() const {
        auto log = Logger::getLogger();
        FINN_LOG_DEBUG(log, loglevel::info) << "(" << name << ") "
                                            << "Initializing buffer objects\n";
        // TODO(ljungemann) Implement
    }
}  // namespace Finn