#include "Accelerator.h"

#include <algorithm>  // for transform
#include <cstddef>    // for size_t
#include <iterator>   // for back_insert_iterator, back_inserter
#include <memory>     // for allocator_traits<>::value_type

#include "DeviceHandler.h"

namespace Finn {
    Accelerator::Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions) {
        // TODO(linusjun): Currently, the fpgaIndex is given in the config, to enable the user to assign the topology themselves. Okay like this? (-bwintermann)
        std::transform(deviceDefinitions.begin(), deviceDefinitions.end(), std::back_inserter(devices), [](const DeviceWrapper& dew) { return DeviceHandler(dew.xclbin, dew.name, dew.fpgaIndex, dew.idmas, dew.odmas); });
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
