#include "Accelerator.h"

#include <algorithm>

#include "DeviceHandler.h"

namespace Finn {
    Accelerator::Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions) {
        std::size_t deviceIndex = 0;  // For now just set deviceIndex to zero.
        std::transform(deviceDefinitions.begin(), deviceDefinitions.end(), std::back_inserter(devices), [&deviceIndex](const DeviceWrapper& dew) { return DeviceHandler(dew.xclbin, dew.name, deviceIndex, dew.idmas, dew.odmas); });
    }

    Accelerator::Accelerator(const DeviceWrapper& deviceWrapper) {
        std::size_t deviceIndex = 0;  // For now just set deviceIndex to zero.
        devices.emplace_back(DeviceHandler(deviceWrapper.xclbin, deviceWrapper.name, deviceIndex, deviceWrapper.idmas, deviceWrapper.odmas));
    }

    bool Accelerator::write(const std::vector<uint8_t>& inputVec) {
        // TODO(linusjun) Write a real implementation with error checking and support for multiple fpgas and inputs
        return devices[0].write(inputVec);
    }
}  // namespace Finn
