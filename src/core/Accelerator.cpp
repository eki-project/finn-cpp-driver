#include "Accelerator.h"

#include <algorithm>  // for transform
#include <cstddef>    // for size_t
#include <iterator>   // for back_insert_iterator, back_inserter
#include <memory>     // for allocator_traits<>::value_type

#include "DeviceHandler.h"

namespace Finn {
    Accelerator::Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions, unsigned int hostBufferSize) {
        std::transform(
            deviceDefinitions.begin(),
            deviceDefinitions.end(),
            std::back_inserter(devices),
            [hostBufferSize](const DeviceWrapper& dew) { 
                return DeviceHandler(dew, hostBufferSize);
            }
        );
    }
}  // namespace Finn
