#include "Accelerator.h"

#include <algorithm>

namespace Finn {
    Accelerator::Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions) {
        std::size_t deviceIndex = 0;  // For now just set deviceIndex to zero.
        std::transform(deviceDefinitions.begin(), deviceDefinitions.end(), std::back_inserter(devices), [this, &deviceIndex](const DeviceWrapper& dew) { return DeviceHandler(dew.xclbin, dew.name, deviceIndex, dew.idmas, dew.odmas); });
    }

    Accelerator::Accelerator(const DeviceWrapper& deviceWrapper) {
        std::size_t deviceIndex = 0;  // For now just set deviceIndex to zero.
        devices.emplace_back(DeviceHandler(deviceWrapper.xclbin, deviceWrapper.name, deviceIndex, deviceWrapper.idmas, deviceWrapper.odmas));
    }
}  // namespace Finn
