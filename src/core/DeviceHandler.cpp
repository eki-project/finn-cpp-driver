#include "DeviceHandler.h"

#include "../utils/Logger.h"


namespace Finn {
    DeviceHandler::DeviceHandler(const std::filesystem::path& xclbinPath, const std::string& name, const std::size_t deviceIndex, const std::vector<std::string>& inputNames, const std::vector<std::string>& outputNames) : name(name) {
        initializeDevice(deviceIndex, xclbinPath, inputNames, outputNames);
        initializeBufferObjects();
    }

    void DeviceHandler::initializeDevice(const std::size_t deviceIndex, const std::filesystem::path& xclbinPath, const std::vector<std::string>& inputKernelNames, const std::vector<std::string>& outputKernelNames) {
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