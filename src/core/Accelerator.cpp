#include "Accelerator.h"

#include <algorithm>  // for transform
#include <cstddef>    // for size_t
#include <iterator>   // for back_insert_iterator, back_inserter
#include <memory>     // for allocator_traits<>::value_type

#include "DeviceHandler.h"
#include "../utils/ConfigurationStructs.h"
#include "ert.h"

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

    // FIXME: Shorten names!!!

    DeviceHandler& Accelerator::getDeviceHandlerByDeviceIndex(unsigned int deviceIndex) {
        if (!containsDeviceHandlerWithDeviceIndex(deviceIndex)) {
            FinnUtils::logAndError<std::runtime_error>("Tried retrieving a deviceHandler with an unknown index");
        }
        for (auto& dh : devices) {
            if (dh.getDeviceIndex() == deviceIndex) {
                return dh;
            }
        }
    }

    bool Accelerator::containsDeviceHandlerWithDeviceIndex(unsigned int deviceIndex) {
        return std::count_if(devices.begin(), devices.end(), [deviceIndex](DeviceHandler& dh) { return dh.getDeviceIndex() == deviceIndex; }) > 0;
    }

    // FIXME:
    // TODO(bwintermann): Make this either a factory or do the checks before calling store to save performance
    bool Accelerator::store(const std::vector<uint8_t>& data, const unsigned int deviceIndex, const std::string& inputBufferKernelName) {
        if (containsDeviceHandlerWithDeviceIndex(deviceIndex)) {
            return getDeviceHandlerByDeviceIndex(deviceIndex).store(data, inputBufferKernelName);
        } else {
            if (containsDeviceHandlerWithDeviceIndex(0)) {
                return getDeviceHandlerByDeviceIndex(0).store(data, inputBufferKernelName);
            } else {
                FinnUtils::logAndError<std::runtime_error>("Tried storing data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }

    bool Accelerator::run(const unsigned int deviceIndex, const std::string& inputBufferKernelName) {
        if (containsDeviceHandlerWithDeviceIndex(deviceIndex)) {
            return getDeviceHandlerByDeviceIndex(deviceIndex).run(inputBufferKernelName);
        } else {
            if (containsDeviceHandlerWithDeviceIndex(0)) {
                return getDeviceHandlerByDeviceIndex(0).run(inputBufferKernelName);
            } else {
                FinnUtils::logAndError<std::runtime_error>("Tried running data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }

    std::vector<std::vector<uint8_t>> Accelerator::retrieveResults(const unsigned int deviceIndex, const std::string& outputBufferKernelName) {
        if (containsDeviceHandlerWithDeviceIndex(deviceIndex)) {
            return getDeviceHandlerByDeviceIndex(deviceIndex).retrieveResults(outputBufferKernelName);
        } else {
            if (containsDeviceHandlerWithDeviceIndex(0)) {
                return getDeviceHandlerByDeviceIndex(0).retrieveResults(outputBufferKernelName);
            } else {
                FinnUtils::logAndError<std::runtime_error>("Tried receiving data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }


    ert_cmd_state Accelerator::read(const unsigned int deviceIndex, const std::string& outputBufferKernelName, unsigned int samples) {
        if (containsDeviceHandlerWithDeviceIndex(deviceIndex)) {
            return getDeviceHandlerByDeviceIndex(deviceIndex).read(outputBufferKernelName, samples);
        } else {
            if (containsDeviceHandlerWithDeviceIndex(0)) {
                return getDeviceHandlerByDeviceIndex(0).read(outputBufferKernelName, samples);
            } else {
                FinnUtils::logAndError<std::runtime_error>("Tried reading data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }


    size_t Accelerator::size(SIZE_SPECIFIER ss, unsigned int deviceIndex, const std::string& bufferName) {
        return getDeviceHandlerByDeviceIndex(deviceIndex).size(ss, bufferName);
    }

}  // namespace Finn
